// Abysslang state queries (include/abyss/query.hpp). Semantics follow Hyprland 0.56.2's Lua API
// (src/config/lua/bindings/LuaBindingsQuery.cpp and src/config/lua/objects/Lua*.cpp).
// Not `using namespace abyss`: see Abysslang.cpp.

#include <abyss/query.hpp>

#include <algorithm>
#include <format>

#include <xkbcommon/xkbcommon.h>

#include "../../Compositor.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../desktop/Workspace.hpp"
#include "../../desktop/history/WindowHistoryTracker.hpp"
#include "../../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/ViewState.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../helpers/CMType.hpp"
#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/algorithm/TiledAlgorithm.hpp"
#include "../../layout/algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../../layout/algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../../layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../protocols/types/ContentType.hpp"
#include "../../state/MonitorState.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../shared/actions/ConfigActions.hpp"

using namespace abyss;

namespace {
    using Desktop::View::IGeometric;

    bool usable(const PHLWORKSPACE& ws) {
        return ws && !ws->inert();
    }

    PHLMONITOR monitorOrFocused(const PHLMONITOR& m) {
        return m ? m : Desktop::focusState()->monitor();
    }

    bool windowMatches(const PHLWINDOW& w, const WindowFilter& f) {
        if (!w)
            return false;
        if (f.monitor && w->m_monitor.lock() != f.monitor)
            return false;
        if (f.workspace && w->m_workspace != f.workspace)
            return false;
        if (f.floating && w->m_isFloating != *f.floating)
            return false;
        if (f.mapped && w->m_isMapped != *f.mapped)
            return false;
        if (f.className && w->m_class != *f.className)
            return false;
        if (f.title && w->m_title != *f.title)
            return false;
        if (f.tag && (!w->m_ruleApplicator || !w->m_ruleApplicator->m_tagKeeper.isTagged(*f.tag, true)))
            return false;
        return true;
    }

    bool keyDownByKeycode(int keycode) {
        if (!g_pKeybindManager)
            return false;
        for (const auto& k : g_pKeybindManager->m_pressedKeys) {
            if (int(k.keycode) == keycode)
                return true;
        }
        return false;
    }

    PHLWORKSPACE workspaceByName(std::string_view s) {
        auto ws = State::workspaceState()->query().name(std::string(s)).run();
        if (!usable(ws))
            ws = State::workspaceState()->query().string(std::string(s)).run();
        return usable(ws) ? ws : nullptr;
    }
}

// ======== 6a ========

std::vector<PHLWINDOW> abyss::windows(const WindowFilter& filter) {
    std::vector<PHLWINDOW> out;
    for (const auto& w : Desktop::windowState()->windows()) {
        if (windowMatches(w, filter))
            out.emplace_back(w);
    }
    return out;
}

PHLWINDOW abyss::urgentWindow() {
    return Desktop::viewState()->query().urgent().runWindow();
}

PHLWINDOW abyss::lastWindow() {
    const auto  current = Desktop::focusState()->window();
    const auto& history = Desktop::History::windowTracker()->fullHistory();
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        const auto candidate = it->lock();
        if (!candidate || !candidate->m_isMapped)
            continue;
        if (current && candidate == current)
            continue;
        return candidate;
    }
    return nullptr;
}

std::vector<PHLWINDOW> abyss::workspaceWindows(const PHLWORKSPACE& ws) {
    if (!ws)
        return {};
    return workspaceWindows(ws, true);
}

std::vector<PHLWINDOW> abyss::workspaceWindows(std::string_view workspaceSelector) {
    return workspaceWindows(workspaceBySelector(workspaceSelector));
}

PHLWORKSPACE abyss::workspaceBySelector(std::string_view selector) {
    const auto ws = State::workspaceState()->query().string(std::string(selector)).run();
    return usable(ws) ? ws : nullptr;
}

PHLWORKSPACE abyss::activeWorkspace(const PHLMONITOR& monitor) {
    const auto m = monitorOrFocused(monitor);
    return m ? m->m_activeWorkspace : nullptr;
}

PHLWORKSPACE abyss::activeSpecialWorkspace(const PHLMONITOR& monitor) {
    const auto m = monitorOrFocused(monitor);
    return m ? m->m_activeSpecialWorkspace : nullptr;
}

