#pragma once

#include <abyssutils/cli/Logger.hpp>

#include "Memory.hpp"

// we do this to add a from start-abyssland to the logs
inline UP<Abyssutils::CLI::CLogger>           g_loggerMain = makeUnique<Abyssutils::CLI::CLogger>();
inline UP<Abyssutils::CLI::CLoggerConnection> g_logger;
