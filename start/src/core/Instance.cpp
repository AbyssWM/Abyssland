#include "Instance.hpp"
#include "State.hpp"
#include "../helpers/Logger.hpp"
#include "../helpers/Nix.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ranges>
#include <string_view>

#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__FreeBSD__)
#include <signal.h>
#include <sys/procctl.h>
#endif

#include <abyssutils/os/Process.hpp>

using namespace Abyssutils::OS;
using namespace std::string_literals;

//
void CAbysslandInstance::runAbysslandThread(bool safeMode, bool lockedCrash) {
    std::vector<std::string> argsStd;
    argsStd.emplace_back("--watchdog-fd");
    argsStd.emplace_back(std::format("{}", m_toHlPid.get()));
    if (safeMode)
        argsStd.emplace_back("--safe-mode");

    if (lockedCrash)
        argsStd.emplace_back("--locked");

    for (const auto& a : g_state->rawArgvNoBinPath) {
        argsStd.emplace_back(a);
    }

    // spawn a process manually. Abyssutils' Async is detached, while Sync redirects stdout
    // TODO: make Sync respect fds?

    std::vector<char*> args = {strdup(g_state->customPath.value_or(ABYSSLAND_BINARY).c_str())};
    for (const auto& a : argsStd) {
        args.emplace_back(strdup(a.c_str()));
    }
    args.emplace_back(nullptr);

    int forkRet = fork();
    if (forkRet == 0) {
        // Make abyssland die on our SIGKILL
#if defined(__linux__)
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#elif defined(__FreeBSD__)
        int sig = SIGKILL;
        procctl(P_PID, getpid(), PROC_PDEATHSIG_CTL, &sig);
#endif

        if (Nix::shouldUseNixGL()) {
            argsStd.insert(argsStd.begin(), g_state->customPath.value_or(ABYSSLAND_BINARY));
            args.insert(args.begin(), strdup(argsStd.front().c_str()));
            execvp("nixGL", args.data());
        } else
            execvp(g_state->customPath.value_or(ABYSSLAND_BINARY).c_str(), args.data());

        g_logger->log(Abyssutils::CLI::LOG_ERR, "fork(): execvp failed: {}", strerror(errno));
        std::fflush(stdout);
        exit(1);
    } else
        m_hlPid = forkRet;

    m_hlThread = std::thread([this] {
        while (true) {
            int status = 0;
            int ret    = waitpid(m_hlPid, &status, 0);
            if (ret == -1) {
                g_logger->log(Abyssutils::CLI::LOG_ERR, "Couldn't waitpid for abyssland: {}", strerror(errno));
                break;
            }

            if (WIFEXITED(status))
                break;
        }

        if (write(m_wakeupWrite.get(), "vax", 3) < 0)
            g_logger->log(Abyssutils::CLI::LOG_ERR, "Failed to write to wakeup fd {}: {}", m_wakeupWrite.get(), strerror(errno));

        std::fflush(stdout);
        std::fflush(stderr);
    });
}

void CAbysslandInstance::forceQuit() {
    m_abysslandExiting = true;
    kill(m_hlPid, SIGTERM); // gracefully, can get stuck but it's unlikely

    m_hlThread.join(); // needs this otherwise can crash
}

void CAbysslandInstance::clearFd(const Abyssutils::OS::CFileDescriptor& fd) {
    if (!fd.isReadable()) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "Can't clear a unreadable fd");
        return;
    }

    static std::array<char, 1024> buf;
    if (read(fd.get(), buf.data(), 1023) < 0)
        g_logger->log(Abyssutils::CLI::LOG_ERR, "Failed clearing fd {}: {}", fd.get(), strerror(errno));
}

void CAbysslandInstance::dispatchAbysslandEvent() {
    std::string                   recvd = "";
    static std::array<char, 4096> buf;
    ssize_t                       n = read(m_fromHlPid.get(), buf.data(), 4096);
    if (n < 0) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "Failed dispatching hl events");
        return;
    }

    recvd.append(buf.data(), n);

    if (recvd.empty())
        return;

    for (const auto& s : std::views::split(recvd, '\n')) {
        const std::string_view sv = std::string_view{s};
        if (sv == "vax") {
            // init passed
            m_abysslandInitialized = true;
            continue;
        }

        if (sv == "end") {
            // exiting
            m_abysslandExiting = true;
            continue;
        }

        if (sv == "lock") {
            // session locked
            m_abysslandLocked = true;
            continue;
        }

        if (sv == "unlock") {
            // session unlocked
            m_abysslandLocked = false;
            continue;
        }
    }
}

bool CAbysslandInstance::run(bool safeMode, bool lockedCrash) {
    int pipefds[2];
    if (pipe(pipefds) != 0) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "pipe() failed, exiting");
        exit(1);
    }

    m_fromHlPid = CFileDescriptor{pipefds[0]};
    m_toHlPid   = CFileDescriptor{pipefds[1]};

    if (pipe(pipefds) != 0) {
        g_logger->log(Abyssutils::CLI::LOG_ERR, "pipe() failed, exiting");
        exit(1);
    }

    m_wakeupRead  = CFileDescriptor{pipefds[0]};
    m_wakeupWrite = CFileDescriptor{pipefds[1]};

    m_fromHlPid.setFlags(m_fromHlPid.getFlags() | FD_CLOEXEC);
    m_wakeupRead.setFlags(m_wakeupRead.getFlags() | FD_CLOEXEC);
    m_wakeupWrite.setFlags(m_wakeupWrite.getFlags() | FD_CLOEXEC);

    m_abysslandLocked = lockedCrash;

    runAbysslandThread(safeMode, lockedCrash);

    pollfd pollfds[2] = {
        {
            .fd      = m_wakeupRead.get(),
            .events  = POLLIN,
            .revents = 0,
        },
        {
            .fd      = m_fromHlPid.get(),
            .events  = POLLIN,
            .revents = 0,
        },
    };

    while (true) {
        int ret = poll(pollfds, 2, -1);

        if (ret < 0) {
            g_logger->log(Abyssutils::CLI::LOG_ERR, "poll() failed, exiting");
            exit(1);
        }

        if (pollfds[1].revents & POLLIN) {
            g_logger->log(Abyssutils::CLI::LOG_DEBUG, "got an event from abyssland");
            dispatchAbysslandEvent();
            continue;
        }

        if (pollfds[0].revents & POLLIN) {
            g_logger->log(Abyssutils::CLI::LOG_DEBUG, "abyssland exit, breaking poll, checking state");
            clearFd(m_wakeupRead);
            break;
        }
    }

    m_hlThread.join();

    return !m_abysslandInitialized || m_abysslandExiting;
}
