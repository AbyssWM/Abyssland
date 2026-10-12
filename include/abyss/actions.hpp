// Abysslang actions: things a keybind / callback can do, plus small query helpers.
//
// Every action below returns an abyss::Action: a deferred, callable value.
//   c.bind(SUPER, "Q", killActive());          // pass it to a bind
//   run(killActive());                         // run it right now (inside a callback)
//   killActive()();                            // same thing
// Action is [[nodiscard]], so a forgotten run() shows up as a warning instead of silently doing nothing.
// Exception: exec() runs immediately (it is meant for callbacks, e.g. c.onStart([] { exec("waybar"); })).
//
// Most actions are string-backed: they go through the same dispatcher table as `abyssctl dispatch <name> <arg>`
// (all 72 dispatchers are reachable through dispatch(name, arg)). Binds made from them behave exactly like
// the classic string binds (release handling, mouse binds, submap switching, global shortcuts ...).
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "SharedDefs.hpp"
#include "desktop/DesktopTypes.hpp"

#include <abyss/types.hpp>

namespace Notification {
    class CNotification;
}

namespace abyss {

    inline SDispatchResult ok() {
        return {};
    }
    inline SDispatchResult error(std::string msg) {
        return {.success = false, .error = std::move(msg)};
    }

    enum class Dir {
        Left,
        Right,
        Up,
        Down
    };

    struct [[nodiscard]] Action {
        std::string                      handler; // dispatcher name ("exec", "workspace", ...); "native" for C++ callbacks
        std::string                      arg;
        std::function<SDispatchResult()> fn; // set for C++ callbacks

        Action() = default;
        Action(std::string h, std::string a) : handler(std::move(h)), arg(std::move(a)) {}

        // any callable returning void or SDispatchResult, e.g. [] { return ok(); }
        template <typename F>
            requires(std::is_invocable_v<F&> && !std::is_same_v<std::decay_t<F>, Action>)
        Action(F f) : handler("native") {
            if constexpr (std::is_void_v<std::invoke_result_t<F&>>)
                fn = [f = std::move(f)]() mutable -> SDispatchResult {
                    f();
                    return {};
                };
            else
                fn = [f = std::move(f)]() mutable -> SDispatchResult { return f(); };
        }

        bool            isNative() const {
            return static_cast<bool>(fn);
        }
        SDispatchResult operator()() const;
    };

    // runs an action now
    inline SDispatchResult run(const Action& a) {
        return a();
    }

    // ---- generic ----

    // any dispatcher by name, same arguments as `abyssctl dispatch <name> <arg>`
    Action          dispatch(std::string name, std::string arg = "");
    // runs immediately through /bin/sh -c; supports "[float; size 800 600] kitty" exec rules
    // use a callback to defer execution: c.bind(SUPER, "Q", [] { return exec("kitty"); });
    SDispatchResult exec(const std::string& cmd);

    // ---- windows (selector: "" = active window, otherwise a window selector like "class:^(kitty)$", "address:0x...", "title:...") ----

