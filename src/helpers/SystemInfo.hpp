#pragma once

#include <string>

#include "../SharedDefs.hpp"

namespace Helpers::SystemInfo {
    std::string getSystemInfo();
    std::string getVersion(eAbyssCtlOutputFormat fmt);
    std::string getStatus(eAbyssCtlOutputFormat fmt);
};