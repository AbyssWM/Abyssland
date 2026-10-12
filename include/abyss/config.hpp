// Abysslang: the compiled-in configuration API.
//
//   #include <abyss/config.hpp>
//   using namespace abyss;
//   void configure(Config& c) {
//       c.general.gaps_in = 5;
//       c.bind(SUPER, "Return", [] { return exec("kitty"); });
//   }
//
// configure() runs on every config load (start and abyssctl reload). Everything it registers (binds, rules, listeners,
// timers, animations, ...) is cleared before the next load, so nothing is registered twice.
//
// Designated initializers (e.g. {.cls = "x"}, {.locked = true, .repeat = true}) must list their fields in declaration order
// (a C++ rule): the order is the one used in the structs below.
//
// Option fields (c.general.gaps_in, c.input.kb_layout, ...) are generated from the option table, see abyss/Options.hpp.
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <abyss/types.hpp>
#include <abyss/Options.hpp>
#include <abyss/actions.hpp>
#include <abyss/effects.hpp>
#include <abyss/events.hpp>
#include <abyss/layout.hpp>
#include <abyss/protocols.hpp>
#include <abyss/query.hpp>
#include <abyss/saver.hpp>

namespace abyss {

    // ---- binds ----

    struct BindOpts {
        bool                     locked          = false; // also works on the lock screen
        bool                     release         = false; // trigger on key release
        bool                     repeat          = false; // repeat while held
        bool                     longPress       = false;
        bool                     mouse           = false; // marker for mouse binds, requires mouseMove() / mouseResize() as the action
        bool                     nonConsuming    = false; // the key press is also passed to the focused window
        bool                     autoConsuming   = false; // consumed only if the action succeeded
        bool                     transparent     = false; // cannot be shadowed by other binds
        bool                     ignoreMods      = false;
        bool                     dontInhibit     = false; // works even if a client inhibits shortcuts
        bool                     click           = false; // mouse bind: only on click (no drag)
        bool                     drag            = false; // mouse bind: only on drag
        bool                     submapUniversal = false; // active in every submap
        bool                     allowInputCapture = false;
        std::string              description;
        std::vector<std::string> devices;          // restrict to (or, with devicesInclusive = false, exclude) these device tags
        bool                     devicesInclusive = true;
    };

    // returned by Config::bind
    // A disabled bind stays registered but does not trigger. A reload drops every bind (and so invalidates the handle).
    struct BindHandle {
        uint64_t id = 0; // internal
        void     setEnabled(bool enabled) const;
        bool     isEnabled() const; // false when the bind was removed or dropped by a reload
        void     remove();          // unregisters exactly this bind (other binds on the same key stay); the handle is dead afterwards
    };

    // ---- rules ----

    // a regex (or "true"/"false" for the boolean ones) per field; unset fields do not take part in matching.
    // Regexes are the same as in the old rule syntax, e.g. "^(kitty)$" or "negative:..." prefixes.
    struct WindowMatch {
        std::optional<std::string> cls;           // class
        std::optional<std::string> title;
        std::optional<std::string> initialClass;
        std::optional<std::string> initialTitle;
        std::optional<bool>        floating;
        std::optional<std::string> tag;
        std::optional<bool>        xwayland;
        std::optional<bool>        fullscreen;
        std::optional<bool>        pinned;
        std::optional<bool>        focus;
        std::optional<bool>        group;
        std::optional<bool>        modal;
        std::optional<std::string> workspace;      // workspace selector, e.g. "1" or "name:web"
        std::optional<std::string> content;
        std::optional<std::string> xdgTag;
        std::optional<int>         fullscreenStateInternal;
        std::optional<int>         fullscreenStateClient;
        // anything not covered above: {match property name, value}
        std::vector<std::pair<std::string, std::string>> extra;
    };

