// Abysslang actions and query helpers (include/abyss/actions.hpp).
// Not `using namespace abyss`: see Abysslang.cpp.

#include <abyss/actions.hpp>

#include <algorithm>
#include <format>

#include "AbyssConvert.hpp"

#include "../shared/dispatch/DispatcherTranslator.hpp"
#include "../shared/parserUtils/ParserUtils.hpp"
#include "../shared/actions/ConfigActions.hpp"
#include "../supplementary/executor/Executor.hpp"

#include "../../Compositor.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../desktop/Workspace.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/ViewState.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../notification/NotificationOverlay.hpp"
#include "../../output/Monitor.hpp"
#include "../../state/MonitorState.hpp"
#include "../../state/WorkspaceState.hpp"

using abyss::Action;
using abyss::Dir;

namespace {
    char dirChar(Dir d) {
        switch (d) {
            case Dir::Left: return 'l';
            case Dir::Right: return 'r';
            case Dir::Up: return 'u';
            case Dir::Down: return 'd';
        }
        return 'l';
    }

    std::string withWindow(const std::string& arg, const std::string& selector) {
        return selector.empty() ? arg : arg + "," + selector;
    }
}

SDispatchResult abyss::Action::operator()() const {
    if (fn)
        return fn();
    // the mouse dispatcher takes a 1/0 (pressed/released) prefix
    return ::Config::Dispatch::translator()->run(handler, handler == "mouse" ? "1" + arg : arg);
}

// ---- generic ----

Action abyss::dispatch(std::string name, std::string arg) {
    return Action{std::move(name), std::move(arg)};
}
Action abyss::spawn(std::string cmd) {
    return Action{"exec", std::move(cmd)};
}
Action abyss::spawnRaw(std::string cmd) {
    return Action{"execr", std::move(cmd)};
}
SDispatchResult abyss::exec(const std::string& cmd) {
    return ::Config::Dispatch::translator()->run("exec", cmd);
}

// ---- windows ----

Action abyss::killActive() {
    return {"killactive", ""};
}
Action abyss::forceKillActive() {
    return {"forcekillactive", ""};
}
Action abyss::closeWindow(std::string selector) {
    return {selector.empty() ? "killactive" : "closewindow", std::move(selector)};
}
Action abyss::killWindow(std::string selector) {
    return {"killwindow", std::move(selector)};
}
Action abyss::signal(int sig) {
    return {"signal", std::to_string(sig)};
}
Action abyss::toggleFloating(std::string selector) {
    return {"togglefloating", std::move(selector)};
}
Action abyss::setFloating(std::string selector) {
    return {"setfloating", std::move(selector)};
}
Action abyss::setTiled(std::string selector) {
    return {"settiled", std::move(selector)};
}
Action abyss::pseudo(std::string selector) {
    return {"pseudo", std::move(selector)};
}
Action abyss::pin(std::string selector) {
    return {"pin", std::move(selector)};
}
Action abyss::fullscreen(int mode, std::string state) {
    return {"fullscreen", std::format("{} {}", mode, state)};
}
Action abyss::fullscreenState(int internal, int client) {
    return {"fullscreenstate", std::format("{} {}", internal, client)};
}
Action abyss::moveFocus(Dir d) {
    return {"movefocus", std::string(1, dirChar(d))};
}
Action abyss::moveWindow(Dir d, bool silent) {
    return {"movewindow", std::string(1, dirChar(d)) + (silent ? " silent" : "")};
}
Action abyss::moveWindowToMonitor(std::string monitor, bool silent) {
    return {"movewindow", "mon:" + monitor + (silent ? " silent" : "")};
}
Action abyss::moveWindowOrGroup(Dir d) {
    return {"movewindoworgroup", std::string(1, dirChar(d))};
}
Action abyss::swapWindow(Dir d) {
    return {"swapwindow", std::string(1, dirChar(d))};
}
Action abyss::swapWith(std::string selector) {
    return {"swapwindow", std::move(selector)};
}
Action abyss::swapNext(bool forward) {
    return {"swapnext", forward ? "" : "prev"};
}
Action abyss::centerWindow() {
    return {"centerwindow", ""};
}
Action abyss::resizeActive(std::string args) {
    return {"resizeactive", std::move(args)};
}
Action abyss::moveActive(std::string args) {
    return {"moveactive", std::move(args)};
}
Action abyss::resizeActive(int dx, int dy) {
    return {"resizeactive", std::format("{} {}", dx, dy)};
}
Action abyss::moveActive(int dx, int dy) {
    return {"moveactive", std::format("{} {}", dx, dy)};
}
Action abyss::resizeWindowPixel(std::string args, std::string selector) {
    return {"resizewindowpixel", args + "," + selector};
}
Action abyss::moveWindowPixel(std::string args, std::string selector) {
    return {"movewindowpixel", args + "," + selector};
}
Action abyss::cycleNext(bool forward, bool onlyTiled, bool onlyFloating) {
    return {"cyclenext", std::string(forward ? "next" : "prev") + (onlyTiled ? " tiled" : "") + (onlyFloating ? " floating" : "")};
}
Action abyss::focusWindow(std::string selector) {
    return {"focuswindow", std::move(selector)};
}
Action abyss::focusUrgentOrLast() {
    return {"focusurgentorlast", ""};
}
Action abyss::focusCurrentOrLast() {
    return {"focuscurrentorlast", ""};
}
Action abyss::bringActiveToTop() {
    return {"bringactivetotop", ""};
}
Action abyss::alterZOrder(std::string position, std::string selector) {
    return {"alterzorder", withWindow(position, selector)};
}
Action abyss::tag(std::string tag, std::string selector) {
    return {"tagwindow", selector.empty() ? tag : tag + " " + selector};
}
Action abyss::toggleSwallow() {
    return {"toggleswallow", ""};
}
Action abyss::setProp(std::string selector, std::string prop, std::string value) {
    return {"setprop", (selector.empty() ? "active" : selector) + " " + prop + " " + value};
}
Action abyss::mouseMove() {
    return {"mouse", "movewindow"};
}
Action abyss::mouseResize(bool keepAspectRatio) {
    return {"mouse", keepAspectRatio ? "resizewindow 1" : "resizewindow"};
}

