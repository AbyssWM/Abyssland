#pragma once

#include <abyssutils/cli/Logger.hpp>
#include <utility>

#include "../../helpers/memory/Memory.hpp"
#include "../../helpers/env/Env.hpp"

namespace Log {
    class CLogger {
      public:
        CLogger();
        ~CLogger() = default;

        void initIS(const std::string_view& IS);
        void initCallbacks();

        void log(Abyssutils::CLI::eLogLevel level, const std::string_view& str);

        template <typename... Args>
        //NOLINTNEXTLINE
        void log(Abyssutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
            if (!m_logsEnabled)
                return;

            if (level == Abyssutils::CLI::LOG_TRACE && !m_isTrace)
                return;

            std::string logMsg = "";
            logMsg += std::format(fmt, std::forward<Args>(args)...);

            log(level, logMsg);
        }

        const std::string&       rolling();
        Abyssutils::CLI::CLogger& hu();

      private:
        void                    recheckCfg();

        Abyssutils::CLI::CLogger m_logger;
        bool                    m_logsEnabled = true;
        bool                    m_isTrace     = false;
    };

    inline UP<CLogger> logger = makeUnique<CLogger>();

    //
    inline constexpr const Abyssutils::CLI::eLogLevel DEBUG = Abyssutils::CLI::LOG_DEBUG;
    inline constexpr const Abyssutils::CLI::eLogLevel WARN  = Abyssutils::CLI::LOG_WARN;
    inline constexpr const Abyssutils::CLI::eLogLevel ERR   = Abyssutils::CLI::LOG_ERR;
    inline constexpr const Abyssutils::CLI::eLogLevel CRIT  = Abyssutils::CLI::LOG_CRIT;
    inline constexpr const Abyssutils::CLI::eLogLevel INFO  = Abyssutils::CLI::LOG_DEBUG;
    inline constexpr const Abyssutils::CLI::eLogLevel TRACE = Abyssutils::CLI::LOG_TRACE;
};
