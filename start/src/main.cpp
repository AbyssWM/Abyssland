#include <csignal>
#include <cstring>
#include <print>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <array>
#include <thread>

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

// Portals, notifications and the polkit agent need a D-Bus session bus, and D-Bus starts the
// portal on demand (as with Hyprland). Started from a TTY there may be none: use the user bus if
// the system runs one, otherwise run again under dbus-run-session (what abyssland-dbus.desktop does).
static void ensureDbusSession(int argc, const char** argv) {
    if (const auto ADDR = getenv("DBUS_SESSION_BUS_ADDRESS"); ADDR && *ADDR)
        return;

    if (const auto RUNTIME = getenv("XDG_RUNTIME_DIR"); RUNTIME && *RUNTIME) {
        const std::string BUS = std::string{RUNTIME} + "/bus";
        struct stat       st;
        if (stat(BUS.c_str(), &st) == 0 && S_ISSOCK(st.st_mode)) {
            setenv("DBUS_SESSION_BUS_ADDRESS", ("unix:path=" + BUS).c_str(), 1);
            g_logger->log(Abyssutils::CLI::LOG_DEBUG, "Using the user D-Bus at {}", BUS);
            return;
        }
    }

    // dbus-run-session always sets the address, the guard only stops a loop if it does not
    if (getenv("ABYSSLAND_DBUS_SESSION_STARTED"))
        return;
    setenv("ABYSSLAND_DBUS_SESSION_STARTED", "1", 1);

    std::error_code ec;
    const auto      SELF = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec)
        return;

    std::vector<const char*> args = {"dbus-run-session", "--", SELF.c_str()};
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    args.emplace_back(nullptr);

    g_logger->log(Abyssutils::CLI::LOG_DEBUG, "No D-Bus session bus, restarting under dbus-run-session");
    execvp("dbus-run-session", const_cast<char* const*>(args.data()));
    g_logger->log(Abyssutils::CLI::LOG_WARN, "No D-Bus session bus and dbus-run-session failed ({}): portals and notifications will not work", strerror(errno));
}

// The TTY output is gone once the session runs, and a compositor that dies before its own log
// exists only writes to stderr. Everything printed by start-abyssland and the compositor (which
// inherits stdout and stderr) still goes to the TTY and is also written, unbuffered, to
// $XDG_RUNTIME_DIR/abyss/start-abyssland.log (appended, one header per start).
static void teeOutputToLog() {
    const auto RUNTIME = getenv("XDG_RUNTIME_DIR");
    if (!RUNTIME || !*RUNTIME)
        return;

    const std::string DIR = std::string{RUNTIME} + "/abyss";
    std::error_code   ec;
    std::filesystem::create_directories(DIR, ec);

    // appended, so a failed restart is still there after the session was started again by hand
    const int LOG = open((DIR + "/start-abyssland.log").c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (LOG < 0)
        return;

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        close(LOG);
        return;
    }

    const auto STAMP = std::format("\n===== start-abyssland {} (pid {}) =====\n", std::chrono::system_clock::now(), getpid());
    [[maybe_unused]] const auto HEADER = write(LOG, STAMP.data(), STAMP.size());

    const int TTY = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
    std::fflush(stdout);
    std::fflush(stderr);
    // dup2 clears close-on-exec: the compositor writes into the same pipe
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);

    std::thread([in = fds[0], TTY, LOG] {
        std::array<char, 4096> buf = {};
        ssize_t                n   = 0;
        while ((n = read(in, buf.data(), buf.size())) > 0) {
            // a failed write (the TTY may be gone) only loses that copy
            [[maybe_unused]] const auto TOTTY  = TTY >= 0 ? write(TTY, buf.data(), n) : 0;
            [[maybe_unused]] const auto TOFILE = write(LOG, buf.data(), n);
        }
    }).detach();
}

int main(int argc, const char** argv, const char** envp) {
    g_logger = makeUnique<Abyssutils::CLI::CLoggerConnection>(*g_loggerMain);
    g_logger->setName("start-abyssland");
    g_logger->setLogLevel(Abyssutils::CLI::LOG_DEBUG);

    g_loggerMain->setTime(true);

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

    ensureDbusSession(argc, argv);
    // after a possible re-exec under dbus-run-session, which would end the copying thread
    teeOutputToLog();

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

        g_logger->log(Abyssutils::CLI::LOG_DEBUG, "Abyssland exited with code {} after {}ms", CODE,
                      std::chrono::duration_cast<std::chrono::milliseconds>(ALIVE).count());

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
