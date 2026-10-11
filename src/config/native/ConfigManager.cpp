#include "ConfigManager.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

#include <algorithm>

#include <abyssutils/utils/ScopeGuard.hpp>

#include "../values/ConfigValues.hpp"
#include "../shared/workspace/WorkspaceRuleManager.hpp"
#include "../shared/monitor/MonitorRuleManager.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../supplementary/propRefresher/PropRefresher.hpp"
#include "../ConfigValue.hpp"

#include "../../desktop/rule/Engine.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../event/EventBus.hpp"
#include "../../Compositor.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/EventManager.hpp"
#include "../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../notification/NotificationOverlay.hpp"
#include "../../errorOverlay/Overlay.hpp"
#include "../../render/Renderer.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../debug/log/Logger.hpp"

using namespace Config;
using namespace Config::Native;

static CConfigManager* g_native = nullptr;

WP<CConfigManager> Native::mgr() {
    if (!g_native || !Config::mgr())
        return {};
    return dynamicPointerCast<CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));
}

CConfigManager* Native::self() {
    return g_native;
}

std::string CConfigManager::normalizeName(std::string_view s) {
    std::string cpy{s};
    std::ranges::replace(cpy, ':', '.');
    std::ranges::replace(cpy, '-', '_');
    return cpy;
}

static std::string normalizeDeviceName(std::string_view dev) {
    std::string copy{dev};
    std::ranges::replace(copy, ' ', '-');
    return copy;
}

CConfigManager::SDeviceConfig::SDeviceConfig() {
    for (const auto& valRaw : Values::CONFIG_DEVICE_VALUE_NAMES) {
        auto generic = std::ranges::find_if(Values::CONFIG_VALUES, [&valRaw](const auto& g) { return g->name() == valRaw; });

        RASSERT(generic != Values::CONFIG_VALUES.end(), "Native: device value has no generic counterpart, what the fuck happened?");

        const auto NORM   = normalizeName(valRaw);
        const auto DEVKEY = NORM.contains('.') ? NORM.substr(NORM.find_last_of('.') + 1) : NORM;

        if (values.contains(DEVKEY))
            continue; // some are duped

        values.emplace(DEVKEY, cellFromValue(*generic));
    }

    // the special per-device keybinds toggle
    values.emplace("keybinds", makeBoolCell(true));
}

CConfigManager::CConfigManager() {
    for (const auto& v : Values::CONFIG_VALUES) {
        auto cell = cellFromValue(v);
        RASSERT(cell, "Native: option {} has an unsupported type", v->name());
        m_configValues.emplace(normalizeName(v->name()), std::move(cell));
    }

    g_native = this;

    // persistent: tells onStart() callbacks whether the compositor is already running
    m_life.startListener = Event::bus()->m_events.start.listen([this] {
        m_life.started = true;
        // callbacks may register more callbacks (-> run immediately, as started is set), or trigger a reload
        auto cbs = std::move(m_life.startCallbacks);
        m_life.startCallbacks.clear();
        for (const auto& cb : cbs)
            cb();
    });
}

void CConfigManager::clearUserLifecycle() {
    m_life.listeners.clear();
    for (const auto& [id, t] : m_life.timers) {
        t->cancel();
        if (g_pEventLoopManager)
            g_pEventLoopManager->removeTimer(t);
    }
    m_life.timers.clear();
    m_life.startCallbacks.clear();
    for (const auto& l : m_life.layouts)
        Layout::Supplementary::algoMatcher()->unregisterAlgo(l);
    m_life.layouts.clear();
}

void CConfigManager::addLayoutName(std::string name) {
    m_life.layouts.emplace_back(std::move(name));
}

uint64_t CConfigManager::addListener(CAbyssSignalListener&& l) {
    const auto ID        = m_life.nextId++;
    m_life.listeners[ID] = std::move(l);
    return ID;
}

void CConfigManager::cancelListener(uint64_t id) {
    m_life.listeners.erase(id);
}

uint64_t CConfigManager::addTimer(SP<CEventLoopTimer> t) {
    const auto ID    = m_life.nextId++;
    m_life.timers[ID] = t;
    if (g_pEventLoopManager)
        g_pEventLoopManager->addTimer(t);
    return ID;
}

SP<CEventLoopTimer> CConfigManager::timerById(uint64_t id) {
    const auto IT = m_life.timers.find(id);
    return IT == m_life.timers.end() ? nullptr : IT->second;
}

