#include "GLRenderbuffer.hpp"
#include "../Renderer.hpp"
#include "../OpenGL.hpp"
#include "../../Compositor.hpp"
#include "../Framebuffer.hpp"
#include "GLFramebuffer.hpp"
#include "../Renderbuffer.hpp"
#include <abyssutils/memory/SharedPtr.hpp>
#include <abyssutils/signal/Listener.hpp>
#include <abyssutils/signal/Signal.hpp>

#include <dlfcn.h>

using namespace Render::GL;

CGLRenderbuffer::~CGLRenderbuffer() {
    if (!g_pCompositor || g_pCompositor->m_isShuttingDown || !g_pAbyssRenderer)
        return;

    g_pAbyssOpenGL->makeEGLCurrent();

    if (m_framebuffer) {
        unbind();
        m_framebuffer->release();
    }

    if (m_rbo)
        glDeleteRenderbuffers(1, &m_rbo);

    if (m_image != EGL_NO_IMAGE_KHR)
        g_pAbyssOpenGL->m_proc.eglDestroyImageKHR(g_pAbyssOpenGL->m_eglDisplay, m_image);
}

CGLRenderbuffer::CGLRenderbuffer(SP<Abyssal::IBuffer> buffer, uint32_t format) : IRenderbuffer(buffer, format) {
    auto dma = buffer->dmabuf();

    m_image = g_pAbyssOpenGL->createEGLImage(dma);
    if (m_image == EGL_NO_IMAGE_KHR) {
        Log::logger->log(Log::ERR, "rb: createEGLImage failed");
        return;
    }

    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    g_pAbyssOpenGL->m_proc.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_image);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    m_framebuffer = makeShared<CGLFramebuffer>();
    glGenFramebuffers(1, &GLFB(m_framebuffer)->m_fb);
    GLFB(m_framebuffer)->m_fbAllocated = true;
    m_framebuffer->m_size              = buffer->size;
    m_framebuffer->m_drmFormat         = dma.format;
    m_framebuffer->bind();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log::logger->log(Log::ERR, "rbo: glCheckFramebufferStatus failed");
        return;
    }

    GLFB(m_framebuffer)->unbind();

    m_listeners.destroyBuffer = buffer->events.destroy.listen([this] { g_pAbyssRenderer->onRenderbufferDestroy(this); });

    m_good = true;
}

void CGLRenderbuffer::bind() {
    g_pAbyssOpenGL->makeEGLCurrent();
    g_pAbyssRenderer->bindFB(m_framebuffer);
}

void CGLRenderbuffer::unbind() {
    GLFB(m_framebuffer)->unbind();
}
