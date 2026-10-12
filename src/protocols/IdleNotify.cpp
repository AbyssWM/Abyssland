#include "IdleNotify.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

static int onTimer(SP<CEventLoopTimer> self, void* data) {

    const auto NOTIF = sc<CExtIdleNotification*>(data);

    NOTIF->onTimerFired();

    return 0;
}

CExtIdleNotification::CExtIdleNotification(SP<CExtIdleNotificationV1> resource_, uint32_t timeoutMs_, bool obeyInhibitors_) :
    m_resource(resource_), m_timeoutMs(timeoutMs_), m_obeyInhibitors(obeyInhibitors_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setDestroy([this](CExtIdleNotificationV1* r) { PROTO::idle->destroyNotification(this); });
    m_resource->setOnDestroy([this](CExtIdleNotificationV1* r) { PROTO::idle->destroyNotification(this); });

    m_timer = makeShared<CEventLoopTimer>(std::nullopt, onTimer, this);
    g_pEventLoopManager->addTimer(m_timer);

    update();

    LOGM(Log::DEBUG, "Registered idle-notification for {}ms", timeoutMs_);
}

CExtIdleNotification::~CExtIdleNotification() {
    g_pEventLoopManager->removeTimer(m_timer);
    m_timer.reset();
}

bool CExtIdleNotification::good() {
    return m_resource->resource();
}

void CExtIdleNotification::update(uint32_t elapsedMs) {
    m_timer->updateTimeout(std::nullopt);

    if (elapsedMs == 0 && PROTO::idle->isInhibited && m_obeyInhibitors) {
        reset();
        return;
    }

    if (m_timeoutMs > elapsedMs) {
        reset();
        m_timer->updateTimeout(std::chrono::milliseconds(m_timeoutMs - elapsedMs));
    } else
        onTimerFired();
}

void CExtIdleNotification::update() {
    update(0);
}

void CExtIdleNotification::onTimerFired() {
    if (m_idled)
        return;

    m_resource->sendIdled();
    m_idled = true;
}

void CExtIdleNotification::reset() {
    if (!m_idled)
        return;

    m_resource->sendResumed();
    m_idled = false;
}

bool CExtIdleNotification::inhibitorsAreObeyed() const {
    return m_obeyInhibitors;
}

CIdleNotifyProtocol::CIdleNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CIdleNotifyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CExtIdleNotifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CExtIdleNotifierV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CExtIdleNotifierV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetIdleNotification(
        [this](CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat) { this->onGetNotification(pMgr, id, timeout, seat, true); });
    RESOURCE->setGetInputIdleNotification(
        [this](CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat) { this->onGetNotification(pMgr, id, timeout, seat, false); });
}

void CIdleNotifyProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CIdleNotifyProtocol::destroyNotification(CExtIdleNotification* notif) {
    std::erase_if(m_notifications, [&](const auto& other) { return other.get() == notif; });
}

void CIdleNotifyProtocol::onGetNotification(CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat, bool obeyInhibitors) {
    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_notifications.emplace_back(makeShared<CExtIdleNotification>(makeShared<CExtIdleNotificationV1>(CLIENT, pMgr->version(), id), timeout, obeyInhibitors)).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_notifications.pop_back();
        return;
    }
}

void CIdleNotifyProtocol::onActivity() {
    for (auto const& n : m_notifications) {
        n->update();
    }

    if (m_rules.empty())
        return;
    // snapshot: a callback may cancel rules or reload the config
    const auto RULES = m_rules;
    for (auto const& r : RULES) {
        if (!r->cancelled())
            r->update(0);
    }
}

void CIdleNotifyProtocol::setInhibit(bool inhibited) {
    isInhibited = inhibited;
    for (auto const& n : m_notifications) {
        if (n->inhibitorsAreObeyed())
            n->update();
    }

    if (m_rules.empty())
        return;
    const auto RULES = m_rules;
    for (auto const& r : RULES) {
        if (!r->cancelled())
            r->update(0);
    }
}