// ---- groups ----

Action abyss::toggleGroup() {
    return {"togglegroup", ""};
}
Action abyss::changeGroupActive(bool forward) {
    return {"changegroupactive", forward ? "f" : "b"};
}
Action abyss::moveGroupWindow(bool forward) {
    return {"movegroupwindow", forward ? "f" : "b"};
}
Action abyss::moveIntoGroup(Dir d) {
    return {"moveintogroup", std::string(1, dirChar(d))};
}
Action abyss::moveIntoOrCreateGroup(Dir d) {
    return {"moveintoorcreategroup", std::string(1, dirChar(d))};
}
Action abyss::moveOutOfGroup() {
    return {"moveoutofgroup", ""};
}
Action abyss::lockGroups(std::string mode) {
    return {"lockgroups", std::move(mode)};
}
Action abyss::lockActiveGroup(std::string mode) {
    return {"lockactivegroup", std::move(mode)};
}
Action abyss::denyWindowFromGroup(std::string mode) {
    return {"denywindowfromgroup", std::move(mode)};
}

// ---- workspaces / monitors ----

Action abyss::workspace(int id) {
    return {"workspace", std::to_string(id)};
}
Action abyss::workspace(std::string name) {
    return {"workspace", std::move(name)};
}
Action abyss::moveToWorkspace(int id, bool silent, std::string selector) {
    return moveToWorkspace(std::to_string(id), silent, std::move(selector));
}
Action abyss::moveToWorkspace(std::string name, bool silent, std::string selector) {
    return {silent ? "movetoworkspacesilent" : "movetoworkspace", withWindow(name, selector)};
}
Action abyss::toggleSpecial(std::string name) {
    return {"togglespecialworkspace", std::move(name)};
}
Action abyss::moveToSpecial(std::string name) {
    return {"movetoworkspace", "special:" + name};
}
Action abyss::renameWorkspace(int id, std::string name) {
    return {"renameworkspace", std::format("{} {}", id, name)};
}
Action abyss::focusMonitor(std::string monitor) {
    return {"focusmonitor", std::move(monitor)};
}
Action abyss::focusWorkspaceOnCurrentMonitor(std::string workspace) {
    return {"focusworkspaceoncurrentmonitor", std::move(workspace)};
}
Action abyss::moveCurrentWorkspaceToMonitor(std::string monitor) {
    return {"movecurrentworkspacetomonitor", std::move(monitor)};
}
Action abyss::moveWorkspaceToMonitor(std::string workspace, std::string monitor) {
    return {"moveworkspacetomonitor", workspace + " " + monitor};
}
Action abyss::swapActiveWorkspaces(std::string monitor1, std::string monitor2) {
    return {"swapactiveworkspaces", monitor1 + " " + monitor2};
}
Action abyss::dpms(std::string mode, std::string monitor) {
    return {"dpms", monitor.empty() ? mode : mode + " " + monitor};
}
Action abyss::moveCursorToCorner(int corner) {
    return {"movecursortocorner", std::to_string(corner)};
}
Action abyss::moveCursor(int x, int y) {
    return {"movecursor", std::format("{} {}", x, y)};
}

