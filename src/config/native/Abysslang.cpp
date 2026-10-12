// Implementation of the Abysslang config API (include/abyss/config.hpp).
// NOTE: this file must NOT `using namespace abyss`: inside abyss::Config members, `Config` is the class, the compositor's
// namespace has to be spelled ::Config.

#include <abyss/config.hpp>

#include <algorithm>
#include <any>
#include <cmath>
#include <limits>
#include <format>
#include <unordered_map>

#include "AbyssConvert.hpp"
#include "ConfigManager.hpp"
#include "../../saver/Saver.hpp"

#include "../supplementary/executor/Executor.hpp"
#include "../supplementary/propRefresher/PropRefresher.hpp"
#include "../shared/animation/AnimationTree.hpp"
#include "../shared/monitor/MonitorRuleManager.hpp"
#include "../shared/monitor/Parser.hpp"
#include "../shared/workspace/WorkspaceRuleManager.hpp"
#include "../shared/parserUtils/KeyParser.hpp"

#include "../../desktop/rule/Engine.hpp"
#include "../../desktop/rule/layerRule/LayerRule.hpp"
#include "../../desktop/rule/layerRule/LayerRuleEffectContainer.hpp"
#include "../../desktop/rule/windowRule/WindowRule.hpp"
#include "../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../event/EventBus.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/input/trackpad/TrackpadGestures.hpp"
#include "../../managers/input/trackpad/gestures/CallbackGesture.hpp"
#include "../../managers/input/trackpad/gestures/CloseGesture.hpp"
#include "../../managers/input/trackpad/gestures/CursorZoomGesture.hpp"
#include "../../managers/input/trackpad/gestures/FloatGesture.hpp"
#include "../../managers/input/trackpad/gestures/FullscreenGesture.hpp"
#include "../../managers/input/trackpad/gestures/MoveGesture.hpp"
#include "../../managers/input/trackpad/gestures/ResizeGesture.hpp"
#include "../../managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"
#include "../../managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp"
#include "../../managers/input/trackpad/gestures/ScrollMoveGesture.hpp"
#include "../../managers/permissions/DynamicPermissionManager.hpp"
#include "../../debug/log/Logger.hpp"

using ::Config::Native::CConfigManager;

// the public modifier constants must match the compositor's bits
static_assert(abyss::SHIFT.mask == HL_MODIFIER_SHIFT && abyss::CAPS.mask == HL_MODIFIER_CAPS && abyss::CTRL.mask == HL_MODIFIER_CTRL && abyss::ALT.mask == HL_MODIFIER_ALT &&
                  abyss::MOD2.mask == HL_MODIFIER_MOD2 && abyss::MOD3.mask == HL_MODIFIER_MOD3 && abyss::SUPER.mask == HL_MODIFIER_META && abyss::MOD5.mask == HL_MODIFIER_MOD5,
              "abyss modifier constants out of sync with eKeyboardModifiers");

namespace {
    CConfigManager* M() {
        return ::Config::Native::self();
    }

    void err(std::string s) {
        if (auto* m = M())
            m->addError(std::move(s));
        else
            Log::logger->log(Log::ERR, "[abyss] {}", s);
    }

    std::string str(bool b) {
        return b ? "true" : "false";
    }
    std::string str(int64_t v) {
        return std::to_string(v);
    }
    std::string str(double v) {
        return std::format("{}", v);
    }
    std::string str(const std::string& s) {
        return s;
    }

    // keybind handles of the current config load
    std::unordered_map<uint64_t, WP<SKeybind>> g_bindHandles;
    uint64_t                                   g_nextBindHandle = 1;
    // timer intervals for TimerHandle::setEnabled
    std::unordered_map<uint64_t, int>          g_timerIntervals;
    // numeric ids of event listeners are the manager's listener ids

    template <typename F, typename... A>
    void guarded(const char* what, const F& f, A&&... a) {
        try {
            f(std::forward<A>(a)...);
        } catch (const std::exception& e) {
            Log::logger->log(Log::ERR, "[abyss] exception in {}: {}", what, e.what());
            if (auto* m = M())
                m->addError(std::format("exception in {}: {}", what, e.what()));
        } catch (...) { Log::logger->log(Log::ERR, "[abyss] unknown exception in {}", what); }
    }

    std::string modsToString(abyss::Mods m) {
        static constexpr std::pair<uint32_t, const char*> NAMES[] = {
            {HL_MODIFIER_SHIFT, "SHIFT"}, {HL_MODIFIER_CAPS, "CAPS"}, {HL_MODIFIER_CTRL, "CTRL"}, {HL_MODIFIER_ALT, "ALT"},
            {HL_MODIFIER_MOD2, "MOD2"},   {HL_MODIFIER_MOD3, "MOD3"}, {HL_MODIFIER_META, "SUPER"}, {HL_MODIFIER_MOD5, "MOD5"},
        };
        std::string out;
        for (const auto& [bit, name] : NAMES) {
            if (m.mask & bit) {
                if (!out.empty())
                    out += "+";
                out += name;
            }
        }
        return out;
    }
}

// ------------------------------------------------------------------------------------------------ env / autostart

void abyss::Config::env(std::string name, std::string value, bool dbus) {
    auto* m = M();
    if (name.empty()) {
        err("env: name must not be empty");
        return;
    }

    // on a reload only touch the environment if the value changed
    if (m && !m->isFirstLaunch()) {
        const auto* CUR = getenv(name.c_str());
        if (CUR && value == CUR)
            return;
    }

    setenv(name.c_str(), value.c_str(), 1);

    if (dbus) {
        std::string CMD;
#ifdef USES_SYSTEMD
        CMD = "systemctl --user import-environment '" + name + "' && hash dbus-update-activation-environment 2>/dev/null && ";
#endif
        CMD += "dbus-update-activation-environment --systemd '" + name + "'";
        if (m && m->isFirstLaunch())
            ::Config::Supplementary::executor()->addExecOnce({CMD, false});
        else
            ::Config::Supplementary::executor()->spawnRaw(CMD);
    }
}

