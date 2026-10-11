#pragma once

#include <string>
#include <abysscursor/abysscursor.hpp>
#include "../../includes.hpp"
#include "../../desktop/view/WLSurface.hpp"
#include "../../helpers/math/Math.hpp"
#include "../../helpers/memory/Memory.hpp"
#include "../../macros.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../managers/XCursorManager.hpp"
#include <abyssal/buffer/Buffer.hpp>

ABYSSAL_FORWARD(IBuffer);

namespace Pointer::Cursor {

    class CActivationEnvironment;

    class CCursorBuffer : public Abyssal::IBuffer {
      public:
        CCursorBuffer(cairo_surface_t* surf, const Vector2D& size, const Vector2D& hotspot);
        CCursorBuffer(const uint8_t* pixelData, const Vector2D& size, const Vector2D& hotspot);
        ~CCursorBuffer() = default;

        virtual Abyssal::eBufferCapability             caps();
        virtual Abyssal::eBufferType                   type();
        virtual void                                   update(const Abyssutils::Math::CRegion& damage);
        virtual bool                                   isSynchronous(); // whether the updates to this buffer are synchronous, aka happen over cpu
        virtual bool                                   good();
        virtual Abyssal::SSHMAttrs                     shm();
        virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
        virtual void                                   endDataPtr();

      private:
        Vector2D             m_hotspot;
        std::vector<uint8_t> m_data;
        size_t               m_stride = 0;
    };

    class CCursorManager {
      public:
        CCursorManager();
        ~CCursorManager();

        SP<Abyssal::IBuffer> getCursorBuffer();

        void                 setCursorFromName(const std::string& name);
        void                 setCursorSurface(SP<Desktop::View::CWLSurface> surf, const Vector2D& hotspot);
        void                 setCursorBuffer(SP<CCursorBuffer> buf, const Vector2D& hotspot, const float& scale);
        void                 setAnimationTimer(const int& frame, const int& delay);

        bool                 changeTheme(const std::string& name, const int size);
        void                 applyConfig(bool force = false);
        void                 updateTheme();
        SCursorImageData     dataFor(const std::string& name); // for xwayland
        void                 setXWaylandCursor();
        void                 syncGsettings();

        void                 tickAnimatedCursor();

        float                getScaledSize() const;

      private:
        void                                 applyEnvironment();
        void                                 exportActivationEnvironment();
        void                                 collectActivationErrors();
        bool                                 m_ourBufferConnected = false;
        std::vector<SP<CCursorBuffer>>       m_cursorBuffers;

        UP<Abysscursor::CAbysscursorManager> m_abysscursor;
        UP<CXCursorManager>                  m_xcursor;
        SP<SXCursors>                        m_currentXcursor;

        std::string                          m_theme       = "";
        std::string                          m_clientTheme = "default";
        std::string                          m_initialNativeTheme;
        std::string                          m_initialClientTheme;
        int                                  m_initialNativeSize = 24;
        int                                  m_initialClientSize = 24;
        std::string                          m_configTheme;
        int                                  m_configSize   = 0;
        bool                                 m_configNative = true;
        int                                  m_size         = 0;
        float                                m_cursorScale  = 1.0;

        Abysscursor::SCursorStyleInfo        m_currentStyleInfo;

        SP<CEventLoopTimer>                  m_animationTimer;
        SP<CEventLoopTimer>                  m_activationTimer;
        UP<CActivationEnvironment>           m_activationEnvironment;
        int                                  m_currentAnimationFrame = 0;
        Abysscursor::SCursorShapeData        m_currentCursorShapeData;
    };

    UP<CCursorManager>& mgr();
}
