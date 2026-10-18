#include "SessionServices.hpp"
#include "XdgAutostart.hpp"
#include "env/Env.hpp"
#include "memory/Memory.hpp"
#include <abyssutils/memory/Casts.hpp>
#include "../debug/log/Logger.hpp"
#include "../Compositor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// known polkit agents, by program name
static constexpr std::array POLKIT_AGENTS = {
    std::string_view{"abyss-polkit-agent"},
    std::string_view{"polkit-gnome-authentication-agent-1"},
    std::string_view{"polkit-kde-authentication-agent-1"},
    std::string_view{"polkit-mate-authentication-agent-1"},
    std::string_view{"lxpolkit"},
    std::string_view{"lxqt-policykit-agent"},
    std::string_view{"hyprpolkitagent"},
    std::string_view{"xfce-polkit"},
};

static constexpr std::array ABYSS_POLKIT_AGENT_PATHS = {
    std::string_view{"/usr/libexec/abyss-polkit-agent"},
    std::string_view{"/usr/lib/abyss-polkit-agent"},
};

// managed by startAudio(), so autostart entries for them are skipped
static constexpr std::array AUDIO_PROGRAMS = {
    std::string_view{"pipewire"},
    std::string_view{"wireplumber"},
    std::string_view{"pipewire-pulse"},
};

struct SProcess {
    pid_t       pid = 0;
    std::string name; // basename of argv[0]
};

// processes of the current user, named by the basename of argv[0] (comm is cut at 15 characters)
static std::vector<SProcess> userProcesses() {
    std::vector<SProcess> result;
    const auto            UID = getuid();
    std::error_code       ec;
    for (const auto& entry : fs::directory_iterator("/proc", ec)) {
        const auto PIDSTR = entry.path().filename().string();
        if (PIDSTR.empty() || !std::ranges::all_of(PIDSTR, ::isdigit))
            continue;

        struct stat st = {};
        if (stat(entry.path().c_str(), &st) != 0 || st.st_uid != UID)
            continue;

        std::ifstream cmdline(entry.path() / "cmdline");
        std::string   argv0;
        if (!std::getline(cmdline, argv0, '\0') || argv0.empty())
            continue;

        result.push_back({.pid = sc<pid_t>(std::stoi(PIDSTR)), .name = fs::path{argv0}.filename().string()});
    }
    return result;
}

static bool isRunning(std::string_view name) {
    return std::ranges::any_of(userProcesses(), [name](const SProcess& p) { return p.name == name; });
}

static std::vector<pid_t> processesNamed(const std::vector<std::string_view>& names) {
    std::vector<pid_t> pids;
    for (const auto& p : userProcesses()) {
        if (std::ranges::find(names, std::string_view{p.name}) == names.end())
            continue;
        pids.push_back(p.pid);
    }
    return pids;
}

// SIGTERM, then SIGKILL for whatever is still there after 2 seconds
static void stopAll(const std::vector<std::string_view>& names) {
    const auto PIDS = processesNamed(names);
    if (PIDS.empty())
        return;

    Log::logger->log(Log::DEBUG, "[session] stopping {} leftover audio process(es)", PIDS.size());
    for (const auto PID : PIDS)
        kill(PID, SIGTERM);

    for (int i = 0; i < 20 && !processesNamed(names).empty(); ++i)
        std::this_thread::sleep_for(100ms);

    for (const auto PID : processesNamed(names))
        kill(PID, SIGKILL);
}

static std::optional<std::string> findInPath(std::string_view program) {
    if (program.contains('/'))
        return access(std::string{program}.c_str(), X_OK) == 0 ? std::optional<std::string>{program} : std::nullopt;

    const auto PATH = getenv("PATH");
    if (!PATH)
        return std::nullopt;

    std::string_view rest{PATH};
    while (!rest.empty()) {
        const auto COLON = rest.find(':');
        const auto DIR   = rest.substr(0, COLON);
        rest             = COLON == std::string_view::npos ? std::string_view{} : rest.substr(COLON + 1);
        if (DIR.empty())
            continue;

        const auto CANDIDATE = fs::path{DIR} / program;
        if (access(CANDIDATE.c_str(), X_OK) == 0)
            return CANDIDATE.string();
    }
    return std::nullopt;
}

