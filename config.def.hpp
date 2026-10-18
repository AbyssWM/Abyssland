// Abyssland configuration, compiled into the compositor (like dwm's config.h).
//
// This file was copied from config.def.hpp. Edit it, then rebuild; changing it recompiles only the config.
// Options live in typed fields (c.general.gaps_in, c.input.kb_layout, ...): a typo is a compile error.
// Everything C++ can do is allowed in here, including your own lambdas as keybind actions.
#include <abyss/config.hpp>

using namespace abyss;

void configure(Config& c) {
    // ---- programs ----
    const std::string terminal    = "kitty";
    const std::string fileManager = "dolphin";
    const std::string menu        = "wofi --show drun";

    // ---- monitors ----
    // c.monitor("DP-1", "2560x1440@144", "0x0", 1.0);
    c.monitor("", "preferred", "auto", 1.0); // every other monitor: automatic

    // ---- environment ----
    c.cursor.size = 24;

    // ---- autostart ----
    // c.onStart([] { exec("quickshell"); });

    // ---- idle (nothing is on by default) ----
    // c.idle(300, [] { exec("abysslock"); });                           // after 5 minutes without input
    // c.idle(600, [] { exec("..."); }, [] { exec("..."); });            // onIdle, optional onResume (first input afterwards)
    // screensaver (fullscreen, drawn by the compositor, the first input only wakes it up; `abyssctl dispatch screensaver` shows it now):
    // c.idle(300, screensaver(saver::clock()));                                          // clock + date, 1 Hz
    // c.idle(300, screensaver(saver::clock({.color = 0xc8a25a, .size = 200, .format = "%H:%M:%S"})));
    // c.idle(300, screensaver(saver::cava({.bars = 64, .color = 0x80b0d8})));            // audio bars (needs the cava program)
    // your own (called once per monitor and frame; see abyss/saver.hpp for Canvas / SaverFrame):
    // c.idle(300, screensaver([](Canvas& g, const SaverFrame& f) {
    //     g.clear(0x000000);
    //     g.text(f.width / 2, f.height / 2, "hello", {.size = 64, .color = 0xc8a25a, .align = Align::Center});
    // }, {.fps = 30}));

    // ---- GPU (read at start; see https://abysswm.github.io/wiki/nvidia) ----
    // render on one card only (paths that do not exist are skipped, so the same config works on other machines):
    // c.gpu.devices = {"/dev/dri/by-path/pci-0000:0b:00.0-card"};
    // c.gpu.noExplicitMgpu = true;   // ABYSSAL_MGPU_NO_EXPLICIT, multi-GPU set-ups with sync problems

    // ---- helper programs (defaults come with Abyssland; see c.helpers in abyss/config.hpp for the contract) ----
    // c.helpers.dialog = "zenity-wrapper";   // own dialog program, "" = no dialogs at all

    // ---- protocols (everything is offered by default; `abyssctl protocols` lists the names, see abyss/protocols.hpp) ----
    // only wlr screencopy (grim, wf-recorder, xdg-desktop-portal-wlr), none of ours / the ext ones:
    // c.protocols.disable({"abyssland_toplevel_export_manager_v1", "ext_image_copy_capture_manager_v1",
    //                      "ext_output_image_capture_source_manager_v1", "ext_foreign_toplevel_image_capture_source_manager_v1"});
    // our own screencopy instead of wlr:  c.protocol("zwlr_screencopy_manager_v1", false);
    // c.protocol("zwlr_screencopy_manager_v1", {.maxVersion = 2});   // offer at most version 2
    // NVIDIA: leave wp_linux_drm_syncobj_manager_v1 on, switching it off causes flickering.

    // ---- look and feel ----
    c.general.gaps_in      = 5;
    c.general.gaps_out     = 20;
    c.general.border_size  = 2;
    c.general.col.active_border   = gradient({rgba(0x4a7bd8, 0xee), rgba(0x1b2f6e, 0xee)}, 45);
    c.general.col.inactive_border = rgba(0x1a2338, 0xaa);
    c.general.resize_on_border = false;
    c.general.allow_tearing    = false;
    c.general.layout           = "dwindle";

    c.decoration.rounding        = 10;
    c.decoration.rounding_power  = 2;
    c.decoration.active_opacity   = 1.0;
    c.decoration.inactive_opacity = 1.0;
    c.decoration.shadow.enabled      = true;
    c.decoration.shadow.range        = 4;
    c.decoration.shadow.render_power = 3;
    c.decoration.shadow.color        = rgba(0x03060f, 0xee);
    c.decoration.blur.enabled   = true;
    c.decoration.blur.size      = 3;
    c.decoration.blur.passes    = 1;
    c.decoration.blur.vibrancy  = 0.1696;

    c.misc.force_default_wallpaper = -1;

    // ---- animations ----
    c.animations.enabled = true;
    c.bezier("easeOutQuint", 0.23, 1, 0.32, 1);
    c.bezier("easeInOutCubic", 0.65, 0.05, 0.36, 1);
    c.bezier("linear", 0, 0, 1, 1);
    c.bezier("almostLinear", 0.5, 0.5, 0.75, 1);
    c.bezier("quick", 0.15, 0, 0.1, 1);
    // a spring curve instead of a bezier: c.spring("easy", 1, 878.5, 59.29);
    c.animation("global", true, 10, "default");
    c.animation("border", true, 5.39, "easeOutQuint");
    c.animation("windows", true, 4.79, "easeOutQuint");
    c.animation("windowsIn", true, 4.1, "easeOutQuint", "popin 87%");
    c.animation("windowsOut", true, 1.49, "linear", "popin 87%");
    c.animation("fadeIn", true, 1.73, "almostLinear");
    c.animation("fadeOut", true, 1.46, "almostLinear");
    c.animation("fade", true, 3.03, "quick");
    c.animation("layers", true, 3.81, "easeOutQuint");
    c.animation("layersIn", true, 4, "easeOutQuint", "fade");
    c.animation("layersOut", true, 1.5, "linear", "fade");
    c.animation("fadeLayersIn", true, 1.79, "almostLinear");
    c.animation("fadeLayersOut", true, 1.39, "almostLinear");
    c.animation("workspaces", true, 1.94, "almostLinear", "fade");
    c.animation("workspacesIn", true, 1.21, "almostLinear", "fade");
    c.animation("workspacesOut", true, 1.94, "almostLinear", "fade");

    // ---- layouts ----
    // your own layout, any C++: put every window into a grid (select it with c.general.layout = "grid" or a workspace rule).
    // Full-power layouts (state, resize, swap, messages) derive from abyss::TiledLayout: see abyss/layout.hpp and examples/centered-master.hpp
    c.addLayout("grid", [](LayoutCtx& l) {
        const auto cells = l.grid(l.count());
        for (size_t i = 0; i < l.count(); ++i)
            l.place(i, cells[i]);
    });
    c.dwindle.preserve_split = true;
    c.master.new_status      = "master";

    // ---- input ----
    c.input.kb_layout     = "us";
    c.input.follow_mouse  = 1;
    c.input.sensitivity   = 0;
    c.input.touchpad.natural_scroll = false;
    // per-device settings: c.device("logitech-mx-master", {.sensitivity = -0.5});
    // touchpad, touchscreen and tablet options (same names as the old config, '-' becomes '_'):
    // c.input.touchpad.tap_to_click = true;
    // c.input.touchpad.disable_while_typing = true;
    // c.input.touchdevice.output = "DP-1";            // bind touchscreens to a monitor
    // c.input.tablet.output = "DP-1";
    // c.input.tablettool.pressure_range_max = 0.9;
    // c.device("wacom-intuos-pen", {.leftHanded = true, .output = "DP-1"});
    // c.device("my-touchpad", {.naturalScroll = true, .tapToClick = true, .tags = "touchpad"});  // "tags" (keyboards and pointers/touchpads) are matched by BindOpts::devices
    // touchpad gestures: c.gesture(fingers, direction, action, {.mods, .scale, ...}); fingers 2..9
    // c.gesture(3, "horizontal", "workspace");
    // c.gesture(4, "pinch", "fullscreen", {.mode = "fullscreen"});
    // c.gesture(3, "down", [] { exec("grim"); });      // callback when the gesture ends
    // c.gesture(4, "swipe", GestureCallbacks{.update = [](const GestureEvent& e) { /* e.delta, e.direction, e.fingers ... */ }});

    // ---- keybinds ----
    // Actions (killActive, workspace, ...) are deferred values. exec runs inside a callback.
    c.bind(SUPER, "Q", [terminal] { return exec(terminal); });
    c.bind(SUPER, "C", killActive());
    c.bind(SUPER, "M", exit());
    c.bind(SUPER, "E", [fileManager] { return exec(fileManager); });
    c.bind(SUPER, "V", toggleFloating());
    c.bind(SUPER, "R", [menu] { return exec(menu); });
    c.bind(SUPER, "P", pseudo());
    c.bind(SUPER, "J", toggleSplit());

    // focus with arrow keys
    c.bind(SUPER, "left", moveFocus(Dir::Left));
    c.bind(SUPER, "right", moveFocus(Dir::Right));
    c.bind(SUPER, "up", moveFocus(Dir::Up));
    c.bind(SUPER, "down", moveFocus(Dir::Down));

    // workspaces 1..10 (key "0" is workspace 10)
    for (int i = 1; i <= 10; ++i) {
        const std::string key = std::to_string(i % 10);
        c.bind(SUPER, key, workspace(i));
        c.bind(SUPER | SHIFT, key, moveToWorkspace(i));
    }

    // special workspace (scratchpad)
    c.bind(SUPER, "S", toggleSpecial("magic"));
    c.bind(SUPER | SHIFT, "S", moveToSpecial("magic"));

    // scroll through workspaces
    c.bind(SUPER, "mouse_down", workspace("e+1"));
    c.bind(SUPER, "mouse_up", workspace("e-1"));

    // move / resize windows with the mouse
    c.bind(SUPER, "mouse:272", mouseMove(), {.mouse = true});
    c.bind(SUPER, "mouse:273", mouseResize(), {.mouse = true});

    // multimedia keys (work on the lock screen, repeat while held)
    c.bind(NONE, "XF86AudioRaiseVolume", [] { return exec("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86AudioLowerVolume", [] { return exec("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86AudioMute", [] { return exec("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86AudioMicMute", [] { return exec("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86MonBrightnessUp", [] { return exec("brightnessctl -e4 -n2 set 5%+"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86MonBrightnessDown", [] { return exec("brightnessctl -e4 -n2 set 5%-"); }, {.locked = true, .repeat = true});
    c.bind(NONE, "XF86AudioNext", [] { return exec("playerctl next"); }, {.locked = true});
    c.bind(NONE, "XF86AudioPause", [] { return exec("playerctl play-pause"); }, {.locked = true});
    c.bind(NONE, "XF86AudioPlay", [] { return exec("playerctl play-pause"); }, {.locked = true});
    c.bind(NONE, "XF86AudioPrev", [] { return exec("playerctl previous"); }, {.locked = true});

    // a submap: SUPER+X enters "resize", arrow keys resize, escape leaves
    c.bind(SUPER, "X", submap("resize"));
    c.submap("resize", [](Config& c) {
        c.bind(NONE, "right", resizeActive(20, 0), {.repeat = true});
        c.bind(NONE, "left", resizeActive(-20, 0), {.repeat = true});
        c.bind(NONE, "up", resizeActive(0, -20), {.repeat = true});
        c.bind(NONE, "down", resizeActive(0, 20), {.repeat = true});
        c.bind(NONE, "escape", submap("reset"));
    });

    // any C++ works as an action:
    // c.bind(SUPER, "N", [] { notify("hello from C++"); return ok(); });

    // ---- window rules ----
    // ignore maximize requests from apps
    c.windowRule({}, {.suppressEvent = "maximize"}, "suppress-maximize");
    // fix some dragging issues with XWayland
    c.windowRule({.cls = "^$", .title = "^$", .floating = true, .xwayland = true, .fullscreen = false, .pinned = false}, {.noFocus = true}, "xwayland-dragging-fix");
    // example: always float pavucontrol
    c.windowRule({.cls = "^(org.pulseaudio.pavucontrol)$"}, {.floating = true, .center = true, .size = "800 600"}, "pavucontrol-float");
    // A name addresses one rule (a second call with the same name adds to it); the handle switches it on and off. `enabled` (4th argument) = false registers it disabled.
    // layerRule and workspaceRule (WorkspaceRule::enabled) return the same RuleHandle.
    // auto notes = c.windowRule({.cls = "^(obsidian)$"}, {.floating = true}, "float-notes", false);
    // c.bind(SUPER, "F9", [notes] { notes.setEnabled(!notes.isEnabled()); });
    //
    // ---- handles: timers, binds, events ----
    // auto tick = c.timer(1000, [] { /* ... */ }, true, false);        // repeat, created disarmed
    // tick.setEnabled(true); tick.setTimeout(500); bool armed = tick.isEnabled(); tick.cancel();
    // auto b = c.bind(SUPER, "F10", [] { notify("hi"); });  b.setEnabled(false); b.isEnabled(); b.remove();
    // auto h = c.on(Event::WindowDestroy, [](PHLWINDOWREF w) { /* w may be expired */ });  h.isActive(); h.cancel();
    // Event::WindowKill fires before the click-to-kill mode (`abyssctl dispatch kill`) kills a window.
}