void abyss::Config::exec(std::string cmd) {
    if (auto* m = M())
        m->onStart([cmd = std::move(cmd)] { ::Config::Supplementary::executor()->spawn(cmd); });
}

void abyss::Config::execOnce(std::string cmd) {
    auto* m = M();
    if (m && m->isFirstLaunch())
        ::Config::Supplementary::executor()->addExecOnce({cmd, true});
}

void abyss::Config::execShutdown(std::string cmd) {
    auto* m = M();
    if (m && m->isFirstLaunch())
        ::Config::Supplementary::executor()->addExecShutdown({cmd, true});
}

void abyss::Config::onStart(std::function<void()> fn) {
    // once per process: an in-process reload must not run it again
    if (auto* m = M(); m && !m->hasStarted())
        m->onStart([fn = std::move(fn)] { guarded("onStart callback", fn); });
}

void abyss::Config::onShutdown(std::function<void()> fn) {
    on(::Event::Shutdown, std::move(fn));
}

// ------------------------------------------------------------------------------------------------ monitors / workspaces / devices

static bool applyMonitorOpts(::Config::CMonitorRuleParser& p, const abyss::MonitorOpts& o) {
    bool ok = true;
    auto check = [&](bool r, const char* what) {
        if (!r) {
            ok = false;
            err(std::format("monitor {}: bad {}{}", p.name(), what, p.getError() ? ": " + *p.getError() : ""));
        }
    };

    auto& r = p.rule();
    if (o.mirror)
        p.setMirror(*o.mirror);
    if (o.transform) {
        if (*o.transform < 0 || *o.transform > 7)
            check(false, "transform (expected 0..7)");
        else
            r.m_transform = sc<wl_output_transform>(*o.transform);
    }
    if (o.bitdepth)
        r.m_enable10bit = *o.bitdepth == 10;
    if (o.cm)
        check(p.parseCM(*o.cm), "cm");
    if (o.vrr)
        r.m_vrr = *o.vrr < 0 ? std::nullopt : std::optional<int>(*o.vrr);
    if (o.reserved)
        check(p.setReserved(Desktop::CReservedArea(o.reserved->top, o.reserved->right, o.reserved->bottom, o.reserved->left)), "reserved area");
    if (o.icc)
        check(p.parseICC(*o.icc), "icc");
    if (o.sdrEotf)
        r.m_sdrEotf = NTransferFunction::fromString(*o.sdrEotf);
    if (o.sdrBrightness)
        r.m_sdrBrightness = *o.sdrBrightness;
    if (o.sdrSaturation)
        r.m_sdrSaturation = *o.sdrSaturation;
    if (o.supportsWideColor)
        r.m_supportsWideColor = *o.supportsWideColor;
    if (o.supportsHdr)
        r.m_supportsHDR = *o.supportsHdr;
    if (o.sdrMinLuminance)
        r.m_sdrMinLuminance = *o.sdrMinLuminance;
    if (o.sdrMaxLuminance)
        r.m_sdrMaxLuminance = *o.sdrMaxLuminance;
    if (o.minLuminance)
        r.m_minLuminance = *o.minLuminance;
    if (o.maxLuminance)
        r.m_maxLuminance = *o.maxLuminance;
    if (o.maxAvgLuminance)
        r.m_maxAvgLuminance = *o.maxAvgLuminance;
    return ok;
}

void abyss::Config::monitor(std::string name, std::string mode, std::string position, std::string scale, const MonitorOpts& opts) {
    // an empty name is the catch-all rule for every monitor without a rule of its own (Hyprland: `monitor=,preferred,auto,1`)
    ::Config::CMonitorRuleParser parser(name);

    // merge into an existing rule of the same output
    const auto EXISTING = std::ranges::find_if(::Config::monitorRuleMgr()->all(), [&name](const auto& rule) { return rule.m_name == name; });
    if (EXISTING != ::Config::monitorRuleMgr()->all().end())
        parser.rule() = *EXISTING;

    if (!parser.parseMode(mode))
        err(std::format("monitor {}: bad mode \"{}\"{}", name, mode, parser.getError() ? ": " + *parser.getError() : ""));
    if (!parser.parsePosition(position))
        err(std::format("monitor {}: bad position \"{}\"{}", name, position, parser.getError() ? ": " + *parser.getError() : ""));
    if (!parser.parseScale(scale))
        err(std::format("monitor {}: bad scale \"{}\"{}", name, scale, parser.getError() ? ": " + *parser.getError() : ""));
    applyMonitorOpts(parser, opts);

    ::Config::monitorRuleMgr()->add(std::move(parser.rule()));
    ::Config::Supplementary::refresher()->scheduleRefresh(::Config::Supplementary::REFRESH_MONITOR_STATES);
}

void abyss::Config::monitor(std::string name, std::string mode, std::string position, double scale, const MonitorOpts& opts) {
    monitor(std::move(name), std::move(mode), std::move(position), scale <= 0 ? std::string("auto") : std::format("{}", scale), opts);
}

void abyss::Config::disableMonitor(std::string name) {
    ::Config::CMonitorRuleParser parser(name);
    parser.setDisabled();
    ::Config::monitorRuleMgr()->add(std::move(parser.rule()));
    ::Config::Supplementary::refresher()->scheduleRefresh(::Config::Supplementary::REFRESH_MONITOR_STATES);
}

