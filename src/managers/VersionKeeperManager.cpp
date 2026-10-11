#include "VersionKeeperManager.hpp"
#include "../debug/log/Logger.hpp"
#include "../macros.hpp"
#include "../version.h"
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "eventLoop/EventLoopManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../helpers/fs/FsUtils.hpp"

#include <filesystem>
#include <fstream>
#include <abyssutils/string/String.hpp>
#include <abyssutils/string/Numeric.hpp>
#include <abyssutils/os/Process.hpp>

using namespace Abyssutils::String;
using namespace Abyssutils::OS;

constexpr const char* VERSION_FILE_NAME = "lastVersion";

CVersionKeeperManager::CVersionKeeperManager() {
    static auto PNONOTIFY = CConfigValue<Config::INTEGER>("ecosystem:no_update_news");

    const auto  DATAROOT = NFsUtils::getDataHome();

    if (!DATAROOT)
        return;

    auto LASTVER = NFsUtils::readFileAsString(*DATAROOT + "/" + VERSION_FILE_NAME);

    if (!LASTVER) {
        NFsUtils::writeToFile(*DATAROOT + "/" + VERSION_FILE_NAME, "0.0.0");
        LASTVER = "0.0.0";
        return;
    }

    if (!isMajorVersionOlderThanRunning(*LASTVER)) {
        Log::logger->log(Log::DEBUG, "CVersionKeeperManager: Read version {} matches or is older than running major.", *LASTVER);
        return;
    }

    NFsUtils::writeToFile(*DATAROOT + "/" + VERSION_FILE_NAME, ABYSSLAND_VERSION);

    if (*PNONOTIFY) {
        Log::logger->log(Log::DEBUG, "CVersionKeeperManager: updated, but update news is disabled in the config :(");
        return;
    }

    if (!NFsUtils::executableExistsInPath("abyssland-update-screen")) {
        Log::logger->log(Log::ERR, "CVersionKeeperManager: abyssland-update-screen doesn't seem to exist, skipping notif about update...");
        return;
    }

    m_fired = true;

    g_pEventLoopManager->doLater([]() {
        CProcess proc("abyssland-update-screen", {"--new-version", ABYSSLAND_VERSION});
        proc.runAsync();
    });
}

bool CVersionKeeperManager::isMajorVersionOlderThanRunning(const std::string& ver) {
    const CVarList        verStrings(ver, 0, '.', true);

    const int             V1 = strToNumber<uint32_t>(verStrings[0]).value_or(0);
    const int             V2 = strToNumber<uint32_t>(verStrings[1]).value_or(0);

    static const CVarList runningStrings(ABYSSLAND_VERSION, 0, '.', true);

    static const int      R1 = strToNumber<uint32_t>(runningStrings[0]).value_or(0);
    static const int      R2 = strToNumber<uint32_t>(runningStrings[1]).value_or(0);

    if (R1 > V1)
        return true;
    if (R2 > V2)
        return true;
    return false;
}

bool CVersionKeeperManager::fired() {
    return m_fired;
}
