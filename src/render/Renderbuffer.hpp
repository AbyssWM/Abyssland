#pragma once

#include "../helpers/signal/Signal.hpp"
#include "../helpers/memory/Memory.hpp"
#include "Framebuffer.hpp"
#include <abyssal/buffer/Buffer.hpp>

namespace Render {
    class IRenderbuffer {
      public:
        IRenderbuffer(SP<Abyssal::IBuffer> buffer, uint32_t format);
        virtual ~IRenderbuffer() = default;

        bool                    good();
        SP<IFramebuffer>        getFB();

        virtual void            bind()   = 0;
        virtual void            unbind() = 0;

        WP<Abyssal::IBuffer> m_hlBuffer;

      protected:
        SP<IFramebuffer> m_framebuffer;
        bool             m_good = false;

        struct {
            CAbyssSignalListener destroyBuffer;
        } m_listeners;
    };
}