static PHLWORKSPACE lastWorkspaceImpl(const PHLMONITOR& mon, bool withMonitorArg) {
    if (!mon || !mon->m_activeWorkspace)
        return nullptr;

    const auto current  = mon->m_activeWorkspace;
    auto       previous = withMonitorArg ? Desktop::History::workspaceTracker()->previousWorkspace(current, mon) : Desktop::History::workspaceTracker()->previousWorkspace(current);

    auto       ws = previous.workspace.lock();
    if ((!ws || ws->inert()) && previous.id != WORKSPACE_INVALID)
        ws = State::workspaceState()->query().id(previous.id).run();

    return usable(ws) ? ws : nullptr;
}

PHLWORKSPACE abyss::lastWorkspace() {
    return lastWorkspaceImpl(Desktop::focusState()->monitor(), false);
}

PHLWORKSPACE abyss::lastWorkspace(const PHLMONITOR& monitor) {
    return lastWorkspaceImpl(monitorOrFocused(monitor), !!monitor);
}

PHLMONITOR abyss::monitorBySelector(std::string_view selector) {
    return State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(std::string(selector)).run();
}

PHLMONITOR abyss::monitorAt(double x, double y) {
    return State::monitorState()->query().vec(Vector2D{x, y}).run();
}

PHLMONITOR abyss::monitorAtCursor() {
    if (!g_pInputManager)
        return nullptr;
    return State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
}

std::vector<PHLLS> abyss::layers(const LayerFilter& filter) {
    std::vector<PHLLS> out;
    for (const auto& mon : State::monitorState()->monitors()) {
        if (filter.monitor && mon != filter.monitor)
            continue;
        for (const auto& level : mon->m_layerSurfaceLayers) {
            for (const auto& ref : level) {
                const auto ls = ref.lock();
                if (!ls)
                    continue;
                if (filter.namespace_ && ls->m_namespace != *filter.namespace_)
                    continue;
                out.emplace_back(ls);
            }
        }
    }
    return out;
}

std::optional<PointD> abyss::cursorPos() {
    if (!g_pInputManager)
        return std::nullopt;
    const auto p = g_pInputManager->getMouseCoordsInternal();
    return PointD{p.x, p.y};
}

std::string abyss::currentSubmap() {
    return Config::Actions::state()->m_currentSubmap;
}

bool abyss::isKeyDown(int keycode) {
    if (!xkb_keycode_is_legal_x11(keycode) && !xkb_keycode_is_legal_ext(keycode)) {
        Log::logger->log(Log::ERR, "abyss::isKeyDown: invalid keycode {}", keycode);
        return false;
    }
    return keyDownByKeycode(keycode);
}

bool abyss::isKeyDown(std::string_view key) {
    if (key.starts_with("code:")) {
        try {
            return isKeyDown(std::stoi(std::string(key.substr(5))));
        } catch (...) {
            Log::logger->log(Log::ERR, "abyss::isKeyDown: bad keycode \"{}\"", key);
            return false;
        }
    }

    const std::string name(key);
    const auto        sym = xkb_keysym_from_name(name.c_str(), XKB_KEYSYM_NO_FLAGS);
    if (sym == XKB_KEY_NoSymbol) {
        Log::logger->log(Log::ERR, "abyss::isKeyDown: unknown keysym \"{}\"{}", name, name == "Enter" ? R"(, did you mean "Return"?)" : "");
        return false;
    }

    if (!g_pKeybindManager)
        return false;
    for (const auto& k : g_pKeybindManager->m_pressedKeys) {
        if (k.keysym == sym)
            return true;
    }
    return false;
}

std::string abyss::version() {
    return ABYSSLAND_VERSION;
}

// ======== window ========