    struct WindowEffects {
#define ABYSS_X(f, n, T) std::optional<T> f;
        ABYSS_WINDOW_EFFECTS(ABYSS_X)
#undef ABYSS_X
        // effects by rule name, e.g. {{"float", "true"}}
        std::vector<std::pair<std::string, std::string>> extra;
    };

    struct LayerMatch {
        std::optional<std::string> nspace; // layer namespace regex ("namespace" is a C++ keyword)
        std::vector<std::pair<std::string, std::string>> extra;
    };

    struct LayerEffects {
#define ABYSS_X(f, n, T) std::optional<T> f;
        ABYSS_LAYER_EFFECTS(ABYSS_X)
#undef ABYSS_X
        std::vector<std::pair<std::string, std::string>> extra;
    };

    struct WorkspaceRule {
        std::optional<std::string> monitor;
        std::optional<bool>        isDefault;
        std::optional<bool>        persistent;
        std::optional<Gap>         gapsIn;
        std::optional<Gap>         gapsOut;
        std::optional<Gap>         floatGaps;
        std::optional<int64_t>     borderSize;
        std::optional<bool>        noBorder;
        std::optional<bool>        noRounding;
        std::optional<bool>        decorate;
        std::optional<bool>        noShadow;
        std::optional<std::string> onCreatedEmpty; // command
        std::optional<std::string> defaultName;
        std::optional<std::string> layout;
        std::optional<std::string> animation;
        std::map<std::string, std::string> layoutOpts;
        // false registers the rule disabled (see RuleHandle). Like in Lua, an enabled rule for the same workspace string is merged into instead
        // of adding a new one, so a disabled rule is only created when none is enabled yet, or switch it with the handle.
        bool                               enabled = true;
    };

    // Returned by Config::windowRule / layerRule / workspaceRule. A disabled rule stays registered but is ignored; switching it re-applies
    // the rules to the existing windows / layers / workspaces right away. A reload drops every rule (and so invalidates the handle).
    struct RuleHandle {
        uint64_t id = 0; // internal
        void     setEnabled(bool enabled) const;
        bool     isEnabled() const; // false when the rule is gone (reload)
    };

    // ---- monitors ----

    struct MonitorOpts {
        std::optional<std::string> mirror;      // name of the monitor to mirror
        std::optional<int>         transform;   // 0..7
        std::optional<int>         bitdepth;    // 8 or 10
        std::optional<std::string> cm;          // color management preset
        std::optional<int>         vrr;         // -1 (unset) .. 3
        std::optional<Gap>         reserved;    // reserved area top/right/bottom/left
        std::optional<std::string> icc;
        std::optional<std::string> sdrEotf;
        std::optional<double>      sdrBrightness;
        std::optional<double>      sdrSaturation;
        std::optional<int>         supportsWideColor;
        std::optional<int>         supportsHdr;
        std::optional<double>      sdrMinLuminance;
        std::optional<int>         sdrMaxLuminance;
        std::optional<double>      minLuminance;
        std::optional<int>         maxLuminance;
        std::optional<int>         maxAvgLuminance;
    };

    // ---- devices ----

    struct DeviceOpts {
#define ABYSS_X(f, n, T) std::optional<T> f;
        ABYSS_DEVICE_FIELDS(ABYSS_X)
#undef ABYSS_X
    };

    // ---- gestures ----

    struct GestureOpts {
        std::string mods;           // e.g. "SUPER" or "SUPER+SHIFT"
        double      scale          = 1.0;
        bool        disableInhibit = false;
        std::string workspaceName;  // for action "special"
        std::string zoomLevel;      // for action "cursor_zoom"
        std::string mode;           // for "float" / "fullscreen" / "cursor_zoom"
    };

