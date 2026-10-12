#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include "WaylandProtocol.hpp"
#include "ext-idle-notify-v1.hpp"

class CEventLoopTimer;

class CExtIdleNotification {
  public:
    CExtIdleNotification(SP<CExtIdleNotificationV1> resource_, uint32_t timeoutMs, bool obeyInhibitors);
    ~CExtIdleNotification();

    bool good();
    void onTimerFired();

    bool inhibitorsAreObeyed() const;

  private:
    SP<CExtIdleNotificationV1> m_resource;
    uint32_t                   m_timeoutMs = 0;
    SP<CEventLoopTimer>        m_timer;

    bool                       m_idled          = false;
    bool                       m_obeyInhibitors = false;

    void                       reset();
    void                       update();
    void                       update(uint32_t elapsedMs);

    friend class CIdleNotifyProtocol;
};

// A native idle rule (Abysslang c.idle). Same semantics as a CExtIdleNotification that obeys inhibitors, but calls functions instead of
// sending events. Owned by CIdleNotifyProtocol (alive while registered); the config lifecycle keeps a second reference to cancel it.
class CIdleRule {
  public:
    CIdleRule(uint32_t timeoutMs, std::function<void()> onIdle, std::function<void()> onResume);
    ~CIdleRule();

    // unregisters the rule and disarms its timer; no callback runs afterwards. Safe to call from inside a callback, safe to call twice.
    void cancel();
    bool cancelled() const;
    bool idled() const;

  private:
    void arm();
    void update(uint32_t elapsedMs);
    void onTimerFired();
    void becomeIdle();
    void resume();

    uint32_t              m_timeoutMs = 0;
    std::function<void()> m_onIdle, m_onResume;
    SP<CEventLoopTimer>   m_timer;
    bool                  m_idled     = false;
    bool                  m_cancelled = false;
    WP<CIdleRule>         m_self;

    friend class CIdleNotifyProtocol;
};

class CIdleNotifyProtocol : public IWaylandProtocol {
  public:
    CIdleNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name);
    virtual ~CIdleNotifyProtocol();

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    void         onActivity();
    void         setInhibit(bool inhibited);
    void         setTimers(uint32_t elapsedMs);

    // native rules: registered rules obey wayland idle inhibitors, count from the last activity, and are dropped by CIdleRule::cancel()
    SP<CIdleRule> addRule(uint32_t timeoutMs, std::function<void()> onIdle, std::function<void()> onResume);
    size_t        ruleCount() const;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyNotification(CExtIdleNotification* notif);
    void onGetNotification(CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat, bool obeyInhibitors);

    bool isInhibited = false;

    //
    std::vector<UP<CExtIdleNotifierV1>>   m_managers;
    std::vector<SP<CExtIdleNotification>> m_notifications;
    std::vector<SP<CIdleRule>>            m_rules;

    friend class CExtIdleNotification;
    friend class CIdleRule;
};

namespace PROTO {
    inline UP<CIdleNotifyProtocol> idle;
};
