#pragma once

#include <string_view>

const std::string_view USAGE = R"#(usage: abyssctl [flags] <command> [args...|--help]

commands:
    activewindow        → Gets the active window name and its properties
    activeworkspace     → Gets the active workspace and its properties
    animations          → Gets the current config'd info about animations
                          and beziers
    binds               → Lists all registered binds
    clients             → Lists all windows with their properties
    configerrors        → Lists all current config parsing errors
    cursorpos           → Gets the current cursor position in global layout
                          coordinates
    decorations <window_regex> → Lists all decorations and their info
    devices             → Lists all connected keyboards and mice
    dismissnotify [amount] → Dismisses all or up to AMOUNT notifications
    doctor              → Read-only report on deviations from the Abyssland
                          defaults: build, portals, env, helpers, config,
                          disabled protocols (-j for JSON)
    dispatch <dispatcher> [args] → Issue a dispatch to call a keybind
                          dispatcher with arguments
    getoption <option>  → Gets the config option status (values)
    globalshortcuts     → Lists all global shortcuts
    abysspaper ...       → Issue a abysspaper request
    abyssdusk ...       → Issue an AbyssDusk request (blue-light filter)
    instances           → Lists all running instances of Abyssland with
                          their info
    kill                → Issue a kill to get into a kill mode, where you can
                          kill an app by clicking on it. You can exit it
                          with ESCAPE
    layers              → Lists all the surface layers
    layouts             → Lists all layouts available 
    monitors            → Lists active outputs with their properties,
                          'monitors all' lists active and inactive outputs
    notify ...          → Sends a notification using the built-in Abyssland
                          notification system
    output ...          → Allows you to add and remove fake outputs to your
                          preferred backend
    run [name [args]]   → Runs a command registered with c.command in the
                          config (no name: lists them)
    reload [flags]      → Rebuild the compiled config (cmake --build),
                          install it and restart Abyssland. See
                          'abyssctl reload --help'
    restart             → Restart Abyssland (needs start-abyssland)
    rollinglog          → Prints tail of the log. Also supports -f/--follow
                          option
    setcursor <theme> <size> → Sets the cursor theme and reloads the cursor
                          manager
    seterror <color> <message...> → Sets the abyssctl error string. Color has
                          the same format as in colors in config. Will reset
                          when Abyssland's config is reloaded
    setprop ...         → Sets a window property
    getprop ...         → Gets a window property
    splash              → Get the current splash
    status              → Get internal status information
    switchxkblayout ... → Sets the xkb layout index for a keyboard
    systeminfo          → Get system info
    version             → Prints the abyssland version, meaning flags, commit
                          and branch of build.
    workspacerules      → Lists all workspace rules
    workspaces          → Lists all workspaces with their properties

flags:
    -j                  → Output in JSON
    -r                  → Refresh state after issuing command (e.g. for
                          updating variables)
    --batch             → Execute a batch of commands, separated by ';'
    --instance (-i)     → use a specific instance. Can be either signature or
                          index in abyssctl instances (0, 1, etc)
    --quiet (-q)        → Disable the output of abyssctl

--help:
    Can be used to print command's arguments that did not fit into this page
    (three dots))#";

const std::string_view ABYSSPAPER_HELP = R"#(usage: abyssctl [flags] abysspaper <request>

requests:
    wallpaper       → Issue a wallpaper to call a config wallpaper dynamically.
                      Arguments are [mon],[path],[fit_mode]. Fit mode is optional.

flags:
    See 'abyssctl --help')#";

const std::string_view ABYSSDUSK_HELP = R"#(usage: abyssctl [flags] abyssdusk <request>

requests:
    temperature <temp> → Enable blue-light filter
    identity           → Disable blue-light filter
    gamma <gamma>      → Enable gamma filter

flags:
    See 'abyssctl --help')#";

const std::string_view RELOAD_HELP = R"#(usage: abyssctl reload [--no-build | --dry-run]

Abyssland's config is C++ code compiled into the binary. 'reload' does:
    1. cmake --build <build dir> --target Abyssland  (shows progress)
    2. on a compile error: shows the first error as a notification and
       keeps the session running, exit code 1
    3. otherwise saves the installed binary as Abyssland.last-good,
       installs the new one and asks Abyssland to restart via
       start-abyssland. If the new binary dies right after starting,
       start-abyssland falls back to Abyssland.last-good.

The build directory is the hidden ~/.config/abyss/.build. The first reload
on a package installation creates it (sources in .build/src, cloned if
missing). The new compositor replaces <prefix>/bin/Abyssland, /usr/bin for
packages; only that binary, the rest stays with the package. If the prefix
is not writable, root is asked for: sudo in a terminal, pkexec otherwise.

flags:
    --no-build          → Don't rebuild: only re-apply the already compiled
                          config in place
    --dry-run           → Only build, don't install or restart

environment:
    ABYSS_SOURCE_DIR, ABYSS_BUILD_DIR   override the directories
    ABYSS_PREFIX                        install prefix (default: the one
                                        abyssctl was built for); with it set,
                                        the whole build is cmake --install'ed

See also: abyssctl restart)#";

const std::string_view NOTIFY_HELP = R"#(usage: abyssctl [flags] notify <icon> <time_ms> <color> <message...>

icon:
    Integer of value:
        0       → Warning
        1       → Info
        2       → Hint
        3       → Error
        4       → Confused
        5       → Ok
        6 or -1 → No icon

time_ms:
    Time to display notification in milliseconds

color:
    Notification color.  Use
    0 for default color for icon

message:
    Notification message

flags:
    See 'abyssctl --help')#";

const std::string_view OUTPUT_HELP = R"#(usage: abyssctl [flags] output <create <backend> | remove <name>>

create <backend>:
    Creates new virtual output. Possible values for backend: wayland, x11,
    headless or auto.

remove <name>:
    Removes virtual output. Pass the output's name, as found in
    'abyssctl monitors'

flags:
    See 'abyssctl --help')#";

const std::string_view SETPROP_HELP = R"#(usage: abyssctl [flags] setprop <regex> <property> <value> [lock]

regex:
    Regular expression by which a window will be searched

property:
    See the wiki for a list of properties

value:
    Property value

lock:
    Optional argument. If lock is not added, will be unlocked. Locking means a
    dynamic windowrule cannot override this setting.

flags:
    See 'abyssctl --help')#";

const std::string_view GETPROP_HELP = R"#(usage: abyssctl [flags] getprop <regex> <property>

regex:
    Regular expression by which a window will be searched

property:
    See the wiki for a list of properties

flags:
    See 'abyssctl --help')#";

const std::string_view SWITCHXKBLAYOUT_HELP = R"#(usage: [flags] switchxkblayout <device> <cmd>

device:
    You can find the device using 'abyssctl devices' command

cmd:
    'next' for next, 'prev' for previous, or ID for a specific one. IDs are
    assigned based on their order in config file (keyboard_layout),
    starting from 0

flags:
    See 'abyssctl --help')#";
