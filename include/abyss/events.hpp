// Abysslang events: c.on(Event::WindowOpen, [](PHLWINDOW w) { ... });
//
// The tags live in the global `Event` namespace (next to the compositor's own event bus) so that they can be written
// as Event::WindowOpen even with `using namespace abyss;`.
// A callback may take fewer arguments than the event provides (down to none): c.on(Event::WorkspaceActive, [] { ... });
#pragma once

#include <any>
#include <functional>
#include <string>
#include <type_traits>

#include "desktop/DesktopTypes.hpp"

namespace abyss {

    enum class EventId : int {
        Start,
        Shutdown,
        WindowOpen,
        WindowOpenEarly,
        WindowClose,
        WindowActive,
        WindowUrgent,
        WindowTitle,
        WindowClass,
        WindowPin,
        WindowFullscreen,
        WindowFloating,
        WindowUpdateRules,
        WindowMoveToWorkspace,
        LayerOpened,
        LayerClosed,
        MonitorAdded,
        MonitorRemoved,
        MonitorFocused,
        MonitorLayoutChanged,
        WorkspaceActive,
        WorkspaceSpecialActive,
        WorkspaceCreated,
        WorkspaceRemoved,
        WorkspaceMoveToMonitor,
        ConfigReloaded,
        PropsRefreshed,
        Submap,
        ScreenshareState,
        KeyboardKey,
    };

    template <EventId E, typename... Args>
    struct EventTag {
        using Fn                    = std::function<void(Args...)>;
        static constexpr EventId id = E;

        // wraps a callable that takes all, or fewer (down to none), of the event arguments
        template <typename F>
        static Fn adapt(F f) {
            return [f = std::move(f)](Args... a) mutable {
                if constexpr (std::is_invocable_v<F&, Args...>)
                    f(a...);
                else
                    f();
            };
        }
    };

    namespace detail {
        // implemented in the compositor: registers a std::function<void(Args...)> (type-erased in `fn`) for the event, returns a handle id
        uint64_t registerEvent(EventId id, std::any fn);
        void     cancelEvent(uint64_t handle);
    }

    // Returned by Config::on(). Config reload removes all listeners anyway; cancel() is for removing one early.
    struct EventHandle {
        uint64_t id = 0;
        void     cancel() {
            if (id)
                detail::cancelEvent(id);
            id = 0;
        }
    };

}

namespace Event {
    using abyss::EventId;
    using abyss::EventTag;

    inline constexpr EventTag<EventId::Start>                                                  Start{};
    inline constexpr EventTag<EventId::Shutdown>                                               Shutdown{};
    // window events (a window is "open" once it is mapped and its rules were applied)
    inline constexpr EventTag<EventId::WindowOpen, PHLWINDOW>                                  WindowOpen{};
    inline constexpr EventTag<EventId::WindowOpenEarly, PHLWINDOW>                             WindowOpenEarly{};
    inline constexpr EventTag<EventId::WindowClose, PHLWINDOW>                                 WindowClose{};
    // second argument: focus reason (int)
    inline constexpr EventTag<EventId::WindowActive, PHLWINDOW, int>                           WindowActive{};
    inline constexpr EventTag<EventId::WindowUrgent, PHLWINDOW>                                WindowUrgent{};
    inline constexpr EventTag<EventId::WindowTitle, PHLWINDOW>                                 WindowTitle{};
    inline constexpr EventTag<EventId::WindowClass, PHLWINDOW>                                 WindowClass{};
    inline constexpr EventTag<EventId::WindowPin, PHLWINDOW>                                   WindowPin{};
    inline constexpr EventTag<EventId::WindowFullscreen, PHLWINDOW>                            WindowFullscreen{};
    inline constexpr EventTag<EventId::WindowFloating, PHLWINDOW>                              WindowFloating{};
    inline constexpr EventTag<EventId::WindowUpdateRules, PHLWINDOW>                           WindowUpdateRules{};
    inline constexpr EventTag<EventId::WindowMoveToWorkspace, PHLWINDOW, PHLWORKSPACE>         WindowMoveToWorkspace{};
    inline constexpr EventTag<EventId::LayerOpened, PHLLS>                                     LayerOpened{};
    inline constexpr EventTag<EventId::LayerClosed, PHLLS>                                     LayerClosed{};
    inline constexpr EventTag<EventId::MonitorAdded, PHLMONITOR>                               MonitorAdded{};
    inline constexpr EventTag<EventId::MonitorRemoved, PHLMONITOR>                             MonitorRemoved{};
    inline constexpr EventTag<EventId::MonitorFocused, PHLMONITOR>                             MonitorFocused{};
    inline constexpr EventTag<EventId::MonitorLayoutChanged>                                   MonitorLayoutChanged{};
    inline constexpr EventTag<EventId::WorkspaceActive, PHLWORKSPACE>                          WorkspaceActive{};
    inline constexpr EventTag<EventId::WorkspaceSpecialActive, PHLWORKSPACE, PHLMONITOR>       WorkspaceSpecialActive{};
    inline constexpr EventTag<EventId::WorkspaceCreated, PHLWORKSPACE>                         WorkspaceCreated{};
    // the workspace may already be on its way out: use workspaceId()/workspaceName() on the ref
    inline constexpr EventTag<EventId::WorkspaceRemoved, PHLWORKSPACEREF>                      WorkspaceRemoved{};
    inline constexpr EventTag<EventId::WorkspaceMoveToMonitor, PHLWORKSPACE, PHLMONITOR>       WorkspaceMoveToMonitor{};
    inline constexpr EventTag<EventId::ConfigReloaded>                                         ConfigReloaded{};
    // argument: whether the refresh ran as scheduled
    inline constexpr EventTag<EventId::PropsRefreshed, bool>                                   PropsRefreshed{};
    // argument: new submap name ("" = default)
    inline constexpr EventTag<EventId::Submap, std::string>                                    Submap{};
    // arguments: started, type, name
    inline constexpr EventTag<EventId::ScreenshareState, bool, int, std::string>               ScreenshareState{};
    // arguments: keycode (xkb, +8), time in ms, state (0 up, 1 down)
    inline constexpr EventTag<EventId::KeyboardKey, int, int, int>                             KeyboardKey{};
}
