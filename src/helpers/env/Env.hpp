#pragma once

#include <string>

namespace Env {
    bool envEnabled(const char* env);
    bool isTrace();

    // systemd is the running init (the sd_booted() test). Decided at runtime, because a build
    // with systemd support also runs on elogind systems without systemctl.
    bool systemdRunning();

    // exports the given variables to D-Bus activated services (and the systemd user manager),
    // or removes them there with import = false
    std::string activationEnvCommand(const std::string& vars, bool import = true);
}
