#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "abyssland-lock-notify-v1.hpp"

class CEventLoopTimer;

class CAbysslandLockNotification {
  public:
    CAbysslandLockNotification(SP<CAbysslandLockNotificationV1> resource_);
    ~CAbysslandLockNotification() = default;

    bool good();
    void onLocked();
    void onUnlocked();

  private:
    SP<CAbysslandLockNotificationV1> m_resource;
    bool                            m_locked = false;
};

class CLockNotifyProtocol : public IWaylandProtocol {
  public:
    CLockNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onLocked();
    void         onUnlocked();

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyNotification(CAbysslandLockNotification* notif);
    void onGetNotification(CAbysslandLockNotifierV1* pMgr, uint32_t id);

    bool m_isLocked = false;

    //
    std::vector<UP<CAbysslandLockNotifierV1>>   m_managers;
    std::vector<SP<CAbysslandLockNotification>> m_notifications;

    friend class CAbysslandLockNotification;
};

namespace PROTO {
    inline UP<CLockNotifyProtocol> lockNotify;
};