void CConfigManager::cancelTimer(uint64_t id) {
    const auto IT = m_life.timers.find(id);
    if (IT == m_life.timers.end())
        return;
    IT->second->cancel();
    if (g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(IT->second);
    m_life.timers.erase(IT);
}

void CConfigManager::onStart(std::function<void()> fn) {
    if (m_life.started) {
        fn();
        return;
    }
    m_life.startCallbacks.emplace_back(std::move(fn));
}

bool CConfigManager::hasStarted() const {
    return m_life.started;
}

void CConfigManager::init() {
    reload();
}

void CConfigManager::reload() {
    Event::bus()->m_events.config.preReload.emit();

    Abyssutils::Utils::CScopeGuard x([this] {
        m_isParsingConfig = false;
        m_isFirstLaunch   = false;
    });
    m_isParsingConfig = true;

    // reset everything the user config can touch
    Config::animationTree()->reset();
    Config::workspaceRuleMgr()->clear();
    Config::monitorRuleMgr()->clear();
    Desktop::Rule::ruleEngine()->clearAllRules();
    if (g_pTrackpadGestures)
        g_pTrackpadGestures->clearGestures();
    if (g_pKeybindManager)
        g_pKeybindManager->clearKeybinds();

    // layouts are dropped and registered again: touch the workspaces only once, after the user config ran
    Layout::Supplementary::algoMatcher()->beginBatch();

    m_errors.clear();
    m_deviceConfigs.clear();
    clearUserLifecycle();

    for (const auto& [name, cell] : m_configValues) {
        cell->reset();
        cell->resetSetByUser();
    }

    applyUserConfig();

    Layout::Supplementary::algoMatcher()->endBatch();

    m_lastConfigVerificationWasSuccessful = m_errors.empty();

    postConfigReload();
}

void CConfigManager::postConfigReload() {
    static auto PSUPPRESSERRORS = CConfigValue<Config::INTEGER>("debug.suppress_errors");
    static auto PXWAYLAND       = CConfigValue<Config::INTEGER>("xwayland.enabled");
    static auto PMANUALCRASH    = CConfigValue<Config::INTEGER>("debug.manual_crash");
    static auto PENABLESTDOUT   = CConfigValue<Config::INTEGER>("debug.enable_stdout_logs");

    for (auto const& w : Desktop::windowState()->windows()) {
        w->uncacheWindowDecos();
    }

    // parseError will be displayed next frame
    if (!m_errors.empty() && !*PSUPPRESSERRORS) {
        std::string errorStr = "Your config has errors:\n";

        for (const auto& e : m_errors) {
            errorStr += e + "\n";

            if (std::ranges::count(errorStr, '\n') > 15) {
                errorStr += "... more";
                break;
            }
        }

        if (!errorStr.empty() && errorStr.back() == '\n')
            errorStr.pop_back();

        ErrorOverlay::overlay()->queueCreate(errorStr, ErrorOverlay::Colors::ERROR);
    } else
        ErrorOverlay::overlay()->destroy();

    // Set the modes for all monitors as we configured them
    // not on first launch because monitors might not exist yet
    // and they'll be taken care of in the newMonitor event
    if (!m_isFirstLaunch) {
        Config::monitorRuleMgr()->scheduleReload();
        Config::monitorRuleMgr()->ensureMonitorStatus();
        Config::monitorRuleMgr()->ensureVRR();
    }

#ifndef NO_XWAYLAND
    g_pCompositor->m_wantsXwayland = *PXWAYLAND;
    // enable/disable xwayland usage
    if (!m_isFirstLaunch && g_pXWayland /* XWayland has to be initialized by CCompositor::initManagers for this to make sense */) {
        bool prevEnabledXwayland = g_pXWayland->enabled();
        if (g_pCompositor->m_wantsXwayland != prevEnabledXwayland)
            g_pXWayland = makeUnique<CXWayland>(g_pCompositor->m_wantsXwayland);
    } else
        g_pCompositor->m_wantsXwayland = *PXWAYLAND;
#endif

    if (*PMANUALCRASH && !m_manualCrashInitiated) {
        m_manualCrashInitiated = true;
        Notification::overlay()->addNotification("Manual crash has been set up. Set debug.manual_crash back to 0 in order to crash the compositor.", CAbyssColor(0), 5000,
                                                 ICON_INFO);
    } else if (m_manualCrashInitiated && !*PMANUALCRASH) {
        // cowabunga it is
        g_pAbyssRenderer->initiateManualCrash();
    }

    auto disableStdout = !*PENABLESTDOUT;
    if (disableStdout && m_isFirstLaunch)
        Log::logger->log(Log::DEBUG,
                         "Disabling stdout logs (debug.enable_stdout_logs = 0). "
                         "Further logs will be written to {}",
                         g_pCompositor->m_instancePath + (ISDEBUG ? "/abysslandd.log" : "/abyssland.log"));

    Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_ALL);

    Event::bus()->m_events.config.reloaded.emit();
    if (g_pEventManager)
        g_pEventManager->postEvent(SAbyssIPCEvent{"configreloaded", ""});
}

void CConfigManager::addError(std::string&& str) {
    Log::logger->log(Log::ERR, "[cfg] {}", str);

    if (m_isParsingConfig) {
        m_errors.emplace_back(std::move(str));
        return;
    }

    // runtime: pop a notification
    Notification::overlay()->addNotification(std::format("Config error:\n{}", std::move(str)), 0, 5000, ICON_WARNING);
}

std::string CConfigManager::verify() {
    if (m_errors.empty())
        return "config ok";

    std::string fullStr = "";
    for (const auto& e : m_errors) {
        fullStr += e;
        fullStr += "\n";
    }

    fullStr.pop_back();
    return fullStr;
}

