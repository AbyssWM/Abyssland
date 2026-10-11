#include "CAbyssInnerGlowDecoration.hpp"

#include <algorithm>

#include "../../config/ConfigValue.hpp"
#include "../../Compositor.hpp"
#include "../pass/InnerGlowPassElement.hpp"
#include "../Renderer.hpp"
#include "../OpenGL.hpp"

CAbyssInnerGlowDecoration::CAbyssInnerGlowDecoration(PHLWINDOW pWindow) : IAbyssWindowDecoration(pWindow), m_window(pWindow) {
    ;
}

eDecorationType CAbyssInnerGlowDecoration::getDecorationType() {
    return DECORATION_INNER_GLOW;
}

SDecorationPositioningInfo CAbyssInnerGlowDecoration::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.policy = DECORATION_POSITION_ABSOLUTE;
    info.edges  = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;
    return info;
}

void CAbyssInnerGlowDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    updateWindow(m_window.lock());
}

uint64_t CAbyssInnerGlowDecoration::getDecorationFlags() {
    return DECORATION_NON_SOLID;
}

std::string CAbyssInnerGlowDecoration::getDisplayName() {
    return "Inner Glow";
}

void CAbyssInnerGlowDecoration::damageEntire() {
    const auto PWINDOW = m_window.lock();
    if (!validMapped(PWINDOW))
        return;

    CBox       windowBox = PWINDOW->getWindowMainSurfaceBox();

    const auto PWORKSPACE = PWINDOW->m_workspace;
    if (PWORKSPACE && PWORKSPACE->m_renderOffset->isBeingAnimated() && !PWINDOW->m_pinned)
        windowBox.translate(PWORKSPACE->m_renderOffset->value());
    windowBox.translate(PWINDOW->m_floatingOffset);

    g_pAbyssRenderer->damageRegion(CRegion(windowBox));
}

void CAbyssInnerGlowDecoration::updateWindow(PHLWINDOW pWindow) {
    const auto PWINDOW = m_window.lock();
    m_lastWindowPos    = PWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    m_lastWindowSize   = PWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
}

void CAbyssInnerGlowDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    CInnerGlowPassElement::SInnerGlowData data;
    data.deco = this;
    data.a    = a;
    g_pAbyssRenderer->addPassElement(makeUnique<CInnerGlowPassElement>(data));
}

void CAbyssInnerGlowDecoration::render(PHLMONITOR pMonitor, float const& a) {
    static auto PGLOW = CConfigValue<Config::INTEGER>("decoration:glow:enabled");
    if (!*PGLOW || !visible())
        return;

    const auto PWINDOW = m_window.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto ROUNDING      = PWINDOW->rounding() > 0 ? PWINDOW->rounding() - 1 : PWINDOW->rounding();
    const auto ROUNDINGPOWER = PWINDOW->roundingPower();
    const auto PWORKSPACE    = PWINDOW->m_workspace;
    const auto WORKSPACEOFF  = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    CBox       windowBox = {m_lastWindowPos.x, m_lastWindowPos.y, m_lastWindowSize.x, m_lastWindowSize.y};
    windowBox.translate(-pMonitor->m_position + WORKSPACEOFF + PWINDOW->m_floatingOffset);
    windowBox.scale(pMonitor->m_scale).round();

    if (windowBox.width < 1 || windowBox.height < 1)
        return;

    static auto PGLOWSIZE = CConfigValue<Config::INTEGER>("decoration:glow:range");
    const auto  GLOWSIZE  = sc<int>(*PGLOWSIZE);

    auto        grad     = PWINDOW->m_realGlowColor;
    const bool  ANIMATED = PWINDOW->m_glowFadeAnimationProgress->isBeingAnimated();
    if (PWINDOW->m_glowAngleAnimationProgress->enabled()) {
        grad.m_angle += PWINDOW->m_glowAngleAnimationProgress->value() * M_PI * 2;
        grad.m_angle = normalizeAngleRad(grad.m_angle);

        if (ANIMATED)
            PWINDOW->m_realGlowColorPrevious.m_angle = grad.m_angle;
    }

    g_pAbyssRenderer->m_renderData.currentWindow = m_window;

    g_pAbyssRenderer->blend(true);

    if (ANIMATED)
        drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, PWINDOW->m_realGlowColorPrevious, grad,
                         PWINDOW->m_glowFadeAnimationProgress->value(), a);
    else
        drawGlowInternal(windowBox, ROUNDING * pMonitor->m_scale, ROUNDINGPOWER, GLOWSIZE * pMonitor->m_scale, grad, a);

    g_pAbyssRenderer->m_renderData.currentWindow.reset();
}

void CAbyssInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad, float a) {
    if (box.w < 1 || box.h < 1)
        return;
    g_pAbyssRenderer->blend(true);
    g_pAbyssRenderer->m_renderData.currentWindow = m_window;
    g_pAbyssRenderer->drawGlow(box, round, roundingPower, range, grad, a);
    g_pAbyssRenderer->m_renderData.currentWindow.reset();
}

void CAbyssInnerGlowDecoration::drawGlowInternal(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                                const Config::CGradientValueData& grad2, float lerp, float a) {
    if (box.w < 1 || box.h < 1)
        return;
    g_pAbyssRenderer->blend(true);
    g_pAbyssRenderer->m_renderData.currentWindow = m_window;
    g_pAbyssRenderer->drawGlow(box, round, roundingPower, range, grad1, grad2, lerp, a);
    g_pAbyssRenderer->m_renderData.currentWindow.reset();
}

eDecorationLayer CAbyssInnerGlowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

bool CAbyssInnerGlowDecoration::visible() {
    static auto PENABLED = CConfigValue<Config::INTEGER>("decoration:glow:enabled");
    return *PENABLED && m_window->m_ruleApplicator->decorate().valueOrDefault();
}