bool abyss::windowMapped(const PHLWINDOW& w) {
    return w && w->m_isMapped;
}
bool abyss::windowHidden(const PHLWINDOW& w) {
    return w && w->isHidden();
}
bool abyss::windowVisible(const PHLWINDOW& w) {
    return w && w->visible();
}
bool abyss::windowAcceptsInput(const PHLWINDOW& w) {
    return w && w->acceptsInput();
}
Point abyss::windowAt(const PHLWINDOW& w) {
    if (!w)
        return {};
    const auto p = w->position(IGeometric::GEOMETRIC_GOAL);
    return {int(p.x), int(p.y)};
}
Size abyss::windowSize(const PHLWINDOW& w) {
    if (!w)
        return {};
    const auto s = w->size(IGeometric::GEOMETRIC_GOAL);
    return {int(s.x), int(s.y)};
}
PHLWORKSPACE abyss::windowWorkspace(const PHLWINDOW& w) {
    return w ? w->m_workspace : nullptr;
}
PHLMONITOR abyss::windowMonitor(const PHLWINDOW& w) {
    return w ? w->m_monitor.lock() : nullptr;
}
std::string abyss::windowInitialTitle(const PHLWINDOW& w) {
    return w ? w->m_initialTitle : "";
}
bool abyss::windowXwayland(const PHLWINDOW& w) {
    return w && w->m_isX11;
}
bool abyss::windowPinned(const PHLWINDOW& w) {
    return w && w->m_pinned;
}
bool abyss::windowPinFullscreened(const PHLWINDOW& w) {
    return w && w->m_pinFullscreened;
}
FullscreenModes abyss::windowFullscreenModes(const PHLWINDOW& w) {
    if (!w)
        return {};
    const auto m = Fullscreen::controller()->getFullscreenModes(w);
    return {int(uint8_t(m.internal)), int(uint8_t(m.client))};
}
bool abyss::windowAllowedOverFullscreen(const PHLWINDOW& w) {
    return w && w->m_allowedOverFullscreen;
}
std::string abyss::windowFullscreenHandler(const PHLWINDOW& w) {
    return w ? Fullscreen::controller()->getFullscreenHandlerNameAsString(w) : "";
}
PHLGROUP abyss::windowGroup(const PHLWINDOW& w) {
    return w ? w->m_group : nullptr;
}
std::vector<std::string> abyss::windowTags(const PHLWINDOW& w) {
    std::vector<std::string> out;
    if (!w || !w->m_ruleApplicator)
        return out;
    for (const auto& t : w->m_ruleApplicator->m_tagKeeper.getTags())
        out.emplace_back(t);
    return out;
}
PHLWINDOW abyss::windowSwallowing(const PHLWINDOW& w) {
    return w ? w->m_swallowee.lock() : nullptr;
}
int abyss::windowFocusHistoryId(const PHLWINDOW& w) {
    if (!w)
        return -1;
    const auto& history = Desktop::History::windowTracker()->fullHistory();
    for (size_t i = 0; i < history.size(); ++i) {
        if (history[i].lock() == w)
            return int(history.size() - i - 1);
    }
    return -1;
}
bool abyss::windowInhibitingIdle(const PHLWINDOW& w) {
    return w && g_pInputManager && g_pInputManager->isWindowInhibiting(w, false);
}
std::optional<std::string> abyss::windowXdgTag(const PHLWINDOW& w) {
    return w ? w->xdgTag() : std::nullopt;
}
std::optional<std::string> abyss::windowXdgDescription(const PHLWINDOW& w) {
    return w ? w->xdgDescription() : std::nullopt;
}
std::string abyss::windowContentType(const PHLWINDOW& w) {
    return w ? NContentType::toString(w->getContentType()) : "";
}
int64_t abyss::windowStableId(const PHLWINDOW& w) {
    return w ? int64_t(w->m_stableID) : 0;
}
bool abyss::windowActive(const PHLWINDOW& w) {
    return w && w == Desktop::focusState()->window();
}
bool abyss::windowTearingHint(const PHLWINDOW& w) {
    return w && w->m_tearingHint;
}