ICell* CConfigManager::findValue(std::string_view name) {
    const auto it = m_configValues.find(normalizeName(name));
    return it == m_configValues.end() ? nullptr : it->second.get();
}

bool CConfigManager::setInput(std::string_view name, const ConfigInput& in) {
    auto* cell = findValue(name);
    if (!cell) {
        addError(std::format("unknown config option \"{}\"", name));
        return false;
    }

    if (auto res = cell->set(in); !res) {
        addError(std::format("option \"{}\": {}", name, res.error()));
        return false;
    }

    // on a dynamic set (outside of a reload) the refresh has to be triggered, during reload REFRESH_ALL comes at the end
    if (!m_isParsingConfig && cell->refreshBits())
        Config::Supplementary::refresher()->scheduleRefresh(cell->refreshBits());

    return true;
}

bool CConfigManager::setDeviceInput(std::string_view device, std::string_view field, const ConfigInput& in) {
    const auto DEV = normalizeDeviceName(device);

    auto&      dev = m_deviceConfigs[DEV];

    const auto it = dev.values.find(normalizeName(field));
    if (it == dev.values.end()) {
        addError(std::format("device \"{}\": unknown field \"{}\"", device, field));
        return false;
    }

    if (auto res = it->second->set(in); !res) {
        addError(std::format("device \"{}\" field \"{}\": {}", device, field, res.error()));
        return false;
    }

    if (!m_isParsingConfig)
        Config::Supplementary::refresher()->scheduleRefresh(Supplementary::REFRESH_INPUT_DEVICES);

    return true;
}

void CConfigManager::ensureDevice(std::string_view device) {
    m_deviceConfigs[normalizeDeviceName(device)];
}

ICell* CConfigManager::findDeviceValue(const std::string& dev, const std::string& field) {
    const auto devIt = m_deviceConfigs.find(dev);
    if (devIt == m_deviceConfigs.end())
        return nullptr;
    const auto valIt = devIt->second.values.find(field);
    return valIt != devIt->second.values.end() ? valIt->second.get() : nullptr;
}

int CConfigManager::getDeviceInt(const std::string& dev, const std::string& field, const std::string& fb) {
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), normalizeName(field)); v && v->setByUser())
        return v->asInt();
    if (auto* f = fb.empty() ? nullptr : findValue(fb))
        return f->asInt();
    return 0;
}

float CConfigManager::getDeviceFloat(const std::string& dev, const std::string& field, const std::string& fb) {
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), normalizeName(field)); v && v->setByUser())
        return v->asFloat();
    if (auto* f = fb.empty() ? nullptr : findValue(fb))
        return f->asFloat();
    return 0.F;
}

Vector2D CConfigManager::getDeviceVec(const std::string& dev, const std::string& field, const std::string& fb) {
    auto toVec = [](const Config::VEC2& v) -> Vector2D { return {v.x, v.y}; };
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), normalizeName(field)); v && v->setByUser())
        return toVec(v->asVec2());
    if (auto* f = fb.empty() ? nullptr : findValue(fb))
        return toVec(f->asVec2());
    return {0, 0};
}

std::string CConfigManager::getDeviceString(const std::string& dev, const std::string& field, const std::string& fb) {
    auto clean = [](const Config::STRING& s) -> std::string { return s == STRVAL_EMPTY ? "" : s; };
    if (auto* v = findDeviceValue(normalizeDeviceName(dev), normalizeName(field)); v && v->setByUser())
        return clean(v->asString());
    if (auto* f = fb.empty() ? nullptr : findValue(fb))
        return clean(f->asString());
    return "";
}

bool CConfigManager::deviceConfigExplicitlySet(const std::string& dev, const std::string& field) {
    auto v = findDeviceValue(normalizeDeviceName(dev), normalizeName(field));
    return v && v->setByUser();
}

bool CConfigManager::deviceConfigExists(const std::string& dev) {
    return m_deviceConfigs.contains(normalizeDeviceName(dev));
}

SConfigOptionReply CConfigManager::getConfigValue(const std::string& s) {
    auto* cell = findValue(s);
    if (!cell)
        return SConfigOptionReply{.dataptr = nullptr};

    return SConfigOptionReply{.dataptr = cc<void* const*>(cell->dataPtrPtr()), .type = cell->underlying(), .setByUser = cell->setByUser()};
}

std::string CConfigManager::getMainConfigPath() {
    return "";
}

std::string CConfigManager::currentConfigPath() {
    return "";
}

std::string CConfigManager::getConfigString() {
    return "Not supported: the config is compiled in";
}

const std::vector<std::string>& CConfigManager::getConfigPaths() {
    return m_configPaths;
}

std::string CConfigManager::getErrors() {
    std::string errStr;
    for (const auto& e : m_errors) {
        errStr += e + "\n";
    }

    if (!errStr.empty())
        errStr.pop_back();

    return errStr;
}

bool CConfigManager::configVerifPassed() {
    return m_lastConfigVerificationWasSuccessful;
}

bool CConfigManager::isFirstLaunch() const {
    return m_isFirstLaunch;
}

bool CConfigManager::isParsingConfig() const {
    return m_isParsingConfig;
}
