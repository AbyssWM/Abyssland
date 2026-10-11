#include "CursorManager.hpp"
#include "CursorActivationEnvironment.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "../PointerManager.hpp"
#include "../../xwayland/XWayland.hpp"
#include "../../output/Monitor.hpp"
#include "../../event/EventBus.hpp"
#include "../../state/MonitorState.hpp"

using namespace Pointer;
using namespace Pointer::Cursor;

UP<CCursorManager>& Pointer::Cursor::mgr() {
    static UP<CCursorManager> p = makeUnique<CCursorManager>();
    return p;
}

static int cursorAnimTimer(SP<CEventLoopTimer> self, void* data) {
    const auto cursorMgr = sc<CCursorManager*>(data);
    cursorMgr->tickAnimatedCursor();
    return 1;
}

static void hcLogger(enum eAbysscursorLogLevel level, char* message) {
    if (level == HC_LOG_TRACE)
        return;

    Log::logger->log(Log::DEBUG, "[hc] {}", message);
}

CCursorBuffer::CCursorBuffer(cairo_surface_t* surf, const Vector2D& size_, const Vector2D& hot_) : m_hotspot(hot_), m_stride(cairo_image_surface_get_stride(surf)) {
    size = size_;

    m_data = std::vector<uint8_t>(cairo_image_surface_get_data(surf), cairo_image_surface_get_data(surf) + (cairo_image_surface_get_height(surf) * m_stride));
}

CCursorBuffer::CCursorBuffer(const uint8_t* pixelData, const Vector2D& size_, const Vector2D& hot_) : m_hotspot(hot_), m_stride(4 * size_.x) {
    size = size_;

    m_data = std::vector<uint8_t>(pixelData, pixelData + (sc<int>(size_.y) * m_stride));
}

Abyssal::eBufferCapability CCursorBuffer::caps() {
    return Abyssal::eBufferCapability::BUFFER_CAPABILITY_DATAPTR;
}

Abyssal::eBufferType CCursorBuffer::type() {
    return Abyssal::eBufferType::BUFFER_TYPE_SHM;
}

void CCursorBuffer::update(const Abyssutils::Math::CRegion& damage) {
    ;
}

bool CCursorBuffer::isSynchronous() {
    return true;
}

bool CCursorBuffer::good() {
    return true;
}

Abyssal::SSHMAttrs CCursorBuffer::shm() {
    Abyssal::SSHMAttrs attrs;
    attrs.success = true;
    attrs.format  = DRM_FORMAT_ARGB8888;
    attrs.size    = size;
    attrs.stride  = m_stride;
    return attrs;
}

std::tuple<uint8_t*, uint32_t, size_t> CCursorBuffer::beginDataPtr(uint32_t flags) {
    return {m_data.data(), DRM_FORMAT_ARGB8888, m_stride};
}

void CCursorBuffer::endDataPtr() {
    ;
}

CCursorManager::CCursorManager() {
    m_xcursor            = makeUnique<CXCursorManager>();
    m_initialNativeTheme = getenv("ABYSSCURSOR_THEME") ? getenv("ABYSSCURSOR_THEME") : "";
    m_initialClientTheme = getenv("XCURSOR_THEME") ? getenv("XCURSOR_THEME") : "default";
    auto readSize        = [](const char* name) {
        if (const auto* value = getenv(name)) {
            try {
                const int size = std::stoi(value);
                if (size > 0)
                    return size;
            } catch (...) { ; }
            Log::logger->log(Log::WARN, "Invalid {} cursor size, defaulting to 24", name);
        }
        return 24;
    };
    m_initialNativeSize = readSize("ABYSSCURSOR_SIZE");
    m_initialClientSize = readSize("XCURSOR_SIZE");

    m_animationTimer = makeShared<CEventLoopTimer>(std::nullopt, cursorAnimTimer, this);
    g_pEventLoopManager->addTimer(m_animationTimer);

    m_activationTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer>, void*) {
            collectActivationErrors();
            return 1;
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_activationTimer);

    applyConfig(true);

    static auto P = Event::bus()->m_events.monitor.layoutChanged.listen([this] { this->updateTheme(); });
}

