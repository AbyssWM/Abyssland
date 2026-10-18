#include "SessionServices.hpp"
#include "../debug/log/Logger.hpp"

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

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {
    // known polkit agents, by program name
    constexpr std::array POLKIT_AGENTS = {
        std::string_view{"abyss-polkit-agent"},
        std::string_view{"polkit-gnome-authentication-agent-1"},
        std::string_view{"polkit-kde-authentication-agent-1"},
        std::string_view{"polkit-mate-authentication-agent-1"},
        std::string_view{"lxpolkit"},
        std::string_view{"lxqt-policykit-agent"},
        std::string_view{"hyprpolkitagent"},
        std::string_view{"xfce-polkit"},
    };

    constexpr std::array ABYSS_POLKIT_AGENT_PATHS = {
        std::string_view{"/usr/libexec/abyss-polkit-agent"},
        std::string_view{"/usr/lib/abyss-polkit-agent"},
    };

    struct SProcess {
        pid_t       pid = 0;
        std::string name; // basename of argv[0]
    };

    // processes of the current user, named by the basename of argv[0] (comm is cut at 15 characters)
    std::vector<SProcess> userProcesses() {
        std::vector<SProcess> result;
        const auto            UID = getuid();
        std::error_code       ec;
        for (const auto& entry : fs::directory_iterator("/proc", ec)) {
            const auto PIDSTR = entry.path().filename().string();
            if (PIDSTR.empty() || !std::ranges::all_of(PIDSTR, ::isdigit))
                continue;

            struct stat st;
            if (stat(entry.path().c_str(), &st) != 0 || st.st_uid != UID)
                continue;

            std::ifstream cmdline(entry.path() / "cmdline");
            std::string   argv0;
            if (!std::getline(cmdline, argv0, '\0') || argv0.empty())
                continue;

            result.push_back({.pid = static_cast<pid_t>(std::stoi(PIDSTR)), .name = fs::path{argv0}.filename().string()});
        }
        return result;
    }

    bool isRunning(std::string_view name) {
        return std::ranges::any_of(userProcesses(), [name](const SProcess& p) { return p.name == name; });
    }

    // SIGTERM, then SIGKILL for whatever is still there after 2 seconds
    void stopAll(const std::vector<std::string_view>& names) {
        auto matching = [&names] {
            std::vector<pid_t> pids;
            for (const auto& p : userProcesses())
                if (std::ranges::find(names, std::string_view{p.name}) != names.end())
                    pids.push_back(p.pid);
            return pids;
        };

        auto pids = matching();
        if (pids.empty())
            return;

        Log::logger->log(Log::DEBUG, "[session] stopping {} leftover audio process(es)", pids.size());
        for (const auto PID : pids)
            kill(PID, SIGTERM);

        for (int i = 0; i < 20 && !matching().empty(); ++i)
            std::this_thread::sleep_for(100ms);

        for (const auto PID : matching())
            kill(PID, SIGKILL);
    }

    std::optional<std::string> findInPath(std::string_view program) {
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

    // detached from the compositor (double fork + setsid), like the executor's spawns
    void spawn(const std::string& path) {
        Log::logger->log(Log::DEBUG, "[session] starting {}", path);

        const pid_t CHILD = fork();
        if (CHILD < 0)
            return;

        if (CHILD == 0) {
            setsid();
            sigset_t set;
            sigemptyset(&set);
            sigprocmask(SIG_SETMASK, &set, nullptr);

            if (fork() == 0) {
                execl(path.c_str(), path.c_str(), nullptr);
                _exit(127);
            }
            _exit(0);
        }

        waitpid(CHILD, nullptr, 0);
    }

    void startAudio() {
        // with systemd the user units start PipeWire on demand
        std::error_code ec;
        if (fs::exists("/run/systemd/system", ec))
            return;

        const auto PIPEWIRE = findInPath("pipewire");
        if (!PIPEWIRE)
            return;

        stopAll({"wireplumber", "pipewire-pulse", "pipewire"});
        spawn(*PIPEWIRE);

        // PipeWire configs often start WirePlumber and the PulseAudio bridge themselves
        std::this_thread::sleep_for(1500ms);

        if (!isRunning("wireplumber"))
            if (const auto WIREPLUMBER = findInPath("wireplumber"))
                spawn(*WIREPLUMBER);

        const auto RUNTIME     = getenv("XDG_RUNTIME_DIR");
        const bool PULSEREADY  = RUNTIME && fs::exists(fs::path{RUNTIME} / "pulse" / "native", ec);
        if (!PULSEREADY)
            if (const auto PULSE = findInPath("pipewire-pulse"))
                spawn(*PULSE);
    }

    void startPolkitAgent() {
        // the system or the user's config may start one
        std::this_thread::sleep_for(2s);

        const auto PROCESSES = userProcesses();
        if (std::ranges::any_of(PROCESSES, [](const SProcess& p) { return std::ranges::find(POLKIT_AGENTS, std::string_view{p.name}) != POLKIT_AGENTS.end(); }))
            return;

        for (const auto PATH : ABYSS_POLKIT_AGENT_PATHS) {
            if (access(std::string{PATH}.c_str(), X_OK) == 0) {
                spawn(std::string{PATH});
                return;
            }
        }
        Log::logger->log(Log::DEBUG, "[session] no polkit agent running and abyss-polkit-agent is not installed");
    }
}

void SessionServices::start() {
    std::thread([] {
        startAudio();
        startPolkitAgent();
    }).detach();
}