    Action killActive();
    Action forceKillActive();
    Action closeWindow(std::string selector = "");
    Action killWindow(std::string selector);
    Action signal(int sig);
    // sends a signal to the selected window's process (signalwindow)
    Action signalWindow(int sig, std::string selector = "");
    Action toggleFloating(std::string selector = "");
    Action setFloating(std::string selector = "");
    Action setTiled(std::string selector = "");
    Action pseudo(std::string selector = "");
    Action pin(std::string selector = "");
    // mode 0 = fullscreen, 1 = maximize; state "toggle" | "set" | "unset"
    Action fullscreen(int mode = 0, std::string state = "toggle");
    // same with a target window and layoutAware (false = the layout is not told, the window just covers the monitor)
    Action fullscreen(int mode, std::string state, bool layoutAware, std::string selector = "");
    Action fullscreenState(int internal, int client);
    Action moveFocus(Dir d);
    Action moveWindow(Dir d, bool silent = false, std::string selector = "");
    Action moveWindowToMonitor(std::string monitor, bool silent = false);
    Action moveWindowOrGroup(Dir d, std::string selector = "");
    Action swapWindow(Dir d, std::string selector = "");
    Action swapWith(std::string selector);
    Action swapNext(bool forward = true, std::string selector = "");
    Action centerWindow(std::string selector = "");
    // relative or exact pixel arguments in the usual syntax: "10 0", "exact 800 600", "-20 -20"
    Action resizeActive(std::string args);
    Action moveActive(std::string args);
    Action resizeActive(int dx, int dy);
    Action moveActive(int dx, int dy);
    Action resizeWindowPixel(std::string args, std::string selector);
    Action moveWindowPixel(std::string args, std::string selector);
    Action cycleNext(bool forward = true, bool onlyTiled = false, bool onlyFloating = false);
    Action focusWindow(std::string selector);
    Action focusUrgentOrLast();
    Action focusCurrentOrLast();
    Action bringActiveToTop();
    Action alterZOrder(std::string position /* "top" | "bottom" */, std::string selector = "");
    Action tag(std::string tag /* "+name", "-name", "name" toggles */, std::string selector = "");
    // removes every tag (also the dynamic ones from rules) from the window
    Action clearTags(std::string selector = "");
    Action toggleSwallow();
    Action setProp(std::string selector, std::string prop, std::string value);
    Action mouseMove();                                // for bind(SUPER, "mouse:272", mouseMove())
    Action mouseResize(bool keepAspectRatio = false);  // for bind(SUPER, "mouse:273", mouseResize())

    // A string literal converts to bool before std::string, so moveWindow(Dir::Left, "class:x") would silently mean silent = true.
    // These overloads turn that mistake into a compile error; pass the bool explicitly: moveWindow(Dir::Left, false, "class:x").
    Action moveWindow(Dir, const char*)                  = delete;
    Action moveWindowToMonitor(std::string, const char*) = delete;
    Action swapNext(const char*)                         = delete;
    Action cycleNext(bool, const char*)                  = delete;
    Action cycleNext(const char*)                        = delete;
    Action moveToWorkspace(int, const char*)             = delete;
    Action moveToWorkspace(std::string, const char*)     = delete;
    Action fullscreen(int, const char*, const char*)     = delete;

    // ---- groups ----

    Action toggleGroup(std::string selector = "");
    Action changeGroupActive(bool forward = true, std::string selector = "");
    // makes the index-th window of the group active (1-based, 0 = last)
    Action setGroupActive(int index, std::string selector = "");
    Action moveGroupWindow(bool forward = true);
    Action changeGroupActive(const char*) = delete; // see above
    Action moveIntoGroup(Dir d, std::string selector = "");
    Action moveIntoOrCreateGroup(Dir d, std::string selector = "");
    Action moveOutOfGroup(std::string selector = "");
    Action lockGroups(std::string mode = "toggle" /* lock | unlock | toggle */);
    Action lockActiveGroup(std::string mode = "toggle");
    Action denyWindowFromGroup(std::string mode = "toggle" /* on | off | toggle */);

    // ---- workspaces / monitors ----

    Action workspace(int id);
    // "3", "name:web", "e+1", "e-1", "+1", "special:scratch", "previous", "empty" ...
    Action workspace(std::string name);
    Action moveToWorkspace(int id, bool silent = false, std::string selector = "");
    Action moveToWorkspace(std::string name, bool silent = false, std::string selector = "");
    Action toggleSpecial(std::string name = "");
    Action moveToSpecial(std::string name = "");
    Action renameWorkspace(int id, std::string name);
    // gives workspace `from` the id `to` (fails if `to` exists)
    Action changeWorkspaceId(int64_t from, int64_t to);
    Action focusMonitor(std::string monitor /* name, id or direction like "+1" / "l" */);
    Action focusWorkspaceOnCurrentMonitor(std::string workspace);
    Action moveCurrentWorkspaceToMonitor(std::string monitor);
    Action moveWorkspaceToMonitor(std::string workspace, std::string monitor);
    Action swapActiveWorkspaces(std::string monitor1, std::string monitor2);
    Action dpms(std::string mode /* on | off | toggle */, std::string monitor = "");
    Action moveCursorToCorner(int corner /* 0..3 */, std::string selector = "");
    Action moveCursor(int x, int y);

