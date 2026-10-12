#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <wayland-server.h>
#include "../../helpers/signal/Signal.hpp"
#include <abyssutils/os/FileDescriptor.hpp>

#include "EventLoopTimer.hpp"

namespace Abyssal {
    struct SPollFD;
};

struct SEventLoopDoLaterLock {
    SEventLoopDoLaterLock(uint64_t seq);
    ~SEventLoopDoLaterLock();

    uint64_t seq = 0;
};

class CEventLoopManager {
  public:
    CEventLoopManager(wl_display* display, wl_event_loop* wlEventLoop);
    ~CEventLoopManager();

    void enterLoop();

    // Note: will remove the timer if the ptr is lost.
    void addTimer(SP<CEventLoopTimer> timer);
    void removeTimer(SP<CEventLoopTimer> timer);

    void onTimerFire();

    // schedules a recalc of the timers
    void scheduleRecalc();

    // schedules a function to run later, aka in a wayland idle event. Returns a sequence which can be used to remove it.
    uint64_t doLater(const std::function<void()>& fn);
    void     removeDoLater(uint64_t seq);

    // automatically cleaned up doLater instance
    [[nodiscard]] UP<SEventLoopDoLaterLock> doLaterLock(const std::function<void()>& fn);

    struct SIdleData {
        wl_event_source*                                        eventSource = nullptr;
        std::vector<std::pair<uint64_t, std::function<void()>>> fns;
    };

    struct SReadableWaiter {
        wl_event_source*               source;
        Abyssutils::OS::CFileDescriptor fd;
        std::function<void()>          fn;

        SReadableWaiter(wl_event_source* src, Abyssutils::OS::CFileDescriptor f, std::function<void()> func) : source(src), fd(std::move(f)), fn(std::move(func)) {}

        ~SReadableWaiter() {
            if (source) {
                wl_event_source_remove(source);
                source = nullptr;
            }
        }

        // copy
        SReadableWaiter(const SReadableWaiter&)            = delete;
        SReadableWaiter& operator=(const SReadableWaiter&) = delete;

        // move
        SReadableWaiter(SReadableWaiter&& other) noexcept            = default;
        SReadableWaiter& operator=(SReadableWaiter&& other) noexcept = default;
    };

    // schedule function to when fd is readable (WL_EVENT_READABLE / POLLIN),
    // takes ownership of fd
    void doOnReadable(Abyssutils::OS::CFileDescriptor fd, std::function<void()>&& fn);
    // persistent watcher: fn(fd, mask, data) on every readable / hangup event, the caller keeps fd and removes the source with wl_event_source_remove
    wl_event_source* addFdWatcher(int fd, wl_event_loop_fd_func_t fn, void* data);
    void onFdReadable(SReadableWaiter* waiter);
    void onFdReadableFail(SReadableWaiter* waiter);

  private:
    // Manages the event sources after AQ pollFDs change.
    void syncPollFDs();
    void nudgeTimers();

    struct SEventSourceData {
        SP<Abyssal::SPollFD> pollFD;
        wl_event_source*        eventSource = nullptr;
    };

    struct {
        wl_event_loop*   loop        = nullptr;
        wl_display*      display     = nullptr;
        wl_event_source* eventSource = nullptr;
    } m_wayland;

    struct {
        std::vector<SP<CEventLoopTimer>> timers;
        Abyssutils::OS::CFileDescriptor   timerfd;
        bool                             recalcScheduled = false;
    } m_timers;

    SIdleData                        m_idle;
    std::map<int, SEventSourceData>  m_aqEventSources;
    std::vector<UP<SReadableWaiter>> m_readableWaiters;

    struct {
        CAbyssSignalListener pollFDsChanged;
    } m_listeners;

    wl_event_source* m_configWatcherInotifySource = nullptr;

    friend class CAsyncDialogBox;
    friend class CMainLoopExecutor;
};

inline UP<CEventLoopManager> g_pEventLoopManager;
