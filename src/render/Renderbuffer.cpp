#include "Renderbuffer.hpp"
#include "Framebuffer.hpp"
#include "Renderer.hpp"
#include <abyssutils/memory/SharedPtr.hpp>
#include <abyssutils/signal/Listener.hpp>
#include <abyssutils/signal/Signal.hpp>

#include <dlfcn.h>

using namespace Render;

IRenderbuffer::IRenderbuffer(SP<Abyssal::IBuffer> buffer, uint32_t format) : m_hlBuffer(buffer) {
    m_listeners.destroyBuffer = buffer->events.destroy.listen([this] { g_pAbyssRenderer->onRenderbufferDestroy(this); });
}

bool IRenderbuffer::good() {
    return m_good;
}

SP<IFramebuffer> IRenderbuffer::getFB() {
    return m_framebuffer;
}