    // ---- layout ----

    Action layoutMsg(std::string msg);
    // switch the tiling layout of every workspace that has no layout in a workspace rule (same as `abyssctl dispatch setlayout <name>`)
    Action setLayout(std::string name);
    Action toggleSplit();
    Action swapSplit();

    // ---- misc ----

    // switch the keybind submap ("reset" leaves it)
    Action submap(std::string name);
    // pass the currently pressed key to a window
    Action pass(std::string selector);
    Action sendShortcut(Mods mods, std::string key, std::string selector = "");
    Action sendKeyState(Mods mods, std::string key, std::string state /* down | repeat | up */, std::string selector = "");
    Action global(std::string name /* "app:name" */);
    Action event(std::string data);
    Action forceRendererReload();
    Action forceIdle(float seconds);
    // shows the first screensaver registered with c.idle(seconds, screensaver(...)) now; the next input ends it (does nothing without one)
    Action showScreensaver();
    Action releaseInputCapture();
    Action exit();
    // does nothing (a bind that only swallows the key, like hl.dsp.no_op)
    Action noop();

    // ---- queries ----
    // Window / workspace / monitor handles are the compositor's own shared pointers; use the accessors below to look inside.

    PHLWINDOW              activeWindow();
    PHLWINDOW              windowByAddress(std::string_view address /* "0x..." */);
    PHLWINDOW              windowBySelector(std::string_view selector);
    std::vector<PHLWINDOW> windows();
    PHLWORKSPACE           activeWorkspace();
    PHLWORKSPACE           workspaceById(int64_t id);
    std::vector<PHLWORKSPACE> workspaces();
    PHLMONITOR             activeMonitor();
    PHLMONITOR             monitorByName(std::string_view name);
    std::vector<PHLMONITOR> monitors();

    std::string            windowClass(const PHLWINDOW& w);
    std::string            windowTitle(const PHLWINDOW& w);
    std::string            windowInitialClass(const PHLWINDOW& w);
    std::string            windowAddress(const PHLWINDOW& w); // "0x..." as printed by abyssctl
    int                    windowPid(const PHLWINDOW& w);
    bool                   windowFloating(const PHLWINDOW& w);
    bool                   windowFullscreen(const PHLWINDOW& w);
    int64_t                windowWorkspaceId(const PHLWINDOW& w);
    std::string            windowMonitorName(const PHLWINDOW& w);
    // selector string for the action functions: focusWindow(byAddress(w))
    std::string            byAddress(const PHLWINDOW& w);

    int64_t                workspaceId(const PHLWORKSPACE& w);
    int64_t                workspaceId(const PHLWORKSPACEREF& w);
    std::string            workspaceName(const PHLWORKSPACE& w);
    std::string            workspaceName(const PHLWORKSPACEREF& w);
    int                    workspaceWindowCount(const PHLWORKSPACE& w);
    std::string            monitorName(const PHLMONITOR& m);

    // ---- desktop notification overlay ----

    struct NotifyOpts {
        int                  ms       = 5000;
        std::optional<Color> color    = std::nullopt; // default: the icon's colour
        std::string          icon     = "info";       // info | warning | error | hint | ok | confused | none
        float                fontSize = 13.f;
    };

    // a shown notification. Copyable; every call does nothing once the notification is gone (timed out or dismissed).
    class NotificationHandle {
      public:
        NotificationHandle() = default;
        explicit NotificationHandle(WP<Notification::CNotification> n) : m_n(std::move(n)) {}

        bool        alive() const;
        std::string text() const;
        void        setText(std::string text);
        void        setColor(Color c);
        void        setIcon(const std::string& icon);
        void        setFontSize(float size);
        // restarts the countdown with a new duration
        void        setTimeout(int ms);
        // stops / restarts the countdown (calls must be balanced)
        void        pause();
        void        resume();
        void        dismiss();

