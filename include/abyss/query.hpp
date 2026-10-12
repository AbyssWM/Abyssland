// Abysslang state queries: everything Hyprland 0.56.2's Lua API could read from windows, monitors, workspaces,
// layer surfaces and window groups (hl.get_* and the fields of the HL.* objects), as plain functions.
//
//   auto w = abyss::activeWindow();                       // handle (null = nothing there, like nil in Lua)
//   auto at = abyss::windowAt(w);                          // {x, y} of the window's goal position
//   for (auto& l : abyss::layers({.namespace_ = "waybar"})) ...
//
// Handles are the compositor's shared pointers (PHLWINDOW & co, see actions.hpp). Every accessor accepts a null
// handle and then returns an empty value (0 / false / "" / null / empty list / nullopt), never crashes.
// Lua returned `nil` for "not there"; here that is a null handle, std::nullopt or an empty list as noted per function.
// Pure reads: nothing here changes compositor state except monitorSetWorkspace / monitorSetSpecialWorkspace
// and groupAdd / groupRemove, which mirror the methods of the Lua objects.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "desktop/DesktopTypes.hpp"

#include <abyss/actions.hpp>

namespace Desktop::View {
    class CGroup;
}

namespace abyss {

    // ---- value types ----

    struct Point {
        int x = 0, y = 0;
    };
    struct Size {
        int width = 0, height = 0;
    };
    struct Rect {
        int x = 0, y = 0, width = 0, height = 0;
    };
    struct PointD {
        double x = 0, y = 0;
    };
    // fullscreen state of a window or workspace: 0 = none, 1 = maximized, 2 = fullscreen (3 = both, client only)
    struct FullscreenModes {
        int internal = 0; // what the compositor shows
        int client   = 0; // what the client believes
    };
    struct MonitorMode {
        int    width = 0, height = 0;
        double refreshRate = 0; // Hz
        bool   preferred   = false;
    };
    // reserved area (bars): pixels taken from each edge
    struct ReservedArea {
        double top = 0, right = 0, bottom = 0, left = 0;
    };

    // window group (tabbed group). Shared pointer to the compositor's group object.
    using PHLGROUP = SP<Desktop::View::CGroup>;

    // How a tiled window sits in its layout (HL.Window.layout). Only the fields of the active layout are set.
    struct WindowLayoutInfo {
        std::string name; // "dwindle", "master", "scrolling", "monocle", or a custom layout's name
        // master layout
        std::optional<bool>   isMaster;
        std::optional<double> percMaster, percSize;
        // scrolling layout
        std::optional<int64_t>       columnIndex;
        std::optional<double>        columnWidth;
        std::vector<PHLWINDOW>       columnWindows;
        std::optional<int64_t>       indexInColumn;
    };

    // filter for windows(): every set field has to match (like the table of hl.get_windows)
    struct WindowFilter {
        PHLMONITOR                 monitor;  // null = any
        PHLWORKSPACE               workspace; // null = any
        std::optional<bool>        floating;
        std::optional<bool>        mapped = true; // default like Lua: mapped windows only; std::nullopt = both
        std::optional<std::string> className;
        std::optional<std::string> title;
        std::optional<std::string> tag;
    };

    // filter for layers()
    struct LayerFilter {
        PHLMONITOR                 monitor; // null = any
        std::optional<std::string> namespace_;
    };

    // ======== 6a: functions (hl.get_* and friends) ========

    // windows matching all set fields of the filter (hl.get_windows{...}). windows() without arguments stays "all mapped windows".
    std::vector<PHLWINDOW> windows(const WindowFilter& filter);
    // first window that has the urgent hint, null if none (hl.get_urgent_window)
    PHLWINDOW              urgentWindow();
    // the window that had focus before the current one (hl.get_last_window)
    PHLWINDOW              lastWindow();
    // mapped windows of a workspace (hl.get_workspace_windows); empty for a null workspace
    std::vector<PHLWINDOW> workspaceWindows(const PHLWORKSPACE& ws);
    // same with a workspace selector: "3", "name:web", "special:scratch", "e+1" ... (empty if it does not resolve)
    std::vector<PHLWINDOW> workspaceWindows(std::string_view workspaceSelector);

    // workspace selector as accepted by workspace dispatchers: "3", "name:web", "special:x", "e+1", "previous", "empty" ... (hl.get_workspace)
    PHLWORKSPACE           workspaceBySelector(std::string_view selector);
    // workspace shown on a monitor (null monitor = focused monitor); the active workspace of the focused monitor is activeWorkspace()
    PHLWORKSPACE           activeWorkspace(const PHLMONITOR& monitor);
    // special workspace currently shown on a monitor, null if none (null monitor = focused monitor) (hl.get_active_special_workspace)
    PHLWORKSPACE           activeSpecialWorkspace(const PHLMONITOR& monitor = nullptr);
    // workspace that was active before the current one on the focused monitor / on the given monitor (hl.get_last_workspace)
    PHLWORKSPACE           lastWorkspace();
    PHLWORKSPACE           lastWorkspace(const PHLMONITOR& monitor);