void abyss::Config::workspaceRule(std::string workspace, const WorkspaceRule& r) {
    const auto& [wsId, wsName, isAutoID] = getWorkspaceIDNameFromString(workspace);

    ::Config::CWorkspaceRule rule;
    rule.m_workspaceString = workspace;
    rule.m_workspaceName   = wsName;
    rule.m_workspaceId     = isAutoID ? WORKSPACE_INVALID : wsId;
    rule.setEnabled(true);

    auto gap = [](const abyss::Gap& g) { return ::Config::CCssGapData(g.top, g.right, g.bottom, g.left); };

    if (r.monitor)
        rule.m_monitor = *r.monitor;
    if (r.isDefault)
        rule.m_isDefault = *r.isDefault;
    if (r.persistent)
        rule.m_isPersistent = *r.persistent;
    if (r.gapsIn)
        rule.m_gapsIn = gap(*r.gapsIn);
    if (r.gapsOut)
        rule.m_gapsOut = gap(*r.gapsOut);
    if (r.floatGaps)
        rule.m_floatGaps = gap(*r.floatGaps);
    if (r.borderSize)
        rule.m_borderSize = *r.borderSize;
    if (r.noBorder)
        rule.m_noBorder = *r.noBorder;
    if (r.noRounding)
        rule.m_noRounding = *r.noRounding;
    if (r.decorate)
        rule.m_decorate = *r.decorate;
    if (r.noShadow)
        rule.m_noShadow = *r.noShadow;
    if (r.onCreatedEmpty)
        rule.m_onCreatedEmptyRunCmd = *r.onCreatedEmpty;
    if (r.defaultName)
        rule.m_defaultName = *r.defaultName;
    if (r.layout)
        rule.m_layout = *r.layout;
    if (r.animation)
        rule.m_animationStyle = *r.animation;
    for (const auto& [k, v] : r.layoutOpts)
        rule.m_layoutopts[k] = v;

    ::Config::workspaceRuleMgr()->replaceOrAdd(std::move(rule));
    ::Config::Supplementary::refresher()->scheduleRefresh(::Config::Supplementary::REFRESH_MONITOR_STATES | ::Config::Supplementary::REFRESH_WINDOW_STATES);
}

void abyss::Config::device(std::string name, const DeviceOpts& o) {
    auto* m = M();
    if (!m)
        return;

    m->ensureDevice(name);

#define ABYSS_X(f, key, T)                                                                                                                                                         \
    if (o.f)                                                                                                                                                                       \
        m->setDeviceInput(name, key, abyss::detail::toInput(*o.f));
    ABYSS_DEVICE_FIELDS(ABYSS_X)
#undef ABYSS_X
}

// ------------------------------------------------------------------------------------------------ rules

static void addMatch(Desktop::Rule::IRule& rule, const char* prop, const std::string& value, const char* what) {
    const auto P = Desktop::Rule::matchPropFromString(prop);
    if (!P) {
        err(std::format("{}: unknown match property '{}'", what, prop));
        return;
    }
    rule.registerMatch(*P, value);
}

void abyss::Config::windowRule(const WindowMatch& m, const WindowEffects& e, std::string name) {
    auto rule = makeShared<Desktop::Rule::CWindowRule>(name);

    auto put = [&](const char* prop, const auto& opt) {
        if (opt)
            addMatch(*rule, prop, str(*opt), "windowRule");
    };
    auto putInt = [&](const char* prop, const std::optional<int>& opt) {
        if (opt)
            addMatch(*rule, prop, std::to_string(*opt), "windowRule");
    };

    put("class", m.cls);
    put("title", m.title);
    put("initial_class", m.initialClass);
    put("initial_title", m.initialTitle);
    put("float", m.floating);
    put("tag", m.tag);
    put("xwayland", m.xwayland);
    put("fullscreen", m.fullscreen);
    put("pin", m.pinned);
    put("focus", m.focus);
    put("group", m.group);
    put("modal", m.modal);
    put("workspace", m.workspace);
    put("content", m.content);
    put("xdg_tag", m.xdgTag);
    putInt("fullscreen_state_internal", m.fullscreenStateInternal);
    putInt("fullscreen_state_client", m.fullscreenStateClient);
    for (const auto& [k, v] : m.extra)
        addMatch(*rule, k.c_str(), v, "windowRule");

    auto addEffect = [&](const char* effName, const std::string& value) {
        const auto EFF = Desktop::Rule::windowEffects()->get(std::string_view{effName});
        if (!EFF) {
            err(std::format("windowRule: unknown effect '{}'", effName));
            return;
        }
        if (const auto R = rule->addEffect(*EFF, value); !R)
            err(std::format("windowRule: effect '{}': {}", effName, R.error()));
    };

#define ABYSS_X(f, n, T)                                                                                                                                                           \
    if (e.f)                                                                                                                                                                       \
        addEffect(n, str(*e.f));
    ABYSS_WINDOW_EFFECTS(ABYSS_X)
#undef ABYSS_X
    for (const auto& [k, v] : e.extra)
        addEffect(k.c_str(), v);

    Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>(rule));
    ::Config::Supplementary::refresher()->scheduleRefresh(::Config::Supplementary::REFRESH_WINDOW_STATES);
}

