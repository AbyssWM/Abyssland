#pragma once

#include <abyssutils/os/FileDescriptor.hpp>
#include <thread>
#include <optional>
#include <string>

#include "../helpers/Memory.hpp"

class CAbysslandInstance {
  public:
    CAbysslandInstance()  = default;
    ~CAbysslandInstance() = default;

    CAbysslandInstance(const CAbysslandInstance&) = delete;
    CAbysslandInstance(CAbysslandInstance&)       = delete;
    CAbysslandInstance(CAbysslandInstance&&)      = delete;

    bool run(bool safeMode = false, bool lockedCrash = false); // if returns false, restart.
    void forceQuit();
    bool initialized() const;

    bool m_abysslandLocked  = false;
    bool m_restartRequested = false;

    // binary to launch instead of the default / --path one (used for last-good fallback)
    std::optional<std::string> m_binaryOverride;

    // process exit code of the last run, 128+sig if killed by a signal, -1 if unknown
    int m_exitCode = -1;

  private:
    void                           runAbysslandThread(bool safeMode, bool lockedCrash);
    void                           clearFd(const Abyssutils::OS::CFileDescriptor& fd);
    void                           dispatchAbysslandEvent();

    int                            m_hlPid = -1;

    Abyssutils::OS::CFileDescriptor m_fromHlPid, m_toHlPid;
    Abyssutils::OS::CFileDescriptor m_wakeupRead, m_wakeupWrite;

    bool                           m_abysslandInitialized = false;
    bool                           m_abysslandExiting     = false;

    std::thread                    m_hlThread;
};

inline UP<CAbysslandInstance> g_instance;