    // monitor selector: name ("DP-1"), id ("0"), description ("desc:..."), direction ("l", "r", "u", "d") or relative ("+1", "-1"),
    // relative to the focused monitor (hl.get_monitor). Null if nothing matches.
    PHLMONITOR             monitorBySelector(std::string_view selector);
    // monitor containing the layout position (hl.get_monitor_at). Null if outside every monitor.
    PHLMONITOR             monitorAt(double x, double y);
    // monitor under the mouse cursor (hl.get_monitor_at_cursor)
    PHLMONITOR             monitorAtCursor();

    // layer surfaces of all monitors / the filtered ones (hl.get_layers)
    std::vector<PHLLS>     layers(const LayerFilter& filter = {});

    // cursor position in layout coordinates, nullopt if input is not up yet (hl.get_cursor_pos)
    std::optional<PointD>  cursorPos();
    // name of the active keybind submap, "" for the default (hl.get_current_submap)
    std::string            currentSubmap();
    // is the key physically held right now? key = xkb keysym name ("Shift_L", "a", "Return") or "code:<n>" for a keycode.
    // False (and a log line) for an unknown key. (hl.is_key_down)
    bool                   isKeyDown(std::string_view key);
    bool                   isKeyDown(int keycode);
    // compositor version, e.g. "0.56.2" (hl.version)
    std::string            version();

    // ======== 6b: window accessors (HL.Window) ========
    // Already in actions.hpp: windowAddress, windowClass, windowTitle, windowInitialClass, windowPid, windowFloating,
    // windowFullscreen (true when fullscreen/maximized), windowWorkspaceId, windowMonitorName.

    bool                      windowMapped(const PHLWINDOW& w);
    bool                      windowHidden(const PHLWINDOW& w);  // e.g. on a hidden group tab or special workspace
    bool                      windowVisible(const PHLWINDOW& w);
    bool                      windowAcceptsInput(const PHLWINDOW& w);
    Point                     windowAt(const PHLWINDOW& w);      // goal position (where the layout wants it)
    Size                      windowSize(const PHLWINDOW& w);    // goal size
    PHLWORKSPACE              windowWorkspace(const PHLWINDOW& w);
    PHLMONITOR                windowMonitor(const PHLWINDOW& w);
    std::string               windowInitialTitle(const PHLWINDOW& w);
    bool                      windowXwayland(const PHLWINDOW& w);
    bool                      windowPinned(const PHLWINDOW& w);
    bool                      windowPinFullscreened(const PHLWINDOW& w);
    FullscreenModes           windowFullscreenModes(const PHLWINDOW& w); // internal = fullscreen, client = fullscreen_client
    bool                      windowAllowedOverFullscreen(const PHLWINDOW& w);
    std::string               windowFullscreenHandler(const PHLWINDOW& w);
    PHLGROUP                  windowGroup(const PHLWINDOW& w);   // null if not grouped
    std::vector<std::string>  windowTags(const PHLWINDOW& w);
    PHLWINDOW                 windowSwallowing(const PHLWINDOW& w); // the window hidden behind this one, null if none
    int                       windowFocusHistoryId(const PHLWINDOW& w); // 0 = most recently focused, -1 = not in history
    bool                      windowInhibitingIdle(const PHLWINDOW& w);
    std::optional<std::string> windowXdgTag(const PHLWINDOW& w);
    std::optional<std::string> windowXdgDescription(const PHLWINDOW& w);
    std::string               windowContentType(const PHLWINDOW& w); // "none", "photo", "video", "game"
    int64_t                   windowStableId(const PHLWINDOW& w);
    bool                      windowActive(const PHLWINDOW& w);  // has focus
    bool                      windowTearingHint(const PHLWINDOW& w);
    // nullopt for floating windows and windows without a tiled layout (Lua: nil)
    std::optional<WindowLayoutInfo> windowLayoutInfo(const PHLWINDOW& w);

    // ======== 6b: monitor accessors (HL.Monitor) ========
    // Already in actions.hpp: monitorName.

