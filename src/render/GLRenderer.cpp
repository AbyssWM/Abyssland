#include "GLRenderer.hpp"
#include "decorations/CAbyssInnerGlowDecoration.hpp"
#include <abyssal/output/Output.hpp>
#include "../config/ConfigValue.hpp"
#include "../pointer/cursor/CursorManager.hpp"
#include "../pointer/PointerManager.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../debug/Overlay.hpp"
#include "../output/Monitor.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "../debug/log/Logger.hpp"
#include "../protocols/types/ContentType.hpp"
#include "OpenGL.hpp"
#include "Renderer.hpp"
#include "./gl/GLElementRenderer.hpp"
#include "./gl/GLFramebuffer.hpp"
#include "./gl/GLTexture.hpp"

#include <cstdint>
#include <abyssutils/memory/SharedPtr.hpp>
#include <abyssutils/memory/UniquePtr.hpp>
#include <abyssutils/utils/ScopeGuard.hpp>
using namespace Abyssutils::Utils;
using namespace Abyssutils::OS;
using enum NContentType::eContentType;
using namespace NColorManagement;
using namespace Render;
using namespace Render::GL;

extern "C" {
#include <xf86drm.h>
}

CAbyssGLRenderer::CAbyssGLRenderer() : IAbyssRenderer(), m_elementRenderer(makeUnique<CGLElementRenderer>()) {}

IAbyssRenderer::eType CAbyssGLRenderer::type() {
    return RT_GL;
}

void CAbyssGLRenderer::initRender() {
    g_pAbyssOpenGL->makeEGLCurrent();
    g_pAbyssRenderer->m_renderData.pMonitor = renderData().pMonitor;
}

bool CAbyssGLRenderer::initRenderBuffer(SP<Abyssal::IBuffer> buffer, uint32_t fmt) {
    try {
        m_currentRenderbuffer = getOrCreateRenderbuffer(m_currentBuffer, fmt);
    } catch (std::exception& e) {
        Log::logger->log(Log::ERR, "getOrCreateRenderbuffer failed for {}", NFormatUtils::drmFormatName(fmt));
        return false;
    }

    return !!m_currentRenderbuffer;
}

bool CAbyssGLRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
    bindFB(fb);
    if (simple)
        g_pAbyssOpenGL->beginSimple(pMonitor, damage, nullptr, fb);
    else
        g_pAbyssOpenGL->begin(pMonitor, damage, fb);
    return true;
}

bool CAbyssGLRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {

    m_currentRenderbuffer->bind();
    if (simple)
        g_pAbyssOpenGL->beginSimple(pMonitor, damage, m_currentRenderbuffer);
    else
        g_pAbyssOpenGL->begin(pMonitor, damage);

    return true;
}

void CAbyssGLRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    const auto  PMONITOR           = g_pAbyssRenderer->m_renderData.pMonitor;
    static auto PNVIDIAANTIFLICKER = CConfigValue<Config::INTEGER>("opengl:nvidia_anti_flicker");

    g_pAbyssRenderer->m_renderData.damage = m_renderPass.render(g_pAbyssRenderer->m_renderData.damage);

    auto cleanup = CScopeGuard([this]() {
        if (m_currentRenderbuffer)
            m_currentRenderbuffer->unbind();
        m_currentRenderbuffer = nullptr;
        m_currentBuffer       = nullptr;
    });

    if (m_renderMode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pAbyssOpenGL->end();
    else {
        g_pAbyssRenderer->m_renderData.pMonitor.reset();
        g_pAbyssRenderer->m_renderData.mouseZoomFactor   = 1.f;
        g_pAbyssRenderer->m_renderData.mouseZoomUseMouse = true;
    }

    if (m_renderMode == RENDER_MODE_FULL_FAKE)
        return;

    if (m_renderMode == RENDER_MODE_NORMAL)
        PMONITOR->m_output->state->setBuffer(m_currentBuffer);

    if (!explicitSyncSupported()) {
        Log::logger->log(Log::TRACE, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        // nvidia doesn't have implicit sync, so we have to explicitly wait here, llvmpipe and other software renderer seems to bug out as well.
        if ((isNvidia() && *PNVIDIAANTIFLICKER) || isSoftware())
            glFinish();
        else
            glFlush(); // mark an implicit sync point

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();

        return;
    }

    auto eglSync = createSyncFDManager();
    if LIKELY (eglSync && eglSync->isValid()) {
        for (auto const& buf : m_usedAsyncBuffers) {
            for (const auto& releaser : buf->m_syncReleasers) {
                releaser->addSyncFileFd(eglSync->fd());
            }
        }

        // release buffer refs with release points now, since syncReleaser handles actual buffer release based on EGLSync
        std::erase_if(m_usedAsyncBuffers, [](const auto& buf) { return !buf->m_syncReleasers.empty(); });

        // release buffer refs without release points when EGLSync sync_file/fence is signalled
        g_pEventLoopManager->doOnReadable(eglSync->fd().duplicate(), [renderingDoneCallback, prevbfs = std::move(m_usedAsyncBuffers)]() mutable {
            prevbfs.clear();
            if (renderingDoneCallback)
                renderingDoneCallback();
        });
        m_usedAsyncBuffers.clear();

        if (m_renderMode == RENDER_MODE_NORMAL) {
            PMONITOR->m_inFence = eglSync->takeFd();
            PMONITOR->m_output->state->setExplicitInFence(PMONITOR->m_inFence.get());
        }
    } else {
        Log::logger->log(Log::ERR, "renderer: Explicit sync failed, falling back to implicit sync");

        // Establish an implicit synchronization point without blocking the render loop.
        glFlush();

        if (m_renderMode == RENDER_MODE_NORMAL && PMONITOR) {
            PMONITOR->m_inFence.reset();
            PMONITOR->m_output->state->resetExplicitFences();
        }

        m_usedAsyncBuffers.clear();
        if (renderingDoneCallback)
            renderingDoneCallback();
    }
}

void CAbyssGLRenderer::renderOffToMain(SP<IFramebuffer> off) {
    g_pAbyssOpenGL->renderOffToMain(off);
}

SP<IRenderbuffer> CAbyssGLRenderer::getOrCreateRenderbufferInternal(SP<Abyssal::IBuffer> buffer, uint32_t fmt) {
    g_pAbyssOpenGL->makeEGLCurrent();
    return makeShared<CGLRenderbuffer>(buffer, fmt);
}

UP<ISyncFDManager> CAbyssGLRenderer::createSyncFDManager() {
    return CEGLSync::create();
}

SP<ITexture> CAbyssGLRenderer::createStencilTexture(const int width, const int height) {
    g_pAbyssOpenGL->makeEGLCurrent();
    auto tex = makeShared<CGLTexture>();
    tex->allocate({width, height});

    return tex;
}

SP<ITexture> CAbyssGLRenderer::createTexture(bool opaque) {
    g_pAbyssOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(opaque);
}

SP<ITexture> CAbyssGLRenderer::createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) {
    g_pAbyssOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(drmFormat, pixels, stride, size, keepDataCopy, opaque);
}

