#include "Env.hpp"

#include <cstdlib>
#include <filesystem>

bool Env::envEnabled(const char* env) {
    const auto ret = getenv(env);
    return ret && ret[0] != '\0' && !(ret[0] == '0' && ret[1] == '\0');
}

bool Env::isTrace() {
    static bool TRACE = envEnabled("ABYSSLAND_TRACE");
    return TRACE;
}

bool Env::systemdRunning() {
    static const bool RUNNING = [] {
        std::error_code ec;
        return std::filesystem::is_directory("/run/systemd/system", ec);
    }();
    return RUNNING;
}

std::string Env::activationEnvCommand(const std::string& vars, bool import) {
    // without systemd there is no systemctl and no --systemd target, and a failing systemctl
    // would stop the && chain before D-Bus gets the variables (portals then miss XDG_CURRENT_DESKTOP)
    if (!systemdRunning())
        return "dbus-update-activation-environment " + vars;
    return std::string{"systemctl --user "} + (import ? "import-environment " : "unset-environment ") + vars +
        " && hash dbus-update-activation-environment 2>/dev/null && dbus-update-activation-environment --systemd " + vars;
}