    int                       monitorId(const PHLMONITOR& m);
    std::string               monitorDescription(const PHLMONITOR& m);
    std::string               monitorSerial(const PHLMONITOR& m);
    Size                      monitorSize(const PHLMONITOR& m);          // pixels (width/height)
    Size                      monitorPhysicalSize(const PHLMONITOR& m);  // millimeters
    double                    monitorRefreshRate(const PHLMONITOR& m);   // Hz
    Point                     monitorPosition(const PHLMONITOR& m);      // layout position (x/y)
    double                    monitorScale(const PHLMONITOR& m);
    int                       monitorTransform(const PHLMONITOR& m);     // wl_output_transform 0..7
    PHLWORKSPACE              monitorActiveWorkspace(const PHLMONITOR& m);
    PHLWORKSPACE              monitorActiveSpecialWorkspace(const PHLMONITOR& m);
    bool                      monitorDpms(const PHLMONITOR& m);          // true = on
    bool                      monitorVrrActive(const PHLMONITOR& m);
    bool                      monitorIsMirror(const PHLMONITOR& m);
    std::vector<PHLMONITOR>   monitorMirrors(const PHLMONITOR& m);       // monitors mirroring this one
    std::vector<MonitorMode>  monitorAvailableModes(const PHLMONITOR& m);
    bool                      monitorFocused(const PHLMONITOR& m);
    std::string               monitorCm(const PHLMONITOR& m);            // color management preset, e.g. "srgb", "hdr"
    ReservedArea              monitorReserved(const PHLMONITOR& m);
    // switch the monitor to a workspace (selector as for workspaceBySelector; must exist, like Lua). False if not found.
    bool                      monitorSetWorkspace(const PHLMONITOR& m, std::string_view workspaceSelector);
    bool                      monitorSetWorkspace(const PHLMONITOR& m, const PHLWORKSPACE& ws);
    // show a special workspace on the monitor. Empty selector / null workspace closes the active special workspace. False if not found.
    bool                      monitorSetSpecialWorkspace(const PHLMONITOR& m, std::string_view workspaceSelector = "");
    bool                      monitorSetSpecialWorkspace(const PHLMONITOR& m, const PHLWORKSPACE& ws);

    // ======== 6b: workspace accessors (HL.Workspace) ========
    // Already in actions.hpp: workspaceId, workspaceName, workspaceWindowCount.

    PHLMONITOR                workspaceMonitor(const PHLWORKSPACE& w);
    bool                      workspaceVisible(const PHLWORKSPACE& w);
    bool                      workspaceSpecial(const PHLWORKSPACE& w);
    bool                      workspaceActive(const PHLWORKSPACE& w);    // shown on its monitor (normal or special)
    bool                      workspaceHasUrgent(const PHLWORKSPACE& w);
    int                       workspaceFullscreenMode(const PHLWORKSPACE& w); // 0 none, 1 maximized, 2 fullscreen
    bool                      workspaceHasFullscreen(const PHLWORKSPACE& w);
    bool                      workspacePersistent(const PHLWORKSPACE& w);
    bool                      workspaceEmpty(const PHLWORKSPACE& w);
    std::string               workspaceConfigName(const PHLWORKSPACE& w);
    std::string               workspaceTiledLayout(const PHLWORKSPACE& w); // "unknown" if it has none
    PHLWINDOW                 workspaceLastWindow(const PHLWORKSPACE& w);
    PHLWINDOW                 workspaceFullscreenWindow(const PHLWORKSPACE& w);
    std::vector<PHLWINDOW>    workspaceWindows(const PHLWORKSPACE& w, bool mappedOnly); // overload helper: all (false) or mapped (true) windows
    int                       workspaceGroupCount(const PHLWORKSPACE& w);
    std::vector<PHLGROUP>     workspaceGroups(const PHLWORKSPACE& w);

    // ======== 6b: layer surface accessors (HL.LayerSurface) ========

    std::string               layerAddress(const PHLLS& l);              // "0x..."
    Rect                      layerGeometry(const PHLLS& l);
    int                       layerX(const PHLLS& l);
    int                       layerY(const PHLLS& l);
    int                       layerW(const PHLLS& l);
    int                       layerH(const PHLLS& l);
    std::string               layerNamespace(const PHLLS& l);
    int                       layerPid(const PHLLS& l);
    PHLMONITOR                layerMonitor(const PHLLS& l);
    bool                      layerMapped(const PHLLS& l);
    int                       layerLayer(const PHLLS& l);                // 0 background, 1 bottom, 2 top, 3 overlay
    int                       layerInteractivity(const PHLLS& l);        // 0 none, 1 exclusive, 2 on-demand
    bool                      layerAboveFullscreen(const PHLLS& l);

    // ======== 6b: group accessors (HL.Group) ========

    bool                      groupLocked(const PHLGROUP& g);
    bool                      groupDenied(const PHLGROUP& g);
    int                       groupSize(const PHLGROUP& g);
    int                       groupCurrentIndex(const PHLGROUP& g);      // 1-based like Lua, 0 for a null group
    PHLWINDOW                 groupCurrent(const PHLGROUP& g);
    std::vector<PHLWINDOW>    groupMembers(const PHLGROUP& g);
    // add a window to the group at a 1-based index (0 = at the end). False if not allowed (denied group, not groupable ...).
    bool                      groupAdd(const PHLGROUP& g, const PHLWINDOW& w, int index = 0);
    // remove a member (by window or 1-based index). False if it is not a member.
    bool                      groupRemove(const PHLGROUP& g, const PHLWINDOW& w);
    bool                      groupRemove(const PHLGROUP& g, int index);

} // namespace abyss
