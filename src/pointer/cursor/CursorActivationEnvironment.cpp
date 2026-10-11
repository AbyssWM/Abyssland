#include "CursorActivationEnvironment.hpp"

#include <array>
#include <utility>

using namespace Pointer::Cursor;

CActivationEnvironment::CActivationEnvironment(std::string address) : m_address(std::move(address)), m_cancel(g_cancellable_new(), g_object_unref), m_worker([this] { run(); }) {
    ;
}

CActivationEnvironment::~CActivationEnvironment() {
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
        m_pending.reset();
    }
    g_cancellable_cancel(m_cancel.get());
    m_condition.notify_one();
    m_worker.join();
}

void CActivationEnvironment::update(SActivationEnvironment environment) {
    {
        std::lock_guard lock(m_mutex);
        // Coalesce pending changes, keeping calls in order even if a previous export is slow.
        m_pending = std::move(environment);
    }
    m_condition.notify_one();
}

std::vector<std::string> CActivationEnvironment::takeErrors() {
    std::lock_guard lock(m_mutex);
    return std::exchange(m_errors, {});
}

bool CActivationEnvironment::busy() {
    std::lock_guard lock(m_mutex);
    return m_exporting || m_pending.has_value();
}

void CActivationEnvironment::run() {
    std::unique_lock lock(m_mutex);
    while (true) {
        m_condition.wait(lock, [this] { return m_stopping || m_pending.has_value(); });
        if (m_stopping)
            return;

        auto environment = std::move(*m_pending);
        m_pending.reset();
        m_exporting = true;
        lock.unlock();
        auto errors = exportEnvironment(environment);
        lock.lock();
        m_errors    = std::move(errors);
        m_exporting = false;
    }
}

std::vector<std::string> CActivationEnvironment::exportEnvironment(const SActivationEnvironment& environment) {
    std::vector<std::string> errors;
    GError*                  rawError = nullptr;
    const auto               flags    = static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION);
    std::unique_ptr<GDBusConnection, decltype(&g_object_unref)> connection(g_dbus_connection_new_for_address_sync(m_address.c_str(), flags, nullptr, m_cancel.get(), &rawError),
                                                                           g_object_unref);
    std::unique_ptr<GError, decltype(&g_error_free)>            error(rawError, g_error_free);
    if (!connection) {
        errors.emplace_back("Could not connect to the session bus for cursor activation environment");
        return errors;
    }
    g_dbus_connection_set_exit_on_close(connection.get(), false);

    const auto                                               size   = std::to_string(environment.size);
    const std::array<std::pair<const char*, std::string>, 4> values = {{
        {"XCURSOR_THEME", environment.clientTheme},
        {"XCURSOR_SIZE", size},
        {"ABYSSCURSOR_THEME", environment.nativeTheme},
        {"ABYSSCURSOR_SIZE", size},
    }};
    GVariantBuilder                                          builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{ss}"));
    for (const auto& [name, value] : values)
        g_variant_builder_add(&builder, "{ss}", name, value.c_str());

    rawError = nullptr;
    std::unique_ptr<GVariant, decltype(&g_variant_unref)> result(
        g_dbus_connection_call_sync(connection.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "UpdateActivationEnvironment",
                                    g_variant_new("(a{ss})", &builder), G_VARIANT_TYPE_UNIT, G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, m_cancel.get(), &rawError),
        g_variant_unref);
    error.reset(rawError);
    if (!result)
        errors.emplace_back("D-Bus rejected the cursor activation environment update");

    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (const auto& [name, value] : values)
        g_variant_builder_add(&builder, "s", (std::string{name} + "=" + value).c_str());

    rawError = nullptr;
    result.reset(g_dbus_connection_call_sync(connection.get(), "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "SetEnvironment",
                                             g_variant_new("(as)", &builder), G_VARIANT_TYPE_UNIT, G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, m_cancel.get(), &rawError));
    error.reset(rawError);
    if (!result && !g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER) && !g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
        errors.emplace_back("The systemd user manager rejected the cursor activation environment update");

    return errors;
}