// ---- layout ----

Action abyss::layoutMsg(std::string msg) {
    return {"layoutmsg", std::move(msg)};
}
Action abyss::setLayout(std::string name) {
    return {"setlayout", std::move(name)};
}
Action abyss::toggleSplit() {
    return {"layoutmsg", "togglesplit"};
}
Action abyss::swapSplit() {
    return {"layoutmsg", "swapsplit"};
}

// ---- misc ----

Action abyss::submap(std::string name) {
    return {"submap", std::move(name)};
}
Action abyss::pass(std::string selector) {
    return {"pass", std::move(selector)};
}
static std::string modsStr(abyss::Mods m) {
    std::string out;
    for (const auto& [bit, name] : {std::pair<uint32_t, const char*>{HL_MODIFIER_SHIFT, "SHIFT"}, {HL_MODIFIER_CAPS, "CAPS"}, {HL_MODIFIER_CTRL, "CTRL"}, {HL_MODIFIER_ALT, "ALT"},
                                    {HL_MODIFIER_MOD2, "MOD2"}, {HL_MODIFIER_MOD3, "MOD3"}, {HL_MODIFIER_META, "SUPER"}, {HL_MODIFIER_MOD5, "MOD5"}}) {
        if (m.mask & bit)
            out += (out.empty() ? "" : " ") + std::string(name);
    }
    return out;
}
Action abyss::sendShortcut(Mods mods, std::string key, std::string selector) {
    return {"sendshortcut", std::format("{},{},{}", modsStr(mods), key, selector)};
}
Action abyss::sendKeyState(Mods mods, std::string key, std::string state, std::string selector) {
    return {"sendkeystate", std::format("{},{},{},{}", modsStr(mods), key, state, selector)};
}
Action abyss::global(std::string name) {
    return {"global", std::move(name)};
}
Action abyss::event(std::string data) {
    return {"event", std::move(data)};
}
Action abyss::forceRendererReload() {
    return {"forcerendererreload", ""};
}
Action abyss::forceIdle(float seconds) {
    return {"forceidle", std::format("{}", seconds)};
}
Action abyss::releaseInputCapture() {
    return {"releaseinputcapture", ""};
}
Action abyss::exit() {
    return {"exit", ""};
}

// ---- queries ----

PHLWINDOW abyss::activeWindow() {
    return Desktop::focusState()->window();
}

PHLWINDOW abyss::windowByAddress(std::string_view address) {
    return Desktop::viewState()->query().selector("address:" + std::string(address)).runWindow();
}

PHLWINDOW abyss::windowBySelector(std::string_view selector) {
    return Desktop::viewState()->query().selector(std::string(selector)).runWindow();
}

std::vector<PHLWINDOW> abyss::windows() {
    std::vector<PHLWINDOW> out;
    for (const auto& w : Desktop::windowState()->windows()) {
        if (w && w->m_isMapped)
            out.emplace_back(w);
    }
    return out;
}

