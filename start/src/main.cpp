#include <csignal>
#include <cstring>
#include <print>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "helpers/Logger.hpp"
#include "helpers/Nix.hpp"
#include "core/State.hpp"
#include "core/Instance.hpp"
#include "../../src/helpers/ExitCodes.hpp"

using namespace Abyssutils::CLI;

#define ASSERT(expr)                                                                                                                                                               \
    if (!(expr)) {                                                                                                                                                                 \
        g_logger->log(LOG_CRIT, "Failed assertion at line {} in {}: {} was false", __LINE__,                                                                                       \
                      ([]() constexpr -> std::string { return std::string(__FILE__).substr(std::string(__FILE__).find("/src/") + 1); })(), #expr);                                 \
        std::abort();                                                                                                                                                              \
    }

constexpr const char* HELP_INFO = R"#(start-abyssland - A binary to properly start Abyssland via a watchdog process.
Any arguments after -- are passed to Abyssland. For Abyssland help, run start-abyssland -- --help or Abyssland --help

Additional arguments for start-abyssland:
 --path [path]       -> Override Abyssland path
 --no-nixgl          -> Force disable nixGL
 --force-nixgl       -> Force enable nixGL
)#";

// Safety net for `abyssctl reload`: the binary that was installed before the last reload.
static bool differentFiles(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (std::filesystem::file_size(a, ec) != std::filesystem::file_size(b, ec) || ec)
        return true;
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    return !std::equal(std::istreambuf_iterator<char>(fa), {}, std::istreambuf_iterator<char>(fb));
}

static std::optional<std::string> findUsableLastGood() {
    const std::filesystem::path CURRENT = g_state->customPath.value_or(ABYSSLAND_BINARY);
    const std::filesystem::path LASTGOOD = CURRENT.parent_path() / "Abyssland.last-good";

    std::error_code ec;
    if (!std::filesystem::is_regular_file(LASTGOOD, ec) || ec)
        return std::nullopt;
    if (!differentFiles(CURRENT, LASTGOOD))
        return std::nullopt;
    return LASTGOOD.string();
}

constexpr auto FRESH_START_GRACE = std::chrono::seconds(5);

//
static void onSignal(int sig) {
    if (!g_instance)
        return;

    g_instance->forceQuit();
    g_instance.reset();

    exit(0);
}

static void terminateChildOnSignal(int signal) {
    struct sigaction sa;
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    int ret     = sigaction(signal, &sa, nullptr);
    if (ret != 0)
        g_logger->log(Abyssutils::CLI::LOG_WARN, "Failed to set up handler for signal {}: {}", signal, strerror(errno));
}

int main(int argc, const char** argv, const char** envp) {
    g_logger = makeUnique<Abyssutils::CLI::CLoggerConnection>(*g_loggerMain);
    g_logger->setName("start-abyssland");
    g_logger->setLogLevel(Abyssutils::CLI::LOG_DEBUG);

    terminateChildOnSignal(SIGTERM);
    terminateChildOnSignal(SIGINT);

    int startArgv = -1;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--") {
            startArgv = i + 1;
            break;
        }
        if (arg == "-h" || arg == "--help") {
            std::println("{}", HELP_INFO);
            return 0;
        }
        if (arg == "--path" || arg == "-p") {
            if (i + 1 >= argc) {
                std::println("{} requires a path", arg);
                return 1;
            }

            g_state->customPath = argv[++i];
            continue;
        }
        if (arg == "--no-nixgl") {
            g_state->noNixGl = true;
            continue;
        }
        if (arg == "--force-nixgl") {
            g_state->forceNixGl = true;
            continue;
        }
    }

    if (startArgv != -1)
        g_state->rawArgvNoBinPath = std::span<const char*>{argv + startArgv, argc - startArgv};

    if (!g_state->rawArgvNoBinPath.empty())
        g_logger->log(Abyssutils::CLI::LOG_WARN, "Arguments after -- are passed to Abyssland");

    // check if our environment is OK
    if (const auto RET = Nix::nixEnvironmentOk(); !RET) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "Nix environment check failed:\n{}", RET.error());
        return 1;
    }

    if (Nix::shouldUseNixGL())
        g_logger->log(Abyssutils::CLI::LOG_DEBUG, "Abyssland was compiled with Nix - will use nixGL");

    bool safeMode        = false;
    bool lockedCrash     = false;
    bool freshRestart    = false; // the next child was started after an `abyssctl restart`
    bool useLastGood     = false;
    std::optional<std::string> binaryOverride;

    while (true) {
        g_instance                   = makeUnique<CAbysslandInstance>();
        g_instance->m_binaryOverride = binaryOverride;

        const auto START = std::chrono::steady_clock::now();
        const bool RET   = g_instance->run(safeMode, lockedCrash);
        const auto ALIVE = std::chrono::steady_clock::now() - START;
        const int  CODE  = g_instance->m_exitCode;
        lockedCrash      = g_instance->m_abysslandLocked;
        g_instance.reset();

        if (CODE == ABYSSLAND_EXIT_RESTART) {
            g_logger->log(Abyssutils::CLI::LOG_DEBUG, "Abyssland requested a restart, relaunching.");
            safeMode       = false;
            freshRestart   = true;
            useLastGood    = false;
            binaryOverride = std::nullopt;
            continue;
        }

        // a freshly restarted (new) binary died right away: fall back to the last known good one, once
        if (freshRestart && !useLastGood && CODE != 0 && ALIVE < FRESH_START_GRACE) {
            if (const auto LASTGOOD = findUsableLastGood()) {
                g_logger->log(Abyssutils::CLI::LOG_WARN,
                              "The new Abyssland binary died {}ms after starting (exit code {}). Falling back to last-good binary {}",
                              std::chrono::duration_cast<std::chrono::milliseconds>(ALIVE).count(), CODE, *LASTGOOD);
                useLastGood    = true;
                binaryOverride = *LASTGOOD;
                continue;
            }
        }
        freshRestart = false;

        if (!RET) {
            g_logger->log(Abyssutils::CLI::LOG_ERR, "Abyssland exit not-cleanly, restarting");
            safeMode = true;
            continue;
        }

        g_logger->log(Abyssutils::CLI::LOG_DEBUG, "Abyssland exit cleanly.");
        break;
    }

    return 0;
}