CCursorManager::~CCursorManager() {
    // Cancel and join the worker before destroying timers or compositor state.
    m_activationEnvironment.reset();
    if (m_activationTimer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(m_activationTimer);
    if (m_animationTimer && g_pEventLoopManager) {
        g_pEventLoopManager->removeTimer(m_animationTimer);
        m_animationTimer.reset();
    }

    if (m_abysscursor->valid() && m_currentStyleInfo.size > 0)
        m_abysscursor->cursorSurfaceStyleDone(m_currentStyleInfo);
}

SP<Abyssal::IBuffer> CCursorManager::getCursorBuffer() {
    return !m_cursorBuffers.empty() ? m_cursorBuffers.back() : nullptr;
}

void CCursorManager::setCursorSurface(SP<Desktop::View::CWLSurface> surf, const Vector2D& hotspot) {
    if (!surf || !surf->resource())
        Pointer::mgr()->resetCursorImage();
    else
        Pointer::mgr()->setCursorSurface(surf, hotspot);

    m_ourBufferConnected = false;
}

void CCursorManager::setCursorBuffer(SP<CCursorBuffer> buf, const Vector2D& hotspot, const float& scale) {
    m_cursorBuffers.emplace_back(buf);
    Pointer::mgr()->setCursorBuffer(getCursorBuffer(), hotspot, scale);
    if (m_cursorBuffers.size() > 1)
        std::erase_if(m_cursorBuffers, [this](const auto& buf) { return buf.get() == m_cursorBuffers.front().get(); });

    m_ourBufferConnected = true;
}

void CCursorManager::setAnimationTimer(const int& frame, const int& delay) {
    if (delay > 0) {
        // arm
        m_animationTimer->updateTimeout(std::chrono::milliseconds(delay));
    } else {
        // disarm
        m_animationTimer->updateTimeout(std::nullopt);
    }

    m_currentAnimationFrame = frame;
}

void CCursorManager::setCursorFromName(const std::string& name) {

    static auto PUSEABYSSCURSOR = CConfigValue<Config::INTEGER>("cursor:enable_abysscursor");

    auto        setXCursor = [this](auto const& name) {
        float scale = std::ceil(m_cursorScale);

        auto  xcursor = m_xcursor->getShape(name, m_size, m_cursorScale);
        auto& icon    = xcursor->images.front();
        auto  buf     = makeShared<CCursorBuffer>(rc<uint8_t*>(icon.pixels.data()), icon.size, icon.hotspot);
        setCursorBuffer(buf, icon.hotspot / scale, scale);

        m_currentXcursor = xcursor;

        int delay = 0;
        int frame = 0;
        if (m_currentXcursor->images.size() > 1)
            delay = m_currentXcursor->images[frame].delay;

        setAnimationTimer(frame, delay);
    };

    auto setAbyssCursor = [this](auto const& name) {
        m_currentCursorShapeData = m_abysscursor->getShape(name.c_str(), m_currentStyleInfo);

        if (m_currentCursorShapeData.images.empty()) {
            // try with '_' first (old hc, etc)
            std::string newName = name;
            std::ranges::replace(newName, '-', '_');

            m_currentCursorShapeData = m_abysscursor->getShape(newName.c_str(), m_currentStyleInfo);
        }

        if (m_currentCursorShapeData.images.empty()) {
            // fallback to a default if available
            constexpr const std::array<const char*, 3> fallbackShapes = {"default", "left_ptr", "left-ptr"};

            for (auto const& s : fallbackShapes) {
                m_currentCursorShapeData = m_abysscursor->getShape(s, m_currentStyleInfo);

                if (!m_currentCursorShapeData.images.empty())
                    break;
            }

            if (m_currentCursorShapeData.images.empty()) {
                Log::logger->log(Log::ERR, "BUG THIS: No fallback found for a cursor in setCursorFromName");
                return false;
            }
        }

        auto buf = makeShared<CCursorBuffer>(m_currentCursorShapeData.images[0].surface, Vector2D{m_currentCursorShapeData.images[0].size, m_currentCursorShapeData.images[0].size},
                                             Vector2D{m_currentCursorShapeData.images[0].hotspotX, m_currentCursorShapeData.images[0].hotspotY});
        auto hotspot = Vector2D{m_currentCursorShapeData.images[0].hotspotX, m_currentCursorShapeData.images[0].hotspotY} / m_cursorScale;
        setCursorBuffer(buf, hotspot, m_cursorScale);

        int delay = 0;
        int frame = 0;
        if (m_currentCursorShapeData.images.size() > 1)
            delay = m_currentCursorShapeData.images[frame].delay;

        setAnimationTimer(frame, delay);
        return true;
    };

    if (!m_abysscursor->valid() || !*PUSEABYSSCURSOR || !setAbyssCursor(name))
        setXCursor(name);
}

void CCursorManager::tickAnimatedCursor() {
    if (!m_ourBufferConnected)
        return;

    if (!m_abysscursor->valid() && m_currentXcursor->images.size() > 1) {
        m_currentAnimationFrame++;

        if (sc<size_t>(m_currentAnimationFrame) >= m_currentXcursor->images.size())
            m_currentAnimationFrame = 0;

        float scale = std::ceil(m_cursorScale);
        auto& icon  = m_currentXcursor->images.at(m_currentAnimationFrame);
        auto  buf   = makeShared<CCursorBuffer>(rc<uint8_t*>(icon.pixels.data()), icon.size, icon.hotspot);
        setCursorBuffer(buf, icon.hotspot / scale, scale);
        setAnimationTimer(m_currentAnimationFrame, m_currentXcursor->images[m_currentAnimationFrame].delay);
    } else if (m_currentCursorShapeData.images.size() > 1) {
        m_currentAnimationFrame++;

        if (sc<size_t>(m_currentAnimationFrame) >= m_currentCursorShapeData.images.size())
            m_currentAnimationFrame = 0;

        auto hotspot =
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotX, m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotY} / m_cursorScale;
        auto buf = makeShared<CCursorBuffer>(
            m_currentCursorShapeData.images[m_currentAnimationFrame].surface,
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].size, m_currentCursorShapeData.images[m_currentAnimationFrame].size},
            Vector2D{m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotX, m_currentCursorShapeData.images[m_currentAnimationFrame].hotspotY});
        setCursorBuffer(buf, hotspot, m_cursorScale);
        setAnimationTimer(m_currentAnimationFrame, m_currentCursorShapeData.images[m_currentAnimationFrame].delay);
    }
}