      private:
        WP<Notification::CNotification> m_n;
    };

    NotificationHandle              notify(const std::string& text, int ms = 5000, std::optional<Color> color = std::nullopt, const std::string& icon = "info" /* info|warning|error|hint|ok|confused|none */);
    NotificationHandle              notify(const std::string& text, const NotifyOpts& opts);
    // every notification currently shown, oldest first
    std::vector<NotificationHandle> notifications();
    // dismisses the oldest `amount` notifications, -1 = all
    void                            dismissNotifications(int amount = -1);

    // changes an option at runtime, e.g. set("general:gaps_in", 3); returns false (and notifies) on unknown option / bad value
    bool                   setOption(std::string_view name, std::variant<bool, int64_t, double, std::string, Color, Gradient, Vec2, Gap> value);
    template <typename T>
    bool set(std::string_view name, T&& v) {
        using D = std::decay_t<T>;
        if constexpr (std::is_same_v<D, bool>)
            return setOption(name, v);
        else if constexpr (std::is_integral_v<D>)
            return setOption(name, int64_t(v));
        else if constexpr (std::is_floating_point_v<D>)
            return setOption(name, double(v));
        else if constexpr (std::is_convertible_v<D, std::string_view>)
            return setOption(name, std::string(std::string_view(v)));
        else
            return setOption(name, D(std::forward<T>(v)));
    }

    // reads an option at runtime (counterpart of hl.get_config). Simple types and gaps come back as they are, the other complex ones
    // (gradients, font weights, ...) as their config string. nullopt = no such option.
    using OptionValue = std::variant<bool, int64_t, double, std::string, Vec2, Gap>;
    std::optional<OptionValue> getOption(std::string_view name);
    // true when the option was set by the config (or set()), false when it still has its default or does not exist
    bool                       optionSetByUser(std::string_view name);
    // typed read: get<int>("general:border_size"), get<bool>(...), get<double>(...), get<std::string>(...), get<Vec2>(...), get<Gap>(...).
    // nullopt when the option does not exist or has a different type (int <-> double <-> bool convert, a gap with four equal sides reads
    // as a number, anything -> std::string works).
    template <typename T>
    std::optional<T> get(std::string_view name) {
        auto v = getOption(name);
        if (!v)
            return std::nullopt;
        if constexpr (std::is_same_v<T, std::string>) {
            return std::visit(
                [](const auto& x) -> std::string {
                    using X = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<X, std::string>)
                        return x;
                    else if constexpr (std::is_same_v<X, bool>)
                        return x ? "true" : "false";
                    else if constexpr (std::is_same_v<X, Vec2>)
                        return std::to_string(x.x) + " " + std::to_string(x.y);
                    else if constexpr (std::is_same_v<X, Gap>)
                        return std::to_string(x.top) + " " + std::to_string(x.right) + " " + std::to_string(x.bottom) + " " + std::to_string(x.left);
                    else
                        return std::to_string(x);
                },
                *v);
        } else if constexpr (std::is_same_v<T, Vec2> || std::is_same_v<T, Gap>) {
            if (auto* p = std::get_if<T>(&*v))
                return *p;
            return std::nullopt;
        } else if constexpr (std::is_arithmetic_v<T>) {
            if (auto* p = std::get_if<int64_t>(&*v))
                return T(*p);
            if (auto* p = std::get_if<double>(&*v))
                return T(*p);
            if (auto* p = std::get_if<bool>(&*v))
                return T(*p);
            if (auto* p = std::get_if<Gap>(&*v); p && p->top == p->right && p->top == p->bottom && p->top == p->left)
                return T(p->top);
            return std::nullopt;
        } else {
            static_assert(std::is_same_v<T, void>, "abyss::get<T>: T must be a number, bool, std::string, Vec2 or Gap");
        }
    }

} // namespace abyss
