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
    struct BindHandle {
        uint64_t id = 0; // internal
        void     setEnabled(bool enabled) const;
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

    // ---- timers ----

    struct TimerHandle {
        uint64_t id = 0; // internal
        void     setEnabled(bool enabled) const; // re-arms with the original interval / disarms
        void     cancel();                       // removes the timer
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
        void workspaceRule(std::string workspace, const WorkspaceRule& rule);
        void device(std::string name, const DeviceOpts& opts);

        // --- window / layer rules ---
        void windowRule(const WindowMatch& match, const WindowEffects& effects, std::string name = "");
        void layerRule(const LayerMatch& match, const LayerEffects& effects, std::string name = "");

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

        // --- events / timers ---
        // c.on(Event::WindowOpen, [](PHLWINDOW w) { ... });  -- see abyss/events.hpp
        template <typename Tag, typename F>
        EventHandle on(Tag, F f) {
            return EventHandle{detail::registerEvent(Tag::id, std::any(Tag::adapt(std::move(f))))};
        }
        // calls fn after `ms` milliseconds (and every `ms` milliseconds when repeat is set)
        TimerHandle timer(int ms, std::function<void()> fn, bool repeat = false);

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

        // internal: used by the compositor
        std::string m_currentSubmap, m_currentSubmapReset;
    };

    namespace detail {
        // defined in UserConfig.cpp: calls the user's `void configure(abyss::Config&)`
        void userConfigure(Config& c);
    }

} // namespace abyss