std::optional<WindowLayoutInfo> abyss::windowLayoutInfo(const PHLWINDOW& w) {
    if (!w)
        return std::nullopt;

    const auto target = w->layoutTarget();
    if (!target || target->floating() || !w->m_workspace || !w->m_workspace->m_space)
        return std::nullopt;

    const auto& algo = w->m_workspace->m_space->algorithm();
    if (!algo || !algo->tiledAlgo())
        return std::nullopt;

    const auto&      tiledAlgo = algo->tiledAlgo();
    WindowLayoutInfo info;
    info.name = Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(tiledAlgo.get());

    if (const auto* master = dynamic_cast<Layout::Tiled::CMasterAlgorithm*>(tiledAlgo.get())) {
        if (const auto node = master->getNodeFromTarget(target)) {
            info.isMaster   = node->isMaster;
            info.percMaster = node->percMaster;
            info.percSize   = node->percSize;
        }
    } else if (auto* scrolling = dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(tiledAlgo.get())) {
        if (const auto data = scrolling->dataFor(target)) {
            if (const auto col = data->column.lock()) {
                if (const auto sd = col->scrollingData.lock())
                    info.columnIndex = int64_t(sd->idx(col));
                info.columnWidth = col->getColumnWidth();
                for (const auto& td : col->targetDatas) {
                    const auto t = td->target.lock();
                    if (!t)
                        continue;
                    if (const auto win = t->window())
                        info.columnWindows.emplace_back(win);
                }
                info.indexInColumn = int64_t(col->idx(target));
            }
        }
    }

    return info;
}

// ======== monitor ========

int abyss::monitorId(const PHLMONITOR& m) {
    return m ? int(m->m_id) : -1;
}
std::string abyss::monitorDescription(const PHLMONITOR& m) {
    return m ? m->m_shortDescription : "";
}
std::string abyss::monitorSerial(const PHLMONITOR& m) {
    return m && m->m_output ? m->m_output->serial : "";
}
Size abyss::monitorSize(const PHLMONITOR& m) {
    return m ? Size{int(m->m_pixelSize.x), int(m->m_pixelSize.y)} : Size{};
}
Size abyss::monitorPhysicalSize(const PHLMONITOR& m) {
    return m && m->m_output ? Size{int(m->m_output->physicalSize.x), int(m->m_output->physicalSize.y)} : Size{};
}
double abyss::monitorRefreshRate(const PHLMONITOR& m) {
    return m ? m->m_refreshRate : 0;
}
Point abyss::monitorPosition(const PHLMONITOR& m) {
    return m ? Point{int(m->m_position.x), int(m->m_position.y)} : Point{};
}
double abyss::monitorScale(const PHLMONITOR& m) {
    return m ? m->m_scale : 0;
}
int abyss::monitorTransform(const PHLMONITOR& m) {
    return m ? int(m->m_transform) : 0;
}
PHLWORKSPACE abyss::monitorActiveWorkspace(const PHLMONITOR& m) {
    return m ? m->m_activeWorkspace : nullptr;
}
PHLWORKSPACE abyss::monitorActiveSpecialWorkspace(const PHLMONITOR& m) {
    return m ? m->m_activeSpecialWorkspace : nullptr;
}
bool abyss::monitorDpms(const PHLMONITOR& m) {
    return m && m->m_dpmsStatus;
}
bool abyss::monitorVrrActive(const PHLMONITOR& m) {
    return m && m->m_vrrActive;
}
bool abyss::monitorIsMirror(const PHLMONITOR& m) {
    return m && m->isMirror();
}
std::vector<PHLMONITOR> abyss::monitorMirrors(const PHLMONITOR& m) {
    std::vector<PHLMONITOR> out;
    if (!m)
        return out;
    for (const auto& ref : m->m_mirrors) {
        if (const auto mirror = ref.lock())
            out.emplace_back(mirror);
    }
    return out;
}
std::vector<MonitorMode> abyss::monitorAvailableModes(const PHLMONITOR& m) {
    std::vector<MonitorMode> out;
    if (!m || !m->m_output)
        return out;
    for (const auto& mode : m->m_output->modes) {
        if (!mode)
            continue;
        out.push_back({int(mode->pixelSize.x), int(mode->pixelSize.y), mode->refreshRate / 1000.0, mode->preferred});
    }
    return out;
}
bool abyss::monitorFocused(const PHLMONITOR& m) {
    return m && m == Desktop::focusState()->monitor();
}
std::string abyss::monitorCm(const PHLMONITOR& m) {
    return m ? NCMType::toString(m->m_cmType) : "";
}
ReservedArea abyss::monitorReserved(const PHLMONITOR& m) {
    if (!m)
        return {};
    return {m->m_reservedArea.top(), m->m_reservedArea.right(), m->m_reservedArea.bottom(), m->m_reservedArea.left()};
}
bool abyss::monitorSetWorkspace(const PHLMONITOR& m, const PHLWORKSPACE& ws) {
    if (!m || !usable(ws))
        return false;
    m->changeWorkspace(ws->m_id);
    return true;
}
bool abyss::monitorSetWorkspace(const PHLMONITOR& m, std::string_view workspaceSelector) {
    if (workspaceSelector.empty())
        return false;
    return monitorSetWorkspace(m, workspaceByName(workspaceSelector));
}
bool abyss::monitorSetSpecialWorkspace(const PHLMONITOR& m, const PHLWORKSPACE& ws) {
    if (!m)
        return false;
    if (!ws) {
        m->setSpecialWorkspace(WORKSPACE_INVALID);
        return true;
    }
    if (!usable(ws))
        return false;
    m->setSpecialWorkspace(ws->m_id);
    return true;
}
bool abyss::monitorSetSpecialWorkspace(const PHLMONITOR& m, std::string_view workspaceSelector) {
    if (workspaceSelector.empty())
        return monitorSetSpecialWorkspace(m, PHLWORKSPACE{});
    const auto ws = workspaceByName(workspaceSelector);
    if (!ws)
        return false;
    return monitorSetSpecialWorkspace(m, ws);
}