SCursorImageData CCursorManager::dataFor(const std::string& name) {

    if (!m_abysscursor->valid())
        return {};

    const auto IMAGES = m_abysscursor->getShape(name.c_str(), m_currentStyleInfo);

    if (IMAGES.images.empty())
        return {};

    return IMAGES.images[0];
}

void CCursorManager::setXWaylandCursor() {
#ifndef NO_XWAYLAND
    if (!g_pXWayland || !g_pXWayland->m_wm)
        return;
    g_pXWayland->m_wm->setCursorTheme(m_clientTheme, m_size);
#else
    return;
#endif

    static auto PUSEABYSSCURSOR = CConfigValue<Config::INTEGER>("cursor:enable_abysscursor");
    const auto  CURSOR          = dataFor("left_ptr");
    if (CURSOR.surface && *PUSEABYSSCURSOR)
        g_pXWayland->setCursor(cairo_image_surface_get_data(CURSOR.surface), cairo_image_surface_get_stride(CURSOR.surface), {CURSOR.size, CURSOR.size},
                               {CURSOR.hotspotX, CURSOR.hotspotY});
    else {
        auto xcursor = m_xcursor->getShape("left_ptr", m_size, 1);
        if (!xcursor || xcursor->images.empty())
            return;
        auto& icon = xcursor->images.front();

        g_pXWayland->setCursor(rc<uint8_t*>(icon.pixels.data()), icon.size.x * 4, icon.size, icon.hotspot);
    }
}

void CCursorManager::updateTheme() {
    static auto PUSEABYSSCURSOR = CConfigValue<Config::INTEGER>("cursor:enable_abysscursor");
    float       highestScale    = 1.0;

    for (auto const& m : State::monitorState()->monitors()) {
        if (m->m_scale > highestScale)
            highestScale = m->m_scale;
    }

    m_cursorScale            = highestScale;
    m_currentCursorShapeData = {};

    if (*PUSEABYSSCURSOR) {
        if (m_currentStyleInfo.size > 0 && m_abysscursor->valid())
            m_abysscursor->cursorSurfaceStyleDone(m_currentStyleInfo);

        m_currentStyleInfo.size = std::round(m_size * highestScale);

        if (m_abysscursor->valid())
            m_abysscursor->loadThemeStyle(m_currentStyleInfo);
    }

    for (auto const& m : State::monitorState()->monitors()) {
        m->m_forceFullFrames = 5;
        m->scheduleFrame(Abyssal::IOutput::ABYSSAL_SCHEDULE_CURSOR_SHAPE);
    }

    setXWaylandCursor();
}