    // event handed to a callback gesture (Config::gesture with GestureCallbacks)
    struct GestureEvent {
        enum class Phase { Start, Update, End };
        Phase       phase = Phase::Start;
        std::string direction;       // "HORIZONTAL" "VERTICAL" "LEFT" "RIGHT" "UP" "DOWN" "SWIPE" "PINCH" "PINCH_IN" "PINCH_OUT" (the actual direction, "NONE" if unknown)
        std::string type;            // "swipe" or "pinch"
        uint32_t    timeMs = 0;
        int         fingers = 0;     // Start / Update only
        Vec2        delta;           // Start / Update only
        double      scale    = 1.0;  // Start / Update of a pinch only
        double      rotation = 0.0;  // Start / Update of a pinch only
        bool        cancelled = false; // End only
    };

    // the three phases of a callback gesture; at least one must be set. Start fires once the direction was recognised (the first update that
    // matched), Update on every following update, Finish when the fingers are lifted (GestureEvent::cancelled tells if the gesture was cancelled).
    struct GestureCallbacks {
        std::function<void(const GestureEvent&)> start;
        std::function<void(const GestureEvent&)> update;
        std::function<void(const GestureEvent&)> finish;
    };

    // ---- timers ----

    struct TimerHandle {
        uint64_t id = 0; // internal
        void     setEnabled(bool enabled) const; // re-arms with the original interval / disarms
        void     cancel();                       // removes the timer
        bool     isEnabled() const;              // armed: a oneshot timer is not enabled any more after it fired, a cancelled / reloaded one never is
        void     setTimeout(int ms) const;       // new interval in ms (>= 1); arms the timer to fire after it, also for repeat (the new interval is kept)
    };

    // ---- the config object ----

    class Config : public Options {
      public:
        // --- environment / autostart ---
        // Sets an environment variable for the compositor and everything it starts. dbus = also import into the dbus activation environment.
        void env(std::string name, std::string value, bool dbus = false);
        // starts a command after the compositor started, on every config load
        void exec(std::string cmd);
        // starts a command after the compositor started, only on the very first load of this session
        void execOnce(std::string cmd);
        // starts a command when the compositor shuts down (registered on the first load only)
        void execShutdown(std::string cmd);
        // runs fn once per compositor process, when the compositor has started (so e.g. exec("quickshell") inside it starts quickshell only
        // once). An in-process reload (`abyssctl reload --no-build`) does NOT run it again. Use exec() inside a callback for immediate execution.
        void onStart(std::function<void()> fn);
        void onShutdown(std::function<void()> fn);

        // --- monitors / workspaces ---
        // name: output name (e.g. "DP-1"), "desc:..." or "" for every monitor without a rule of its own. mode: "preferred" | "highres" | "highrr" | "WxH" | "WxH@Hz"; position: "auto" | "auto-left" | "XxY" ...; scale: factor or "auto"
        void monitor(std::string name, std::string mode = "preferred", std::string position = "auto", double scale = 1.0, const MonitorOpts& opts = {});
        void monitor(std::string name, std::string mode, std::string position, std::string scale, const MonitorOpts& opts = {});
        void disableMonitor(std::string name);
        // rule.enabled = false registers it disabled, see WorkspaceRule / RuleHandle
        RuleHandle workspaceRule(std::string workspace, const WorkspaceRule& rule);
        void device(std::string name, const DeviceOpts& opts);

        // --- window / layer rules ---
        // A rule with a name is updated when one with the same name was already registered in this config load (the match properties and
        // effects of the new call are added to it, and `enabled` is set again), so a name addresses one rule. Without a name every call
        // adds a new rule. enabled = false registers (or switches) the rule disabled; use the handle to toggle it later.
        RuleHandle windowRule(const WindowMatch& match, const WindowEffects& effects, std::string name = "", bool enabled = true);
        RuleHandle layerRule(const LayerMatch& match, const LayerEffects& effects, std::string name = "", bool enabled = true);