void abyss::Config::layerRule(const LayerMatch& m, const LayerEffects& e, std::string name) {
    auto rule = makeShared<Desktop::Rule::CLayerRule>(name);

    if (m.nspace)
        addMatch(*rule, "namespace", *m.nspace, "layerRule");
    for (const auto& [k, v] : m.extra)
        addMatch(*rule, k.c_str(), v, "layerRule");

    auto addEffect = [&](const char* effName, const std::string& value) {
        const auto EFF = Desktop::Rule::layerEffects()->get(std::string_view{effName});
        if (!EFF) {
            err(std::format("layerRule: unknown effect '{}'", effName));
            return;
        }
        if (const auto R = rule->addEffect(*EFF, value); !R)
            err(std::format("layerRule: effect '{}': {}", effName, R.error()));
    };

#define ABYSS_X(f, n, T)                                                                                                                                                           \
    if (e.f)                                                                                                                                                                       \
        addEffect(n, str(*e.f));
    ABYSS_LAYER_EFFECTS(ABYSS_X)
#undef ABYSS_X
    for (const auto& [k, v] : e.extra)
        addEffect(k.c_str(), v);

    Desktop::Rule::ruleEngine()->registerRule(SP<Desktop::Rule::IRule>(rule));
    ::Config::Supplementary::refresher()->scheduleRefresh(::Config::Supplementary::REFRESH_RULES);
}

// ------------------------------------------------------------------------------------------------ animations

void abyss::Config::bezier(std::string name, double x0, double y0, double x1, double y1) {
    if (name.empty()) {
        err("bezier: name must not be empty");
        return;
    }
    for (const auto& [what, v] : {std::pair<const char*, double>{"x0", x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}}) {
        if (v < -1.0 || v > 2.0) {
            err(std::format("bezier \"{}\": {} = {} is out of range [-1, 2]", name, what, v));
            return;
        }
    }
    Animation::mgr()->addBezierWithName(name, Vector2D(x0, y0), Vector2D(x1, y1));
}

void abyss::Config::spring(std::string name, double mass, double stiffness, double damping) {
    if (name.empty()) {
        err("spring: name must not be empty");
        return;
    }
    if (mass <= 0.5 || stiffness <= 0.5 || damping <= 0.5) {
        err(std::format("spring \"{}\": mass, stiffness and damping all must be > 0.5", name));
        return;
    }

    Abyssutils::Animation::SSpringCurve curve;
    curve.stiffness = stiffness;
    curve.damping   = damping;
    curve.mass      = mass;
    Animation::mgr()->addSpringWithName(name, curve);
}

void abyss::Config::animation(std::string leaf, bool enabled, double speed, std::string curve, std::string style) {
    if (!::Config::animationTree()->nodeExists(leaf)) {
        err(std::format("animation: no such animation leaf \"{}\"", leaf));
        return;
    }

    if (!enabled) {
        ::Config::animationTree()->setConfigForNode(leaf, false, 1, "default");
        return;
    }

    if (speed <= 0 || speed > 100) {
        err(std::format("animation \"{}\": speed must be in (0, 100]", leaf));
        return;
    }

    std::string curveName;
    if (Animation::mgr()->bezierExists(curve))
        curveName = curve;
    else if (Animation::mgr()->springExists(curve))
        curveName = "spring:" + curve;
    else {
        err(std::format("animation \"{}\": no such bezier or spring \"{}\"", leaf, curve));
        return;
    }

    if (!style.empty()) {
        if (const auto E = Animation::mgr()->styleValidInConfigVar(leaf, style); !E.empty()) {
            err(std::format("animation \"{}\": {}", leaf, E));
            return;
        }
    }

    ::Config::animationTree()->setConfigForNode(leaf, true, sc<float>(speed), curveName, style);
}

// ------------------------------------------------------------------------------------------------ binds

abyss::BindHandle abyss::Config::bind(Mods mods, std::string key, Action action, const BindOpts& opts) {
    const auto MODS = modsToString(mods);
    return bind(MODS.empty() ? key : MODS + "+" + key, std::move(action), opts);
}

abyss::BindHandle abyss::Config::bind(std::string combo, Action action, const BindOpts& o) {
    if (!g_pKeybindManager) {
        err("bind: keybind manager not available");
        return {};
    }

    SKeybind kb;
    kb.submap.name  = m_currentSubmap;
    kb.submap.reset = m_currentSubmapReset;

    if (const auto R = ::Config::KeyParser::parseKeyString(kb, combo); !R) {
        err(std::format("bind \"{}\": {}", combo, R.error()));
        return {};
    }

    if (kb.catchAll && m_currentSubmap.empty()) {
        err(std::format("bind \"{}\": catchall binds are only allowed in submaps", combo));
        return {};
    }

    if (action.isNative()) {
        kb.handler  = "native";
        kb.nativeFn = [fn = action.fn, combo]() -> SDispatchResult {
            try {
                return fn();
            } catch (const std::exception& e) {
                Log::logger->log(Log::ERR, "[abyss] exception in bind \"{}\": {}", combo, e.what());
                return {.success = false, .error = e.what()};
            } catch (...) {
                Log::logger->log(Log::ERR, "[abyss] unknown exception in bind \"{}\"", combo);
                return {.success = false, .error = "unknown exception"};
            }
        };
    } else {
        if (!g_pKeybindManager->m_dispatchers.contains(action.handler)) {
            err(std::format("bind \"{}\": unknown dispatcher \"{}\"", combo, action.handler));
            return {};
        }
        kb.handler = action.handler;
        kb.arg     = action.arg;
    }

    if (o.mouse && kb.handler != "mouse") {
        err(std::format("bind \"{}\": .mouse requires mouseMove() or mouseResize() as the action", combo));
        return {};
    }
    if (kb.handler == "mouse" && (o.repeat || o.release || o.locked)) {
        err(std::format("bind \"{}\": mouse binds cannot be repeat / release / locked", combo));
        return {};
    }
    if (o.click && o.drag) {
        err(std::format("bind \"{}\": click and drag are exclusive", combo));
        return {};
    }
    if ((o.longPress || o.release || o.click || o.drag) && o.repeat) {
        err(std::format("bind \"{}\": long press / release / click / drag are incompatible with repeat", combo));
        return {};
    }

    kb.displayKey      = combo;
    kb.repeat          = o.repeat;
    kb.locked          = o.locked;
    kb.release         = o.release || o.click || o.drag;
    kb.longPress       = o.longPress;
    kb.nonConsuming    = o.nonConsuming;
    kb.autoConsuming   = o.autoConsuming;
    kb.transparent     = o.transparent;
    kb.ignoreMods      = o.ignoreMods;
    kb.dontInhibit     = o.dontInhibit;
    kb.click           = o.click;
    kb.drag            = o.drag;
    kb.submapUniversal = o.submapUniversal;
    kb.allowInputCapture = o.allowInputCapture;
    if (!o.description.empty()) {
        kb.description    = o.description;
        kb.hasDescription = true;
    }
    if (!o.devices.empty()) {
        kb.deviceInclusive = o.devicesInclusive;
        for (const auto& d : o.devices)
            kb.devices.emplace(d);
    }

    const auto BIND = g_pKeybindManager->addKeybind(kb);
    const auto ID   = g_nextBindHandle++;
    g_bindHandles[ID] = BIND;
    return BindHandle{ID};
}