void CIdleNotifyProtocol::setTimers(uint32_t elapsedMs) {
    for (auto const& n : m_notifications) {
        n->update(elapsedMs);
    }

    if (m_rules.empty())
        return;
    const auto RULES = m_rules;
    for (auto const& r : RULES) {
        if (!r->cancelled())
            r->update(elapsedMs);
    }
}

CIdleNotifyProtocol::~CIdleNotifyProtocol() {
    // the config lifecycle may still hold the rules: make sure they never fire again
    const auto RULES = m_rules;
    for (auto const& r : RULES)
        r->cancel();
}

SP<CIdleRule> CIdleNotifyProtocol::addRule(uint32_t timeoutMs, std::function<void()> onIdle, std::function<void()> onResume) {
    auto rule = makeShared<CIdleRule>(timeoutMs, std::move(onIdle), std::move(onResume));
    rule->m_self = rule;
    m_rules.emplace_back(rule);
    rule->arm();
    return rule;
}

size_t CIdleNotifyProtocol::ruleCount() const {
    return m_rules.size();
}

// ---- native rules ----

CIdleRule::CIdleRule(uint32_t timeoutMs, std::function<void()> onIdle, std::function<void()> onResume) :
    m_timeoutMs(timeoutMs), m_onIdle(std::move(onIdle)), m_onResume(std::move(onResume)) {
    ;
}

CIdleRule::~CIdleRule() {
    if (m_timer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(m_timer);
}

bool CIdleRule::cancelled() const {
    return m_cancelled;
}

bool CIdleRule::idled() const {
    return m_idled;
}

void CIdleRule::arm() {
    m_timer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [w = m_self](SP<CEventLoopTimer>, void*) {
            if (const auto R = w.lock(); R && !R->m_cancelled)
                R->onTimerFired();
            return 0;
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_timer);
    update(0);
}

void CIdleRule::cancel() {
    if (m_cancelled)
        return;
    m_cancelled = true;
    // stays alive until the end of this function even if the caller held the last reference
    const auto SELF = m_self.lock();

    if (m_timer) {
        m_timer->updateTimeout(std::nullopt);
        if (g_pEventLoopManager)
            g_pEventLoopManager->removeTimer(m_timer);
        m_timer.reset();
    }
    if (PROTO::idle)
        std::erase(PROTO::idle->m_rules, SELF);
    m_onIdle   = nullptr;
    m_onResume = nullptr;
}

void CIdleRule::update(uint32_t elapsedMs) {
    if (m_cancelled || !m_timer)
        return;

    // activity or an inhibitor counts the whole timeout again (forceIdle passes elapsedMs > 0 and ignores the inhibitor, like the protocol)
    m_timer->updateTimeout(std::nullopt);

    if (elapsedMs == 0 && PROTO::idle && PROTO::idle->isInhibited) {
        resume();
        return;
    }

    if (m_timeoutMs > elapsedMs) {
        resume();
        if (!m_cancelled && m_timer)
            m_timer->updateTimeout(std::chrono::milliseconds(m_timeoutMs - elapsedMs));
    } else
        becomeIdle();
}

void CIdleRule::onTimerFired() {
    becomeIdle();
}

void CIdleRule::becomeIdle() {
    if (m_idled || m_cancelled)
        return;
    m_idled = true; // state first: a callback that reloads or triggers activity sees the right state
    if (!m_onIdle)
        return;
    const auto SELF = m_self.lock();
    const auto FN   = m_onIdle; // copy: cancel() inside the callback must not destroy the running function
    try {
        FN();
    } catch (const std::exception& e) { Log::logger->log(Log::ERR, "[idle] exception in onIdle: {}", e.what()); } catch (...) {
        Log::logger->log(Log::ERR, "[idle] unknown exception in onIdle");
    }
}

void CIdleRule::resume() {
    if (!m_idled || m_cancelled)
        return; // no resume without a previous idle
    m_idled = false;
    if (!m_onResume)
        return;
    const auto SELF = m_self.lock();
    const auto FN   = m_onResume;
    try {
        FN();
    } catch (const std::exception& e) { Log::logger->log(Log::ERR, "[idle] exception in onResume: {}", e.what()); } catch (...) {
        Log::logger->log(Log::ERR, "[idle] unknown exception in onResume");
    }
}