        // --- animations ---
        // bezier curve with control points (x0,y0) (x1,y1), each coordinate in [-1, 2]
        void bezier(std::string name, double x0, double y0, double x1, double y1);
        // spring curve, mass / stiffness / damping all >= 0.5
        void spring(std::string name, double mass, double stiffness, double damping);
        // leaf: "global", "windows", "windowsIn", "fade", "workspaces", ... ; curve: name of a bezier or a spring
        void animation(std::string leaf, bool enabled, double speed = 1.0, std::string curve = "default", std::string style = "");

        // --- binds ---
        // c.bind(SUPER | SHIFT, "Q", killActive());   key: xkb key name, "code:N", "mouse:N", "mouse_down", "switch:...", "catchall" (submaps only)
        BindHandle bind(Mods mods, std::string key, Action action, const BindOpts& opts = {});
        // same with a combined string: c.bind("SUPER+SHIFT+Q", killActive());
        BindHandle bind(std::string combo, Action action, const BindOpts& opts = {});
        void       unbind(Mods mods, std::string key);
        void       unbind(std::string combo);
        void       unbindAll();
        // binds registered inside fn belong to the submap `name`; leave it with bind(..., submap("reset")).
        // reset: if non-empty, every bind in the submap switches to that submap (usually "reset") after it ran
        void       submap(std::string name, const std::function<void(Config&)>& fn, std::string reset = "");

        // --- gestures ---
        // action: "workspace" | "move" | "resize" | "special" | "close" | "float" | "fullscreen" | "cursor_zoom" | "scroll_move" | "unset"
        // direction: "horizontal" | "vertical" | "left" | "right" | "up" | "down" | "swipe" | "pinch" ...
        void gesture(int fingers, std::string direction, std::string action, const GestureOpts& opts = {});
        // callback gestures. Only the plain function: runs once when the gesture ends, without arguments (also if it was cancelled):
        //   c.gesture(3, "down", [] { exec("grim"); });
        void gesture(int fingers, std::string direction, std::function<void()> onEnd, const GestureOpts& opts = {});
        // start / update / finish callbacks with a GestureEvent; at least one of them must be set (err() otherwise):
        //   c.gesture(4, "swipe", GestureCallbacks{.update = [](const GestureEvent& e) { ... e.delta.x ... }});
        // Exceptions in callbacks are caught and logged. Opts (mods, scale, disableInhibit) work as for the named actions; workspaceName / zoomLevel / mode are ignored.
        void gesture(int fingers, std::string direction, GestureCallbacks callbacks, const GestureOpts& opts = {});

        // --- events / timers ---
        // c.on(Event::WindowOpen, [](PHLWINDOW w) { ... });  -- see abyss/events.hpp
        template <typename Tag, typename F>
        EventHandle on(Tag, F f) {
            return EventHandle{detail::registerEvent(Tag::id, std::any(Tag::adapt(std::move(f))))};
        }
        // calls fn after `ms` milliseconds (and every `ms` milliseconds when repeat is set). enabled = false creates it disarmed:
        // TimerHandle::setEnabled(true) (or setTimeout) starts it later.
        TimerHandle timer(int ms, std::function<void()> fn, bool repeat = false, bool enabled = true);
        // Native idle rule: onIdle runs once when there was no input for `seconds` (keyboard, pointer, touch, tablet), onResume runs on the
        // first input after that (never without a previous onIdle). Active idle inhibitors (video players, idle-inhibit windows rules) block
        // it, and when they go away the whole timeout counts again. Nothing is registered by default: there is no built-in lock, suspend or helper,
        // do that in the callbacks (c.idle(300, [] { exec("abysslock"); });). seconds: finite, at least 0.001, at most ~4294966 (uint32 ms); onResume may be empty.
        // The rule is registered once the compositor started, a reload (`abyssctl reload --no-build`) drops and re-creates it, it never stacks up.
        // Callbacks run on the compositor thread: keep them short, exec() for anything slow. Exceptions are caught and logged. Note that the
        // forceIdle action bypasses inhibitors (as for the ext-idle-notify protocol).
        void idle(double seconds, std::function<void()> onIdle, std::function<void()> onResume = {});
        // Screensaver: the same idle rule (same seconds, inhibitor and reload semantics) shows `s` fullscreen on every monitor on idle, above everything
        // including the session lock, and hides it again on the first input, which is swallowed (it only wakes up, e.g. a click or key does not reach the
        // window or the lock screen below; moving the mouse also wakes up). Lock at the same time and the lock screen appears when you wake it up:
        // c.idle(300, screensaver(saver::clock())); c.idle(300, [] { exec("abysslock"); }); The cursor is hidden meanwhile. See abyss/saver.hpp: c.idle(300, screensaver(saver::clock()));
        // Other idle rules (lock, dpms via exec) still run next to it. A reload ends a running screensaver. `abyssctl dispatch screensaver` shows
        // the first configured one right away (see showScreensaver()). Nothing runs (no timer, no process) while no screensaver is shown.
        void idle(double seconds, Screensaver s);