// ======== workspace ========

PHLMONITOR abyss::workspaceMonitor(const PHLWORKSPACE& w) {
    return usable(w) ? w->m_monitor.lock() : nullptr;
}
bool abyss::workspaceVisible(const PHLWORKSPACE& w) {
    return usable(w) && w->isVisible();
}
bool abyss::workspaceSpecial(const PHLWORKSPACE& w) {
    return usable(w) && w->m_isSpecialWorkspace;
}
bool abyss::workspaceActive(const PHLWORKSPACE& w) {
    if (!usable(w))
        return false;
    const auto mon = w->m_monitor.lock();
    return mon && (mon->m_activeWorkspace == w || mon->m_activeSpecialWorkspace == w);
}
bool abyss::workspaceHasUrgent(const PHLWORKSPACE& w) {
    return usable(w) && w->hasUrgentWindow();
}
int abyss::workspaceFullscreenMode(const PHLWORKSPACE& w) {
    return usable(w) ? int(Fullscreen::controller()->getFullscreenModes(w).internal) : 0;
}
bool abyss::workspaceHasFullscreen(const PHLWORKSPACE& w) {
    return usable(w) && Fullscreen::controller()->hasFullscreen(w);
}
bool abyss::workspacePersistent(const PHLWORKSPACE& w) {
    return usable(w) && w->isPersistent();
}
bool abyss::workspaceEmpty(const PHLWORKSPACE& w) {
    return usable(w) && w->getWindowCount() == 0;
}
std::string abyss::workspaceConfigName(const PHLWORKSPACE& w) {
    return usable(w) ? w->getConfigName() : "";
}
std::string abyss::workspaceTiledLayout(const PHLWORKSPACE& w) {
    if (!usable(w))
        return "";
    if (w->m_space && w->m_space->algorithm() && w->m_space->algorithm()->tiledAlgo())
        return Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(w->m_space->algorithm()->tiledAlgo().get());
    return "unknown";
}
PHLWINDOW abyss::workspaceLastWindow(const PHLWORKSPACE& w) {
    return usable(w) ? w->m_lastFocusedWindow.lock() : nullptr;
}
PHLWINDOW abyss::workspaceFullscreenWindow(const PHLWORKSPACE& w) {
    return usable(w) ? Fullscreen::controller()->getFullscreenWindow(w) : nullptr;
}
std::vector<PHLWINDOW> abyss::workspaceWindows(const PHLWORKSPACE& ws, bool mappedOnly) {
    std::vector<PHLWINDOW> out;
    if (!usable(ws))
        return out;
    for (const auto& w : Desktop::windowState()->windows()) {
        if (w && w->m_workspace == ws && (!mappedOnly || w->m_isMapped))
            out.emplace_back(w);
    }
    return out;
}
int abyss::workspaceGroupCount(const PHLWORKSPACE& w) {
    return usable(w) ? w->getGroups() : 0;
}
std::vector<PHLGROUP> abyss::workspaceGroups(const PHLWORKSPACE& ws) {
    std::vector<PHLGROUP> out;
    if (!usable(ws))
        return out;
    for (const auto& w : Desktop::windowState()->windows()) {
        if (!w || w->m_workspace != ws || !w->m_group)
            continue;
        if (std::ranges::find(out, w->m_group) != out.end())
            continue;
        out.emplace_back(w->m_group);
    }
    return out;
}