bool CCursorManager::changeTheme(const std::string& name, const int size) {
    static auto PUSEABYSSCURSOR = CConfigValue<Config::INTEGER>("cursor:enable_abysscursor");
    m_theme                     = name.empty() ? "" : name;
    m_size                      = size <= 0 ? 24 : size;
    m_clientTheme               = m_theme.empty() ? m_clientTheme : m_theme;

    if (*PUSEABYSSCURSOR) {
        auto options                 = Abysscursor::SManagerOptions();
        options.logFn                = hcLogger;
        options.allowDefaultFallback = false;

        if (m_abysscursor->valid() && m_currentStyleInfo.size > 0)
            m_abysscursor->cursorSurfaceStyleDone(m_currentStyleInfo);
        m_currentStyleInfo.size = 0;

        m_abysscursor = makeUnique<Abysscursor::CAbysscursorManager>(m_theme.empty() ? nullptr : m_theme.c_str(), options);
        if (!m_abysscursor->valid())
            Log::logger->log(Log::ERR, "Abysscursor failed loading theme \"{}\", falling back to XCursor.", m_theme);
    }

    // Keep the fallback and client-side cursor theme current even when the native theme is valid.
    m_xcursor->loadTheme(m_clientTheme, m_size, m_cursorScale);
    applyEnvironment();
    updateTheme();

    return true;
}

void CCursorManager::applyConfig(bool force) {
    static auto PTHEME  = CConfigValue<Config::STRING>("cursor:theme");
    static auto PSIZE   = CConfigValue<Config::INTEGER>("cursor:size");
    static auto PNATIVE = CConfigValue<Config::INTEGER>("cursor:enable_abysscursor");
    const auto  theme   = *PTHEME;
    const int   size    = *PSIZE;
    const bool  native  = *PNATIVE;
    if (!force && theme == m_configTheme && size == m_configSize && native == m_configNative)
        return;

    m_configTheme  = theme;
    m_configSize   = size;
    m_configNative = native;
    m_theme        = theme.empty() ? m_initialNativeTheme : theme;
    m_clientTheme  = theme.empty() ? m_initialClientTheme : theme;

    if (m_abysscursor && m_abysscursor->valid() && m_currentStyleInfo.size > 0)
        m_abysscursor->cursorSurfaceStyleDone(m_currentStyleInfo);
    m_currentStyleInfo.size      = 0;
    auto options                 = Abysscursor::SManagerOptions();
    options.logFn                = hcLogger;
    options.allowDefaultFallback = theme.empty();
    // A null theme makes Abysscursor read the environment again. Restore the
    // startup value first when an explicit config theme is changed back to auto.
    setenv("ABYSSCURSOR_THEME", m_theme.c_str(), 1);
    m_abysscursor = makeUnique<Abysscursor::CAbysscursorManager>(m_theme.empty() ? nullptr : m_theme.c_str(), options);
    m_size        = size > 0 ? size : (native && m_abysscursor->valid() ? m_initialNativeSize : m_initialClientSize);
    if (native && !m_abysscursor->valid())
        Log::logger->log(Log::WARN, "Abysscursor failed loading theme \"{}\", falling back to XCursor", m_theme);

    m_xcursor->loadTheme(m_clientTheme, m_size, m_cursorScale);
    applyEnvironment();
    updateTheme();
}

void CCursorManager::applyEnvironment() {
    const auto SIZE = std::to_string(m_size);
    setenv("XCURSOR_THEME", m_clientTheme.c_str(), 1);
    setenv("XCURSOR_SIZE", SIZE.c_str(), 1);
    setenv("ABYSSCURSOR_THEME", m_theme.c_str(), 1);
    setenv("ABYSSCURSOR_SIZE", SIZE.c_str(), 1);
    syncGsettings();
    exportActivationEnvironment();
}

void CCursorManager::exportActivationEnvironment() {
    // A nested compositor shares its parent's session bus. Never change that activation environment.
    if (!g_pCompositor->m_aqBackend->hasSession() || Env::envEnabled("ABYSSLAND_NO_SD_VARS"))
        return;

    const auto* address = getenv("DBUS_SESSION_BUS_ADDRESS");
    if (!address || !*address)
        return;

    if (!m_activationEnvironment)
        m_activationEnvironment = makeUnique<CActivationEnvironment>(address);
    m_activationEnvironment->update({m_theme, m_clientTheme, m_size});
    m_activationTimer->updateTimeout(std::chrono::milliseconds(100));
}

void CCursorManager::collectActivationErrors() {
    if (!m_activationEnvironment)
        return;

    for (const auto& error : m_activationEnvironment->takeErrors())
        Log::logger->log(Log::WARN, "{}", error);
    m_activationTimer->updateTimeout(m_activationEnvironment->busy() ? std::make_optional(std::chrono::milliseconds(100)) : std::nullopt);
}

void CCursorManager::syncGsettings() {
    m_xcursor->syncGsettings();
}

float CCursorManager::getScaledSize() const {
    return m_size * m_cursorScale;
}