void abyss::BindHandle::setEnabled(bool enabled) const {
    const auto IT = g_bindHandles.find(id);
    if (IT == g_bindHandles.end())
        return;
    if (const auto K = IT->second.lock(); K)
        K->enabled = enabled;
}

void abyss::Config::unbind(Mods mods, std::string key) {
    const auto MODS = modsToString(mods);
    unbind(MODS.empty() ? key : MODS + "+" + key);
}

void abyss::Config::unbind(std::string combo) {
    if (g_pKeybindManager)
        g_pKeybindManager->removeKeybind(combo);
}

void abyss::Config::unbindAll() {
    if (g_pKeybindManager)
        g_pKeybindManager->clearKeybinds();
}

void abyss::Config::submap(std::string name, const std::function<void(Config&)>& fn, std::string reset) {
    if (name.empty()) {
        err("submap: name must not be empty");
        return;
    }

    const auto PREV      = m_currentSubmap;
    const auto PREVRESET = m_currentSubmapReset;
    m_currentSubmap      = name;
    m_currentSubmapReset = reset;

    guarded("submap definition", [&] { fn(*this); });

    m_currentSubmap      = PREV;
    m_currentSubmapReset = PREVRESET;
}

// ------------------------------------------------------------------------------------------------ gestures

namespace {
    struct SGestureArgs {
        size_t                    fingers = 0;
        eTrackpadGestureDirection dir     = TRACKPAD_GESTURE_DIR_NONE;
        uint32_t                  mods    = 0;
        float                     scale   = 1.F;
    };

    // the checks every gesture registration has in common (as in the old hl.gesture); reports through err()
    std::optional<SGestureArgs> gestureArgs(int fingers, const std::string& direction, const abyss::GestureOpts& o) {
        if (!g_pTrackpadGestures) {
            err("gesture: trackpad gesture manager not available");
            return std::nullopt;
        }
        if (fingers < 2 || fingers > 9) {
            err("gesture: fingers must be in 2..9");
            return std::nullopt;
        }

        const auto DIR = g_pTrackpadGestures->dirForString(direction);
        if (DIR == TRACKPAD_GESTURE_DIR_NONE) {
            err(std::format("gesture: invalid direction \"{}\"", direction));
            return std::nullopt;
        }
        if (!(o.scale >= 0.1 && o.scale <= 10)) {
            err("gesture: scale must be in 0.1..10");
            return std::nullopt;
        }

        return SGestureArgs{.fingers = sc<size_t>(fingers),
                            .dir     = DIR,
                            .mods    = o.mods.empty() ? 0 : g_pKeybindManager->stringToModMask(o.mods),
                            .scale   = sc<float>(o.scale)};
    }

    void addCallbackGesture(const char* what, const SGestureArgs& a, const abyss::GestureOpts& o, CCallbackTrackpadGesture::Fn start, CCallbackTrackpadGesture::Fn update,
                            CCallbackTrackpadGesture::Fn end) {
        auto result = g_pTrackpadGestures->addGesture(makeUnique<CCallbackTrackpadGesture>(std::move(start), std::move(update), std::move(end)), a.fingers, a.dir, a.mods, a.scale,
                                                      o.disableInhibit);
        if (!result)
            err(std::format("{}: {}", what, result.error()));
    }

    CCallbackTrackpadGesture::Fn guardedGesture(std::function<void(const abyss::GestureEvent&)> f, const char* what) {
        if (!f)
            return {};
        return [f = std::move(f), what](const abyss::GestureEvent& e) { guarded(what, f, e); };
    }
}

void abyss::Config::gesture(int fingers, std::string direction, std::function<void()> onEnd, const GestureOpts& o) {
    const auto A = gestureArgs(fingers, direction, o);
    if (!A)
        return;
    if (!onEnd) {
        err("gesture: the callback is empty");
        return;
    }

    // like the old function form: only at the end, no arguments
    addCallbackGesture("gesture", *A, o, {}, {}, [onEnd = std::move(onEnd)](const GestureEvent&) { guarded("gesture callback", onEnd); });
}

void abyss::Config::gesture(int fingers, std::string direction, GestureCallbacks cb, const GestureOpts& o) {
    const auto A = gestureArgs(fingers, direction, o);
    if (!A)
        return;
    if (!cb.start && !cb.update && !cb.finish) {
        err("gesture: callbacks must define at least one of start, update or finish");
        return;
    }

    addCallbackGesture("gesture", *A, o, guardedGesture(std::move(cb.start), "gesture start callback"), guardedGesture(std::move(cb.update), "gesture update callback"),
                       guardedGesture(std::move(cb.finish), "gesture finish callback"));
}

