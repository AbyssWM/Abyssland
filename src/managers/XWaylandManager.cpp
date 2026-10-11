#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../config/ConfigValue.hpp"
#include "../output/Monitor.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../xwayland/XWayland.hpp"
#include "../state/MonitorState.hpp"
#include <abyssutils/math/Vector2D.hpp>

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

CAbyssXWaylandManager::CAbyssXWaylandManager() = default;

CAbyssXWaylandManager::~CAbyssXWaylandManager() {
#ifndef NO_XWAYLAND
    unsetenv("DISPLAY");
#endif
}

SP<CWLSurfaceResource> CAbyssXWaylandManager::getWindowSurface(PHLWINDOW pWindow) {
    return pWindow ? pWindow->wlSurface()->resource() : nullptr;
}

void CAbyssXWaylandManager::activateSurface(SP<CWLSurfaceResource> pSurface, bool activate) {
    if (!pSurface)
        return;

    auto HLSurface = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!HLSurface) {
        Log::logger->log(Log::TRACE, "CAbyssXWaylandManager::activateSurface on non-desktop surface, ignoring");
        return;
    }

    const auto PWINDOW = Desktop::View::CWindow::fromView(HLSurface->view());
    if (!PWINDOW) {
        Log::logger->log(Log::TRACE, "CAbyssXWaylandManager::activateSurface on non-window surface, ignoring");
        return;
    }

    if (PWINDOW->m_isX11) {
        if (PWINDOW->m_xwaylandSurface) {
            if (activate) {
                PWINDOW->m_xwaylandSurface->setMinimized(false);
                PWINDOW->m_xwaylandSurface->restackToTop();
            }
            PWINDOW->m_xwaylandSurface->activate(activate);
        }
    } else if (PWINDOW->m_xdgSurface && PWINDOW->m_xdgSurface->m_toplevel)
        PWINDOW->m_xdgSurface->m_toplevel->setActive(activate);
}

void CAbyssXWaylandManager::activateWindow(PHLWINDOW pWindow, bool activate) {
    if (pWindow->m_isX11) {

        if (activate) {
            pWindow->sendWindowSize(true); // update xwayland output pos
            pWindow->m_xwaylandSurface->setMinimized(false);

            if (!pWindow->isX11OverrideRedirect())
                pWindow->m_xwaylandSurface->restackToTop();
        }

        pWindow->m_xwaylandSurface->activate(activate);

    } else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->m_toplevel)
        pWindow->m_xdgSurface->m_toplevel->setActive(activate);

    if (activate) {
        Desktop::focusState()->surface() = getWindowSurface(pWindow);
        Desktop::focusState()->window()  = pWindow;
    }

    if (!pWindow->m_pinned)
        pWindow->m_workspace->m_lastFocusedWindow = pWindow;
}

CBox CAbyssXWaylandManager::getGeometryForWindow(PHLWINDOW pWindow) {
    if (!pWindow)
        return {};

    CBox box;

    if (pWindow->m_isX11)
        box = pWindow->m_xwaylandSurface->m_geometry;
    else if (pWindow->m_xdgSurface)
        box = pWindow->m_xdgSurface->m_current.geometry;

    Vector2D MINSIZE = pWindow->minSize().value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
    Vector2D MAXSIZE = pWindow->maxSize().value_or(Math::VECTOR2D_MAX).clamp(MINSIZE + Vector2D{1, 1});

    Vector2D oldSize = box.size();
    box.w            = std::clamp(box.w, MINSIZE.x, MAXSIZE.x);
    box.h            = std::clamp(box.h, MINSIZE.y, MAXSIZE.y);
    box.translate((oldSize - box.size()) / 2.F);

    return box;
}

void CAbyssXWaylandManager::sendCloseWindow(PHLWINDOW pWindow) {
    if (pWindow->m_isX11)
        pWindow->m_xwaylandSurface->close();
    else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->m_toplevel)
        pWindow->m_xdgSurface->m_toplevel->close();
}