        // --- layouts (see abyss/layout.hpp) ---
        // simple layout: fn places the windows. Selected by name: c.general.layout = "grid", a workspace rule, or `abyssctl dispatch setlayout grid`
        void addLayout(std::string name, LayoutFn fn, LayoutHooks hooks = {});
        // full-power layout: A derives from abyss::TiledLayout and is default-constructible; one instance per workspace
        template <typename A>
        void addLayout(std::string name) {
            static_assert(std::is_base_of_v<TiledLayout, A>, "custom layouts must derive from abyss::TiledLayout");
            detail::registerLayout(std::move(name), [] { return UP<TiledLayout>(new A()); });
        }

        // --- misc ---
        // permission rule, e.g. permission("/usr/bin/grim", "screencopy", "allow"); type: screencopy | cursorpos | keyboard | input-capture; mode: allow | ask | deny
        void permission(std::string binary, std::string type, std::string mode);
        // sets an option by its string name; same checks as the typed fields (c.set("general:gaps_in", 5))
        template <typename T>
        void set(std::string_view name, T&& v) {
            abyss::set(name, std::forward<T>(v));
        }

        // --- GPU / backend (typed form of the ABYSSAL_* variables, see https://abysswm.github.io/wiki/nvidia) ---
        // Read once at start, before the backend exists; a reload that changes them only warns ("takes effect after a restart").
        // Nothing set = nothing touched (variables from the login environment stay as they are).
        struct GpuOpts {
            // DRM devices to use, first = primary (ABYSSAL_DRM_DEVICES). Paths that do not exist are skipped, so a list written for
            // one machine does not break another; if none exists the variable is left alone and the backend picks automatically.
            // Example for "render on the AMD card, NVIDIA only for CUDA": {"/dev/dri/by-path/pci-0000:0b:00.0-card"}.
            std::vector<std::string> devices;
            bool                     noModifiers    = false; // ABYSSAL_NO_MODIFIERS: no explicit buffer modifiers (old drivers)
            bool                     noExplicitMgpu = false; // ABYSSAL_MGPU_NO_EXPLICIT: no explicit sync between GPUs
            bool                     noAtomic       = false; // ABYSSAL_NO_ATOMIC: legacy KMS instead of atomic modesetting
            bool                     eglNoModifiers = false; // ABYSSLAND_EGL_NO_MODIFIERS: renderer ignores modifiers
        } gpu;

        // --- protocols (see abyss/protocols.hpp): everything is offered by default ---
        Protocols protocols;
        void      protocol(std::string name, bool enabled) { protocols.set(std::move(name), enabled); }
        void      protocol(std::string name, ProtocolOpts opts) { protocols.set(std::move(name), opts); }

        // internal: used by the compositor
        std::string m_currentSubmap, m_currentSubmapReset;
    };

    namespace detail {
        // defined in UserConfig.cpp: calls the user's `void configure(abyss::Config&)`
        void userConfigure(Config& c);
    }

} // namespace abyss