void abyss::Config::gesture(int fingers, std::string direction, std::string action, const GestureOpts& o) {
    const auto A = gestureArgs(fingers, direction, o);
    if (!A)
        return;

    const auto DIR = A->dir;
    const auto F   = A->fingers;
    const uint32_t MODS = A->mods;
    const float    SC   = A->scale;

    std::expected<void, std::string> result;

    if (action == "workspace")
        result = g_pTrackpadGestures->addGesture(makeUnique<CWorkspaceSwipeGesture>(), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "resize")
        result = g_pTrackpadGestures->addGesture(makeUnique<CResizeTrackpadGesture>(), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "move")
        result = g_pTrackpadGestures->addGesture(makeUnique<CMoveTrackpadGesture>(), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "special")
        result = g_pTrackpadGestures->addGesture(makeUnique<CSpecialWorkspaceGesture>(o.workspaceName), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "close")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCloseTrackpadGesture>(), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "float")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFloatTrackpadGesture>(o.mode), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "fullscreen")
        result = g_pTrackpadGestures->addGesture(makeUnique<CFullscreenTrackpadGesture>(o.mode), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "cursor_zoom" || action == "cursorZoom")
        result = g_pTrackpadGestures->addGesture(makeUnique<CCursorZoomTrackpadGesture>(o.zoomLevel, o.mode), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "scroll_move")
        result = g_pTrackpadGestures->addGesture(makeUnique<CScrollMoveTrackpadGesture>(), F, DIR, MODS, SC, o.disableInhibit);
    else if (action == "unset")
        result = g_pTrackpadGestures->removeGesture(F, DIR, MODS, SC, o.disableInhibit);
    else {
        err(std::format("gesture: unknown action \"{}\"", action));
        return;
    }

    if (!result)
        err(std::format("gesture: {}", result.error()));
}

// ------------------------------------------------------------------------------------------------ events

#define FN(tag) std::remove_cvref_t<decltype(::Event::tag)>::Fn

