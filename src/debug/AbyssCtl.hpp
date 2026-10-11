#pragma once

#include <fstream>
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/defer/Promise.hpp"
#include "../desktop/view/Window.hpp"
#include "../desktop/DesktopTypes.hpp"
#include <functional>
#include <sys/types.h>
#include <abyssutils/os/FileDescriptor.hpp>

// exposed for main.cpp
std::string systemInfoRequest(eAbyssCtlOutputFormat format, std::string request);
std::string versionRequest(eAbyssCtlOutputFormat format, std::string request);

class CAbyssCtl {
  public:
    CAbyssCtl();
    ~CAbyssCtl();

    std::string                    makeDynamicCall(const std::string& input);
    SP<SAbyssCtlCommand>            registerCommand(SAbyssCtlCommand cmd);
    void                           unregisterCommand(const SP<SAbyssCtlCommand>& cmd);
    std::string                    getReply(std::string);

    Abyssutils::OS::CFileDescriptor m_socketFD;

    struct {
        bool                      all              = false;
        bool                      sysInfoConfig    = false;
        bool                      isDynamicKeyword = false;
        pid_t                     pid              = 0;
        SP<CPromise<std::string>> pendingPromise;
    } m_currentRequestParams;

    static std::string getWindowData(PHLWINDOW w, eAbyssCtlOutputFormat format);
    static std::string getWorkspaceData(PHLWORKSPACE w, eAbyssCtlOutputFormat format);
    static std::string getSolitaryBlockedReason(PHLMONITOR m, eAbyssCtlOutputFormat format);
    static std::string getDSBlockedReason(PHLMONITOR m, eAbyssCtlOutputFormat format);
    static std::string getTearingBlockedReason(PHLMONITOR m, eAbyssCtlOutputFormat format);
    static std::string getMonitorData(PHLMONITOR m, eAbyssCtlOutputFormat format);

  private:
    void                             startAbyssCtlSocket();

    std::vector<SP<SAbyssCtlCommand>> m_commands;
    wl_event_source*                 m_eventSource = nullptr;
    std::string                      m_socketPath;
};

inline UP<CAbyssCtl> g_pAbyssCtl;
