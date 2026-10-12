#pragma once

// Internal side of include/abyss/protocols.hpp (not part of the public API).

#include <abyss/protocols.hpp>

struct wl_interface;

namespace Config::Native {
    // called by applyUserConfig after configure(): validates the choices (errors go to the config error list) and stores them
    void applyProtocols(const abyss::Protocols& p);

    // called for every wl_global the compositor is about to create (IWaylandProtocol): returns the version to offer, 0 = do not create
    // the global. Also records what is actually running, so a later reload can tell that a restart is needed.
    int protocolGlobalVersion(const wl_interface* iface, int builtinVersion);

    // text for `abyssctl protocols`
    std::string protocolsReport(bool json);
}