uint64_t abyss::detail::registerEvent(EventId id, std::any fn) {
    auto* m = M();
    if (!m)
        return 0;

    auto&                  ev = ::Event::bus()->m_events;
    CAbyssSignalListener   l;

    try {
        switch (id) {
            case EventId::Start: {
                auto f = std::any_cast<FN(Start)>(fn);
                l      = ev.start.listen([f] { guarded("Start event", f); });
            } break;
            case EventId::Shutdown: {
                auto f = std::any_cast<FN(Shutdown)>(fn);
                l      = ev.exit.listen([f] { guarded("Shutdown event", f); });
            } break;
            case EventId::WindowOpen: {
                auto f = std::any_cast<FN(WindowOpen)>(fn);
                l      = ev.window.openLate.listen([f](PHLWINDOW w) { guarded("WindowOpen event", f, w); });
            } break;
            case EventId::WindowOpenEarly: {
                auto f = std::any_cast<FN(WindowOpenEarly)>(fn);
                l      = ev.window.openEarly.listen([f](PHLWINDOW w) { guarded("WindowOpenEarly event", f, w); });
            } break;
            case EventId::WindowClose: {
                auto f = std::any_cast<FN(WindowClose)>(fn);
                l      = ev.window.close.listen([f](PHLWINDOW w) { guarded("WindowClose event", f, w); });
            } break;
            case EventId::WindowActive: {
                auto f = std::any_cast<FN(WindowActive)>(fn);
                l      = ev.window.active.listen([f](PHLWINDOW w, Desktop::eFocusReason r) { guarded("WindowActive event", f, w, sc<int>(r)); });
            } break;
            case EventId::WindowUrgent: {
                auto f = std::any_cast<FN(WindowUrgent)>(fn);
                l      = ev.window.urgent.listen([f](PHLWINDOW w) { guarded("WindowUrgent event", f, w); });
            } break;
            case EventId::WindowTitle: {
                auto f = std::any_cast<FN(WindowTitle)>(fn);
                l      = ev.window.title.listen([f](PHLWINDOW w) { guarded("WindowTitle event", f, w); });
            } break;
            case EventId::WindowClass: {
                auto f = std::any_cast<FN(WindowClass)>(fn);
                l      = ev.window.class_.listen([f](PHLWINDOW w) { guarded("WindowClass event", f, w); });
            } break;
            case EventId::WindowPin: {
                auto f = std::any_cast<FN(WindowPin)>(fn);
                l      = ev.window.pin.listen([f](PHLWINDOW w) { guarded("WindowPin event", f, w); });
            } break;
            case EventId::WindowFullscreen: {
                auto f = std::any_cast<FN(WindowFullscreen)>(fn);
                l      = ev.window.fullscreen.listen([f](PHLWINDOW w) { guarded("WindowFullscreen event", f, w); });
            } break;
            case EventId::WindowFloating: {
                auto f = std::any_cast<FN(WindowFloating)>(fn);
                l      = ev.window.floating.listen([f](PHLWINDOW w) { guarded("WindowFloating event", f, w); });
            } break;
            case EventId::WindowUpdateRules: {
                auto f = std::any_cast<FN(WindowUpdateRules)>(fn);
                l      = ev.window.updateRules.listen([f](PHLWINDOW w) { guarded("WindowUpdateRules event", f, w); });
            } break;
            case EventId::WindowMoveToWorkspace: {
                auto f = std::any_cast<FN(WindowMoveToWorkspace)>(fn);
                l      = ev.window.moveToWorkspace.listen([f](PHLWINDOW w, PHLWORKSPACE ws) { guarded("WindowMoveToWorkspace event", f, w, ws); });
            } break;
            case EventId::LayerOpened: {
                auto f = std::any_cast<FN(LayerOpened)>(fn);
                l      = ev.layer.opened.listen([f](PHLLS ls) { guarded("LayerOpened event", f, ls); });
            } break;
            case EventId::LayerClosed: {
                auto f = std::any_cast<FN(LayerClosed)>(fn);
                l      = ev.layer.closed.listen([f](PHLLS ls) { guarded("LayerClosed event", f, ls); });
            } break;
            case EventId::MonitorAdded: {
                auto f = std::any_cast<FN(MonitorAdded)>(fn);
                l      = ev.monitor.added.listen([f](PHLMONITOR mon) { guarded("MonitorAdded event", f, mon); });
            } break;
            case EventId::MonitorRemoved: {
                auto f = std::any_cast<FN(MonitorRemoved)>(fn);
                l      = ev.monitor.removed.listen([f](PHLMONITOR mon) { guarded("MonitorRemoved event", f, mon); });
            } break;
            case EventId::MonitorFocused: {
                auto f = std::any_cast<FN(MonitorFocused)>(fn);
                l      = ev.monitor.focused.listen([f](PHLMONITOR mon) { guarded("MonitorFocused event", f, mon); });
            } break;
            case EventId::MonitorLayoutChanged: {
                auto f = std::any_cast<FN(MonitorLayoutChanged)>(fn);
                l      = ev.monitor.layoutChanged.listen([f] { guarded("MonitorLayoutChanged event", f); });
            } break;
            case EventId::WorkspaceActive: {
                auto f = std::any_cast<FN(WorkspaceActive)>(fn);
                l      = ev.workspace.active.listen([f](PHLWORKSPACE ws) { guarded("WorkspaceActive event", f, ws); });
            } break;
            case EventId::WorkspaceSpecialActive: {
                auto f = std::any_cast<FN(WorkspaceSpecialActive)>(fn);
                l      = ev.workspace.specialActive.listen([f](PHLWORKSPACE ws, PHLMONITOR mon) { guarded("WorkspaceSpecialActive event", f, ws, mon); });
            } break;
            case EventId::WorkspaceCreated: {
                auto f = std::any_cast<FN(WorkspaceCreated)>(fn);
                l      = ev.workspace.created.listen([f](PHLWORKSPACEREF ref) {
                    if (const auto ws = ref.lock(); ws)
                        guarded("WorkspaceCreated event", f, ws);
                });
            } break;
            case EventId::WorkspaceRemoved: {
                auto f = std::any_cast<FN(WorkspaceRemoved)>(fn);
                l      = ev.workspace.removed.listen([f](PHLWORKSPACEREF ref) { guarded("WorkspaceRemoved event", f, ref); });
            } break;
            case EventId::WorkspaceMoveToMonitor: {
                auto f = std::any_cast<FN(WorkspaceMoveToMonitor)>(fn);
                l      = ev.workspace.moveToMonitor.listen([f](PHLWORKSPACE ws, PHLMONITOR mon) { guarded("WorkspaceMoveToMonitor event", f, ws, mon); });
            } break;
            case EventId::ConfigReloaded: {
                auto f = std::any_cast<FN(ConfigReloaded)>(fn);
                l      = ev.config.reloaded.listen([f] { guarded("ConfigReloaded event", f); });
            } break;
            case EventId::PropsRefreshed: {
                auto f = std::any_cast<FN(PropsRefreshed)>(fn);
                l      = ev.config.props_refreshed.listen([f](const bool b) { guarded("PropsRefreshed event", f, b); });
            } break;
            case EventId::Submap: {
                auto f = std::any_cast<FN(Submap)>(fn);
                l      = ev.keybinds.submap.listen([f](const std::string& s) { guarded("Submap event", f, s); });
            } break;
            case EventId::ScreenshareState: {
                auto f = std::any_cast<FN(ScreenshareState)>(fn);
                l      = ev.screenshare.state.listen([f](bool state, uint8_t type, const std::string& name) { guarded("ScreenshareState event", f, state, sc<int>(type), name); });
            } break;
            case EventId::KeyboardKey: {
                auto f = std::any_cast<FN(KeyboardKey)>(fn);
                l      = ev.input.keyboard.key.listen([f](const IKeyboard::SKeyEvent& e, Event::SCallbackInfo&) {
                    guarded("KeyboardKey event", f, sc<int>(e.keycode + 8), sc<int>(e.timeMs), sc<int>(e.state));
                });
            } break;
        }
    } catch (const std::bad_any_cast&) {
        err("on: internal error, callback type does not match the event");
        return 0;
    }

    if (!l)
        return 0;

    return m->addListener(std::move(l));
}

void abyss::detail::cancelEvent(uint64_t handle) {
    if (auto* m = M())
        m->cancelListener(handle);
}

// ------------------------------------------------------------------------------------------------ timers

abyss::TimerHandle abyss::Config::timer(int ms, std::function<void()> fn, bool repeat) {
    if (ms <= 0) {
        err("timer: interval must be > 0 ms");
        return {};
    }

    auto* m = M();
    if (!m)
        return {};

    auto t = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(ms),
        [fn = std::move(fn), repeat, ms](SP<CEventLoopTimer> self, void*) {
            // re-arm first, so that a callback can still disarm / change it
            if (repeat)
                self->updateTimeout(std::chrono::milliseconds(ms));
            guarded("timer callback", fn);
        },
        nullptr);

    const auto ID = m->addTimer(t);
    g_timerIntervals[ID] = ms;
    return TimerHandle{ID};
}

