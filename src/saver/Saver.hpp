#pragma once

// Screensaver runtime (Abysslang c.idle(seconds, screensaver(...))): fullscreen overlay per monitor drawn from a Canvas callback,
// optional cava child process for audio bars. Costs nothing (no timer, no process, no texture) while no screensaver is shown.

#include <chrono>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>
#include <sys/types.h>
#include <wayland-server-core.h>

#include <abyss/saver.hpp>

#include "../helpers/memory/Memory.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"
#include "../render/Texture.hpp"

namespace Saver {

    // returns false when the user callback threw (the frame is then drawn black)
    using DrawFn = std::function<bool(abyss::Canvas&, const abyss::SaverFrame&)>;

    struct SDef {
        DrawFn           draw;
        abyss::SaverOpts opts;
    };

    class CSaverManager {
      public:
        CSaverManager();
        ~CSaverManager();

        // config lifecycle: registered by c.idle(seconds, screensaver(...)), dropped (and a running screensaver ended) by reload
        void   registerDef(SP<SDef> def);
        void   reset();
        size_t defCount() const;

        bool   showFirst();
        void   show(SP<SDef> def);
        void   hide();
        bool   active() const;

        // renderer: adds the saver of this monitor to the render pass (between workspace and lockscreen)
        void draw(PHLMONITOR mon);

        // input: called at the start of the input handlers. Wakes up when active and returns true when the event must not reach anyone else.
        bool consumeKey(uint32_t keycode, bool pressed);
        bool consumeButton(uint32_t button, bool pressed);
        bool consumeOther(); // wheel, touch, tablet, gestures, modifiers

        // introspection for tests / abyssctl
        bool   cavaRunning() const;
        int    timerArmed() const;
        size_t monitorStates() const;

      private:
        struct SMonState {
            SP<Render::ITexture> tex;
            Vector2D             size;
            float                scale = 0;
            uint64_t             tick  = 0;
        };

        void startTimer();
        void stopTimer();
        void onTick();
        void invalidateAll();

        void startCava();
        void stopCava();
        void onCavaReadable();
        static int cavaFdCallback(int fd, uint32_t mask, void* data);
        std::string cavaConfigPath() const;

        std::vector<SP<SDef>>                      m_defs;
        SP<SDef>                                   m_active;
        std::chrono::steady_clock::time_point      m_start;
        uint64_t                                   m_tick = 1;
        std::unordered_map<MONITORID, SMonState>   m_mons;
        SP<CEventLoopTimer>                        m_timer;
        std::set<uint32_t>                         m_swallowedKeys, m_swallowedButtons;

        // cava
        pid_t                                      m_cavaPid = -1;
        int                                        m_cavaPidfd = -1;
        int                                        m_cavaFd  = -1;
        wl_event_source*                           m_cavaSource = nullptr;
        std::vector<uint8_t>                       m_cavaBuf;
        std::vector<float>                         m_audio;
        std::string                                m_cavaConf;
        std::vector<SP<CEventLoopTimer>>           m_killTimers;

        bool                                       m_exiting = false;
        CAbyssSignalListener                       m_monRemoved, m_exit;
    };

    UP<CSaverManager>& mgr();
}
