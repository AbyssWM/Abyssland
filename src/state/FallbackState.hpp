#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

namespace State {
    constexpr const char* FALLBACK_OUTPUT_NAME = "FALLBACK";

    class CFallbackStateKeeper {
      public:
        CFallbackStateKeeper();
        ~CFallbackStateKeeper();

        CFallbackStateKeeper(const CFallbackStateKeeper&&) = delete;
        CFallbackStateKeeper(const CFallbackStateKeeper&)  = delete;
        CFallbackStateKeeper(CFallbackStateKeeper&)        = delete;

        PHLMONITOR fallbackOutput() const;

      private:
        void                initSignals();
        void                initOutput();

        void                setFallbackActive(bool enabled);

        PHLMONITOR          m_fallbackOutput;
        bool                m_fallbackActive = false;

        SP<CEventLoopTimer> m_launchTimer;

        bool                m_fallbackStateUpdateDeferred = false;

        struct {
            CAbyssSignalListener newMon;
            CAbyssSignalListener monitorRemoved;
            CAbyssSignalListener monitorAdded;
            CAbyssSignalListener start;
            CAbyssSignalListener ready;
        } m_listeners;
    };

    UP<CFallbackStateKeeper>& fallbackState();
}