// detached from the compositor (double fork + setsid); like the executor's spawns with the
// original file limit and the output going to /dev/null
static void spawn(const std::vector<std::string>& argv) {
    if (argv.empty())
        return;

    Log::logger->log(Log::DEBUG, "[session] starting {}", argv.back());

    // built before fork: only async-signal-safe calls between fork and exec
    std::vector<char*> cargv;
    for (const auto& a : argv)
        cargv.push_back(cc<char*>(a.c_str()));
    cargv.push_back(nullptr);

    const pid_t CHILD = fork();
    if (CHILD < 0)
        return;

    if (CHILD == 0) {
        setsid();
        g_pCompositor->restoreNofile();

        sigset_t set;
        sigemptyset(&set);
        sigprocmask(SIG_SETMASK, &set, nullptr);

        const int DEVNULL = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (DEVNULL != -1) {
            dup2(DEVNULL, STDOUT_FILENO);
            dup2(DEVNULL, STDERR_FILENO);
            close(DEVNULL);
        }

        if (fork() == 0) {
            execvp(cargv[0], cargv.data());
            _exit(127);
        }
        _exit(0);
    }

    waitpid(CHILD, nullptr, 0);
}

// without systemd: PipeWire fresh for this session, WirePlumber and the PulseAudio bridge if the
// PipeWire config does not start them
static void startAudio() {
    const auto PIPEWIRE = findInPath("pipewire");
    if (!PIPEWIRE)
        return;

    stopAll({AUDIO_PROGRAMS.begin(), AUDIO_PROGRAMS.end()});
    spawn({*PIPEWIRE});

    std::this_thread::sleep_for(1500ms);

    if (!isRunning("wireplumber"))
        if (const auto WIREPLUMBER = findInPath("wireplumber"))
            spawn({*WIREPLUMBER});

    std::error_code ec;
    const auto      RUNTIME    = getenv("XDG_RUNTIME_DIR");
    const bool      PULSEREADY = RUNTIME && fs::exists(fs::path{RUNTIME} / "pulse" / "native", ec);
    if (!PULSEREADY)
        if (const auto PULSE = findInPath("pipewire-pulse"))
            spawn({*PULSE});
}

// without systemd: the XDG autostart entries meant for this desktop
static void startAutostart() {
    const auto DESKTOPS = getenv("XDG_CURRENT_DESKTOP");
    for (const auto& [name, entry] : XdgAutostart::collect(XdgAutostart::directories())) {
        if (!XdgAutostart::wanted(entry) || !XdgAutostart::shownIn(entry, DESKTOPS ? DESKTOPS : ""))
            continue;

        if (!entry.tryExec.empty() && !findInPath(entry.tryExec))
            continue;

        if (std::ranges::find(AUDIO_PROGRAMS, std::string_view{XdgAutostart::program(entry.exec)}) != AUDIO_PROGRAMS.end())
            continue;

        Log::logger->log(Log::DEBUG, "[session] autostart {}", name);
        spawn({"/bin/sh", "-c", XdgAutostart::stripFieldCodes(entry.exec)});
    }
}

// a polkit agent on every system, if none runs a moment after the start (the session target,
// autostart or the user's config may have started one)
static void startPolkitAgent() {
    std::this_thread::sleep_for(2s);

    const auto PROCESSES = userProcesses();
    if (std::ranges::any_of(PROCESSES, [](const SProcess& p) { return std::ranges::find(POLKIT_AGENTS, std::string_view{p.name}) != POLKIT_AGENTS.end(); }))
        return;

    // through systemd, which never starts the unit twice
    if (Env::systemdRunning()) {
        spawn({"systemctl", "--user", "start", "abyss-polkit-agent.service"});
        return;
    }

    for (const auto PATH : ABYSS_POLKIT_AGENT_PATHS) {
        if (access(std::string{PATH}.c_str(), X_OK) != 0)
            continue;
        spawn({std::string{PATH}});
        return;
    }
    Log::logger->log(Log::DEBUG, "[session] no polkit agent running and abyss-polkit-agent is not installed");
}

void SessionServices::start() {
    std::thread([] {
        // with systemd, the user units and xdg-desktop-autostart.target do this
        if (!Env::systemdRunning()) {
            startAudio();
            startAutostart();
        }
        startPolkitAgent();
    }).detach();
}
