#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"
#include "../config/native/AbyssProtocols.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    sc<IWaylandProtocol*>(data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    SIWaylandProtocolDestroyWrapper* wrap  = wl_container_of(listener, wrap, listener);
    IWaylandProtocol*                proto = wrap->parent;
    proto->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    wl_list_remove(&m_liDisplayDestroy.listener.link);
    wl_list_init(&m_liDisplayDestroy.listener.link);
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
}

// Abysslang protocol selection (include/abyss/protocols.hpp): a protocol that is switched off in the config keeps its PROTO object (so no
// PROTO::xyz anywhere can be null) but never gets a wl_global, so no client can see or bind it. A version limit lowers the offered version.
static wl_global* createGlobalFiltered(IWaylandProtocol* self, const wl_interface* iface, int ver) {
    const int V = Config::Native::protocolGlobalVersion(iface, ver);
    if (V <= 0)
        return nullptr;
    return wl_global_create(g_pCompositor->m_wlDisplay, iface, V, self, &bindManagerInternal);
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) :
    m_name(name), m_global(createGlobalFiltered(this, iface, ver)) {

    // init before the early return: the destructor always calls onDisplayDestroy(), which removes this link
    wl_list_init(&m_liDisplayDestroy.listener.link);

    if UNLIKELY (!m_global) {
        LOGM(Log::DEBUG, "no global created for [{}] (disabled by the config or creation failed)", m_name);
        return;
    }

    m_liDisplayDestroy.listener.notify = displayDestroyInternal;
    m_liDisplayDestroy.parent          = this;
    wl_display_add_destroy_listener(g_pCompositor->m_wlDisplay, &m_liDisplayDestroy.listener);

    LOGM(Log::DEBUG, "Registered global [{}]", m_name);
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}

void IWaylandProtocol::removeGlobal() {
    if (m_global)
        wl_global_remove(m_global);
}

wl_global* IWaylandProtocol::getGlobal() {
    return m_global;
}