void abyss::TimerHandle::setEnabled(bool enabled) const {
    auto* m = M();
    if (!m || !id)
        return;
    const auto T = m->timerById(id);
    if (!T)
        return;
    if (enabled) {
        const auto IT = g_timerIntervals.find(id);
        T->updateTimeout(std::chrono::milliseconds(IT == g_timerIntervals.end() ? 1000 : IT->second));
    } else
        T->updateTimeout(std::nullopt);
}

void abyss::TimerHandle::cancel() {
    if (auto* m = M(); m && id)
        m->cancelTimer(id);
    g_timerIntervals.erase(id);
    id = 0;
}

// ------------------------------------------------------------------------------------------------ idle

namespace {
    // shared validation of the idle seconds; nullopt (after err()) when invalid
    std::optional<uint32_t> idleMs(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0.001) {
            err("idle: seconds must be a finite number of at least 0.001");
            return std::nullopt;
        }
        // the protocol counts in uint32 milliseconds
        if (seconds > double(std::numeric_limits<uint32_t>::max()) / 1000.0 - 1.0) {
            err(std::format("idle: {} seconds is too long (maximum is {} seconds)", seconds, std::numeric_limits<uint32_t>::max() / 1000 - 1));
            return std::nullopt;
        }
        return sc<uint32_t>(std::llround(seconds * 1000.0));
    }
}

void abyss::Config::idle(double seconds, std::function<void()> onIdle, std::function<void()> onResume) {
    auto* m = M();
    if (!m)
        return;
    const auto MS = idleMs(seconds);
    if (!MS)
        return;
    if (!onIdle && !onResume) {
        err("idle: needs at least one callback");
        return;
    }

    m->addIdleRule(
        *MS, [f = std::move(onIdle)] { if (f) guarded("idle callback", f); }, [f = std::move(onResume)] { if (f) guarded("idle resume callback", f); });
}

void abyss::Config::idle(double seconds, abyss::Screensaver s) {
    auto* m = M();
    if (!m)
        return;
    const auto MS = idleMs(seconds);
    if (!MS)
        return;
    if (!s.draw) {
        err("idle: screensaver needs a draw callback");
        return;
    }
    if (s.opts.fps < 0 || s.opts.fps > 144) {
        err(std::format("idle: screensaver fps must be in 0..144, got {}", s.opts.fps));
        return;
    }
    if (s.opts.bars < 1 || s.opts.bars > 512) {
        err(std::format("idle: screensaver bars must be in 1..512, got {}", s.opts.bars));
        return;
    }

    auto def  = makeShared<Saver::SDef>();
    def->opts = std::move(s.opts);
    // only the first failure of the callback is reported (it runs every frame); it keeps being called and a failing frame is drawn black
    def->draw = [f = std::move(s.draw), failed = std::make_shared<bool>(false)](abyss::Canvas& g, const abyss::SaverFrame& fr) {
        bool ok = false;
        if (*failed) {
            try {
                f(g, fr);
                ok = true;
            } catch (...) {}
        } else {
            guarded("screensaver callback", [&] {
                f(g, fr);
                ok = true;
            });
            *failed = !ok;
        }
        return ok;
    };
    m->addSaverRule(*MS, std::move(def));
}

// ------------------------------------------------------------------------------------------------ misc

void abyss::Config::permission(std::string binary, std::string typeStr, std::string modeStr) {
    if (binary.empty()) {
        err("permission: binary must not be empty");
        return;
    }

    eDynamicPermissionType      type = PERMISSION_TYPE_UNKNOWN;
    eDynamicPermissionAllowMode mode = PERMISSION_RULE_ALLOW_MODE_UNKNOWN;

    if (typeStr == "screencopy")
        type = PERMISSION_TYPE_SCREENCOPY;
    else if (typeStr == "cursorpos")
        type = PERMISSION_TYPE_CURSOR_POS;
    else if (typeStr == "keyboard" || typeStr == "keeb")
        type = PERMISSION_TYPE_KEYBOARD;
    else if (typeStr == "input-capture")
        type = PERMISSION_TYPE_INPUT_CAPTURE;

    if (modeStr == "ask")
        mode = PERMISSION_RULE_ALLOW_MODE_ASK;
    else if (modeStr == "allow")
        mode = PERMISSION_RULE_ALLOW_MODE_ALLOW;
    else if (modeStr == "deny")
        mode = PERMISSION_RULE_ALLOW_MODE_DENY;

    if (type == PERMISSION_TYPE_UNKNOWN) {
        err(std::format("permission: unknown permission type \"{}\"", typeStr));
        return;
    }
    if (mode == PERMISSION_RULE_ALLOW_MODE_UNKNOWN) {
        err(std::format("permission: unknown mode \"{}\"", modeStr));
        return;
    }

    if (auto* m = M(); m && m->isFirstLaunch() && g_pDynamicPermissionManager)
        g_pDynamicPermissionManager->addConfigPermissionRule(binary, type, mode);
}

bool abyss::setOption(std::string_view name, std::variant<bool, int64_t, double, std::string, Color, Gradient, Vec2, Gap> value) {
    auto* m = M();
    if (!m)
        return false;
    return m->setInput(name, std::visit([](const auto& v) { return detail::toInput(v); }, value));
}

// ------------------------------------------------------------------------------------------------ entry point

void Config::Native::applyUserConfig() {
    g_bindHandles.clear();
    g_timerIntervals.clear();

    abyss::Config cfg;

    try {
        abyss::detail::userConfigure(cfg);
    } catch (const std::exception& e) {
        if (auto* m = M())
            m->addError(std::format("configure() threw: {}", e.what()));
    } catch (...) {
        if (auto* m = M())
            m->addError("configure() threw an unknown exception");
    }

    abyss::applyOptions(cfg);
}
