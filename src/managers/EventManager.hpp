#pragma once
#include <vector>
#include <abyssutils/os/FileDescriptor.hpp>
#include "../defines.hpp"
#include "../helpers/memory/Memory.hpp"

struct SAbyssIPCEvent {
    std::string event;
    std::string data;
};

class CEventManager {
  public:
    CEventManager();
    ~CEventManager();

    void postEvent(const SAbyssIPCEvent& event);

  private:
    std::string formatEvent(const SAbyssIPCEvent& event) const;

    static int  onServerEvent(int fd, uint32_t mask, void* data);
    static int  onClientEvent(int fd, uint32_t mask, void* data);

    int         onServerEvent(int fd, uint32_t mask);
    int         onClientEvent(int fd, uint32_t mask);

    struct SClient {
        Abyssutils::OS::CFileDescriptor fd;
        std::vector<SP<std::string>>   events;
        wl_event_source*               eventSource = nullptr;
        size_t                         writeOffset = 0;
    };

    std::vector<SClient>::iterator findClientByFD(int fd);
    std::vector<SClient>::iterator removeClientByFD(int fd);
    bool                           flushClient(SClient& client);

  private:
    Abyssutils::OS::CFileDescriptor m_socketFD;
    wl_event_source*               m_eventSource = nullptr;

    std::vector<SClient>           m_clients;
};

inline UP<CEventManager> g_pEventManager;