// ======== layer surface ========

std::string abyss::layerAddress(const PHLLS& l) {
    return l ? std::format("0x{:x}", reinterpret_cast<uintptr_t>(l.get())) : "";
}
Rect abyss::layerGeometry(const PHLLS& l) {
    return l ? Rect{int(l->m_geometry.x), int(l->m_geometry.y), int(l->m_geometry.width), int(l->m_geometry.height)} : Rect{};
}
int abyss::layerX(const PHLLS& l) {
    return l ? int(l->m_geometry.x) : 0;
}
int abyss::layerY(const PHLLS& l) {
    return l ? int(l->m_geometry.y) : 0;
}
int abyss::layerW(const PHLLS& l) {
    return l ? int(l->m_geometry.width) : 0;
}
int abyss::layerH(const PHLLS& l) {
    return l ? int(l->m_geometry.height) : 0;
}
std::string abyss::layerNamespace(const PHLLS& l) {
    return l ? l->m_namespace : "";
}
int abyss::layerPid(const PHLLS& l) {
    return l ? int(l->getPID()) : 0;
}
PHLMONITOR abyss::layerMonitor(const PHLLS& l) {
    return l ? l->m_monitor.lock() : nullptr;
}
bool abyss::layerMapped(const PHLLS& l) {
    return l && l->m_mapped;
}
int abyss::layerLayer(const PHLLS& l) {
    return l ? int(l->m_layer) : 0;
}
int abyss::layerInteractivity(const PHLLS& l) {
    return l ? int(l->m_interactivity) : 0;
}
bool abyss::layerAboveFullscreen(const PHLLS& l) {
    return l && l->m_aboveFullscreen;
}

// ======== group ========

bool abyss::groupLocked(const PHLGROUP& g) {
    return g && g->locked();
}
bool abyss::groupDenied(const PHLGROUP& g) {
    return g && g->denied();
}
int abyss::groupSize(const PHLGROUP& g) {
    return g ? int(g->size()) : 0;
}
int abyss::groupCurrentIndex(const PHLGROUP& g) {
    return g ? int(g->getCurrentIdx()) + 1 : 0;
}
PHLWINDOW abyss::groupCurrent(const PHLGROUP& g) {
    return g ? g->current() : nullptr;
}
std::vector<PHLWINDOW> abyss::groupMembers(const PHLGROUP& g) {
    std::vector<PHLWINDOW> out;
    if (!g)
        return out;
    for (const auto& ref : g->windows()) {
        if (const auto w = ref.lock())
            out.emplace_back(w);
    }
    return out;
}
bool abyss::groupAdd(const PHLGROUP& g, const PHLWINDOW& w, int index) {
    if (!g || !w)
        return false;

    std::optional<size_t> idx;
    if (index != 0) {
        if (index < 1 || size_t(index) > g->size() + 1)
            return false;
        idx = size_t(index - 1);
    }

    if (w->m_group == g)
        return false;
    if (g->denied() || !w->canBeGroupedInto(g))
        return false;

    g->add(w, idx);
    return true;
}
bool abyss::groupRemove(const PHLGROUP& g, const PHLWINDOW& w) {
    if (!g || !w || !g->has(w))
        return false;
    g->remove(w);
    return true;
}
bool abyss::groupRemove(const PHLGROUP& g, int index) {
    if (!g || index < 1 || size_t(index) > g->size())
        return false;
    return groupRemove(g, g->fromIndex(size_t(index - 1)));
}
