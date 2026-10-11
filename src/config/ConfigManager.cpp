#include "ConfigManager.hpp"
#include "native/ConfigManager.hpp"
#include "../debug/log/Logger.hpp"

using namespace Config;

static UP<IConfigManager> g_mgr;

//
bool Config::initConfigManager() {
    if (mgr())
        return true;

    g_mgr = makeUnique<Native::CConfigManager>();

    RASSERT(g_mgr, "failed to create the config manager");

    return true;
}

UP<IConfigManager>& Config::mgr() {
    return g_mgr;
}