bool CAbyssXWaylandManager::shouldBeFloated(PHLWINDOW pWindow, bool pending) {
    if (pWindow->m_isX11) {
        for (const auto& a : pWindow->m_xwaylandSurface->m_atoms)
            if (a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
                a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_DOCK"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
                a == ABYSSATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"]) {

                if (a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    pWindow->m_X11ShouldntFocus = true;

                if (a != ABYSSATOMS["_NET_WM_WINDOW_TYPE_DIALOG"])
                    pWindow->m_noInitialFocus = true;

                return true;
            }

        if (pWindow->isModal() || pWindow->m_xwaylandSurface->m_transient ||
            (pWindow->m_xwaylandSurface->m_role.contains("task_dialog") || pWindow->m_xwaylandSurface->m_role.contains("pop-up")) || pWindow->m_xwaylandSurface->m_overrideRedirect)
            return true;

        const auto SIZEHINTS = pWindow->m_xwaylandSurface->m_sizeHints.get();
        if (pWindow->m_xwaylandSurface->m_transient || pWindow->m_xwaylandSurface->m_parent ||
            (SIZEHINTS && SIZEHINTS->min_width > 0 && SIZEHINTS->min_height > 0 && SIZEHINTS->max_width > 0 && SIZEHINTS->max_height > 0 &&
             (SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height)))
            return true;
    } else {
        if (!pWindow->m_xdgSurface || !pWindow->m_xdgSurface->m_toplevel)
            return false;

        const auto PSTATE = pending ? &pWindow->m_xdgSurface->m_toplevel->m_pending : &pWindow->m_xdgSurface->m_toplevel->m_current;
        if (pWindow->m_xdgSurface->m_toplevel->m_parent ||
            (PSTATE->minSize.x != 0 && PSTATE->minSize.y != 0 && (PSTATE->minSize.x == PSTATE->maxSize.x || PSTATE->minSize.y == PSTATE->maxSize.y)))
            return true;
    }

    return false;
}

void CAbyssXWaylandManager::checkBorders(PHLWINDOW pWindow) {
    if (!pWindow->m_isX11)
        return;

    for (auto const& a : pWindow->m_xwaylandSurface->m_atoms) {
        if (a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
            a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_COMBO"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_MENU"] || a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            a == ABYSSATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {

            pWindow->m_X11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->isX11OverrideRedirect())
        pWindow->m_X11DoesntWantBorders = true;
}

void CAbyssXWaylandManager::setWindowFullscreen(PHLWINDOW pWindow, bool fullscreen) {
    if (!pWindow)
        return;

    if (pWindow->m_isX11)
        pWindow->m_xwaylandSurface->setFullscreen(fullscreen);
    else if (pWindow->m_xdgSurface && pWindow->m_xdgSurface->m_toplevel)
        pWindow->m_xdgSurface->m_toplevel->setFullscreen(fullscreen);
}

Vector2D CAbyssXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord) {
    return waylandToXWaylandCoords(coord, nullptr);
}

Vector2D CAbyssXWaylandManager::waylandToXWaylandCoords(const Vector2D& coord, PHLMONITOR preferredMonitor) {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor = preferredMonitor;
    if (!pMonitor) {
        double bestDistance = __FLT_MAX__;
        for (const auto& m : State::monitorState()->monitors()) {
            const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

            double     distance = vecToRectDistanceSquared(coord, {m->m_position.x, m->m_position.y}, {m->m_position.x + SIZ.x - 1, m->m_position.y + SIZ.y - 1});

            if (distance < bestDistance) {
                bestDistance = distance;
                pMonitor     = m;
            }
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_position;
    // if scaled, scale
    if (*PXWLFORCESCALEZERO)
        result *= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_xwaylandPosition;

    return result;
}

Vector2D CAbyssXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {
    return xwaylandToWaylandCoords(coord, nullptr);
}

Vector2D CAbyssXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord, PHLMONITOR preferredMonitor) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    PHLMONITOR  pMonitor = preferredMonitor;
    if (!pMonitor) {
        double bestDistance = __FLT_MAX__;
        for (const auto& m : State::monitorState()->monitors()) {
            const auto SIZ = *PXWLFORCESCALEZERO ? m->m_transformedSize : m->m_size;

            double     distance =
                vecToRectDistanceSquared(coord, {m->m_xwaylandPosition.x, m->m_xwaylandPosition.y}, {m->m_xwaylandPosition.x + SIZ.x - 1, m->m_xwaylandPosition.y + SIZ.y - 1});

            if (distance < bestDistance) {
                bestDistance = distance;
                pMonitor     = m;
            }
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->m_xwaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result /= pMonitor->m_scale;
    // add pos
    result += pMonitor->m_position;

    return result;
}