PHLWORKSPACE abyss::activeWorkspace() {
    const auto MON = Desktop::focusState()->monitor();
    return MON ? MON->m_activeWorkspace : nullptr;
}

PHLWORKSPACE abyss::workspaceById(int64_t id) {
    return State::workspaceState()->query().id(id).run();
}

std::vector<PHLWORKSPACE> abyss::workspaces() {
    std::vector<PHLWORKSPACE> out;
    for (const auto& ref : State::workspaceState()->workspaces()) {
        if (const auto ws = ref.lock(); ws && !ws->inert())
            out.emplace_back(ws);
    }
    return out;
}

PHLMONITOR abyss::activeMonitor() {
    return Desktop::focusState()->monitor();
}

PHLMONITOR abyss::monitorByName(std::string_view name) {
    for (const auto& m : State::monitorState()->monitors()) {
        if (m && m->m_name == name)
            return m;
    }
    return nullptr;
}

std::vector<PHLMONITOR> abyss::monitors() {
    std::vector<PHLMONITOR> out;
    for (const auto& m : State::monitorState()->monitors())
        out.emplace_back(m);
    return out;
}

std::string abyss::windowClass(const PHLWINDOW& w) {
    return w ? w->m_class : "";
}
std::string abyss::windowTitle(const PHLWINDOW& w) {
    return w ? w->m_title : "";
}
std::string abyss::windowInitialClass(const PHLWINDOW& w) {
    return w ? w->m_initialClass : "";
}
std::string abyss::windowAddress(const PHLWINDOW& w) {
    return w ? std::format("0x{:x}", rc<uintptr_t>(w.get())) : "";
}
int abyss::windowPid(const PHLWINDOW& w) {
    return w ? w->getPID() : -1;
}
bool abyss::windowFloating(const PHLWINDOW& w) {
    return w && w->m_isFloating;
}
bool abyss::windowFullscreen(const PHLWINDOW& w) {
    return w && Fullscreen::controller()->isFullscreen(w);
}
int64_t abyss::windowWorkspaceId(const PHLWINDOW& w) {
    return w && w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID;
}
std::string abyss::windowMonitorName(const PHLWINDOW& w) {
    return w && w->m_workspace && w->m_workspace->m_monitor ? w->m_workspace->m_monitor->m_name : "";
}
std::string abyss::byAddress(const PHLWINDOW& w) {
    return "address:" + windowAddress(w);
}

int64_t abyss::workspaceId(const PHLWORKSPACE& w) {
    return w ? w->m_id : WORKSPACE_INVALID;
}
int64_t abyss::workspaceId(const PHLWORKSPACEREF& w) {
    const auto ws = w.lock();
    return ws ? ws->m_id : WORKSPACE_INVALID;
}
std::string abyss::workspaceName(const PHLWORKSPACE& w) {
    return w ? w->m_name : "";
}
std::string abyss::workspaceName(const PHLWORKSPACEREF& w) {
    const auto ws = w.lock();
    return ws ? ws->m_name : "";
}
int abyss::workspaceWindowCount(const PHLWORKSPACE& w) {
    return w ? w->getWindowCount() : 0;
}
std::string abyss::monitorName(const PHLMONITOR& m) {
    return m ? m->m_name : "";
}

void abyss::notify(const std::string& text, int ms, std::optional<Color> color, const std::string& icon) {
    if (!Notification::overlay())
        return;

    std::string lc = icon;
    std::ranges::transform(lc, lc.begin(), ::tolower);
    eIcons ic = ICON_INFO;
    if (lc == "warning" || lc == "warn")
        ic = ICON_WARNING;
    else if (lc == "error" || lc == "err")
        ic = ICON_ERROR;
    else if (lc == "hint")
        ic = ICON_HINT;
    else if (lc == "confused" || lc == "question")
        ic = ICON_CONFUSED;
    else if (lc == "ok")
        ic = ICON_OK;
    else if (lc == "none")
        ic = ICON_NONE;

    Notification::overlay()->addNotification(text, color ? CAbyssColor(sc<uint64_t>(color->argb)) : CAbyssColor(0), sc<float>(ms), ic);
}
