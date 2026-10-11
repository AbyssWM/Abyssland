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

    // ---- look and feel ----
    c.general.gaps_in      = 5;
    c.general.gaps_out     = 20;
    c.general.border_size  = 2;
    c.general.col.active_border   = gradient({rgba(0x33ccff, 0xee), rgba(0x00ff99, 0xee)}, 45);
    c.general.col.inactive_border = rgba(0x595959, 0xaa);
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
    c.decoration.shadow.color        = rgba(0x1a1a1a, 0xee);
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
}