SP<ITexture> CAbyssGLRenderer::createTexture(const Abyssal::SDMABUFAttrs& attrs, bool opaque) {
    g_pAbyssOpenGL->makeEGLCurrent();
    const auto image = g_pAbyssOpenGL->createEGLImage(attrs);
    if (!image)
        return nullptr;
    return makeShared<CGLTexture>(attrs, image, opaque);
}

SP<ITexture> CAbyssGLRenderer::createTexture(const int width, const int height, unsigned char* const data) {
    g_pAbyssOpenGL->makeEGLCurrent();
    SP<ITexture> tex = makeShared<CGLTexture>();

    tex->allocate({width, height}, DRM_FORMAT_ARGB8888); // FIXME assume DRM_FORMAT_ARGB8888

    tex->m_size = {width, height};
    // copy the data to an OpenGL texture we have
    const GLint glFormat = GL_RGBA;
    const GLint glType   = GL_UNSIGNED_BYTE;

    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, data);
    tex->unbind();

    return tex;
}

SP<ITexture> CAbyssGLRenderer::createTexture(cairo_surface_t* cairo) {
    g_pAbyssOpenGL->makeEGLCurrent();
    const auto CAIROFORMAT = cairo_image_surface_get_format(cairo);
    auto       tex         = makeShared<CGLTexture>();

    tex->allocate({cairo_image_surface_get_width(cairo), cairo_image_surface_get_height(cairo)});

    const GLint glIFormat = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB32F : GL_RGBA;
    const GLint glFormat  = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB : GL_RGBA;
    const GLint glType    = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_FLOAT : GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(cairo);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (CAIROFORMAT != CAIRO_FORMAT_RGB96F) {
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
        tex->m_drmFormat = DRM_FORMAT_ARGB8888;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    return tex;
}

SP<ITexture> CAbyssGLRenderer::createTexture(std::span<const float> lut3D, size_t N) {
    g_pAbyssOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(lut3D, N);
}

bool CAbyssGLRenderer::explicitSyncSupported() {
    return g_pAbyssOpenGL->explicitSyncSupported();
}

std::vector<SDRMFormat> CAbyssGLRenderer::getDRMFormats() {
    return g_pAbyssOpenGL->getDRMFormats();
}

std::vector<uint64_t> CAbyssGLRenderer::getDRMFormatModifiers(DRMFormat format) {
    return g_pAbyssOpenGL->getDRMFormatModifiers(format);
}

SP<IFramebuffer> CAbyssGLRenderer::createFB(const std::string& name) {
    g_pAbyssOpenGL->makeEGLCurrent();
    return makeShared<CGLFramebuffer>(name);
}

void CAbyssGLRenderer::disableScissor() {
    g_pAbyssOpenGL->scissor(nullptr);
}

void CAbyssGLRenderer::blend(bool enabled) {
    g_pAbyssOpenGL->blend(enabled);
}

void CAbyssGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {
    g_pAbyssOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, a);
}

void CAbyssGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,
                                 float lerp, float a) {
    g_pAbyssOpenGL->renderRoundedShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);
}

void CAbyssGLRenderer::drawGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {
    g_pAbyssOpenGL->renderInnerGlow(box, round, roundingPower, range, color, 0, a);
}

void CAbyssGLRenderer::drawGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,
                               float lerp, float a) {
    g_pAbyssOpenGL->renderInnerGlow(box, round, roundingPower, range, grad1, grad2, lerp, 0, a);
}

SP<ITexture> CAbyssGLRenderer::blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) {
    auto src = GLFB(source);
    return g_pAbyssOpenGL->blurFramebufferWithDamage(a, originalDamage, *src)->getTexture();
}

void CAbyssGLRenderer::setViewport(int x, int y, int width, int height) {
    g_pAbyssOpenGL->setViewport(x, y, width, height);
}

bool CAbyssGLRenderer::reloadShaders(const std::string& path) {
    return g_pAbyssOpenGL->initShaders(path);
}

SP<ITexture> CAbyssGLRenderer::getBlurTexture(PHLMONITORREF pMonitor) {
    return pMonitor->resources()->m_blurFB->getTexture();
}

void CAbyssGLRenderer::unsetEGL() {
    if (!g_pAbyssOpenGL)
        return;

    eglMakeCurrent(g_pAbyssOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

WP<IElementRenderer> CAbyssGLRenderer::elementRenderer() {
    return m_elementRenderer;
}
