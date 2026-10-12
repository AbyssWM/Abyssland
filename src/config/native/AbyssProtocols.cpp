// Abysslang: choosing the offered Wayland protocols (include/abyss/protocols.hpp).
// Not `using namespace abyss`: see Abysslang.cpp.

#include <abyss/protocols.hpp>

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <string>
#include <wayland-server-core.h>

#include "AbyssProtocols.hpp"
#include "ConfigManager.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../notification/NotificationOverlay.hpp"

namespace {
    struct SKnown {
        const char* name;
        bool        core;
        const char* note;
    };

    // clang-format off
    const SKnown KNOWN[] = {
        // ---- core: cannot be disabled ----
        {"wl_seat", true, "input"},
        {"wl_data_device_manager", true, "clipboard / drag and drop"},
        {"wl_compositor", true, "surfaces"},
        {"wl_subcompositor", true, "subsurfaces"},
        {"wl_shm", true, "shared memory buffers"},
        {"wl_output", true, "monitors"},
        {"xdg_wm_base", true, "application windows"},
        {"zwp_linux_dmabuf_v1", true, "GPU buffers (Vulkan / GL clients); only offered when the renderer supports it"},
        // ---- Abyssland's own ----
        {"abyssland_lock_notifier_v1", false, "lock screen notification for helpers"},
        {"abyssland_focus_grab_manager_v1", false, "popup focus grabs (launchers, menus)"},
        {"abyssland_toplevel_mapping_manager_v1", false, "maps toplevel handles to windows (portal)"},
        {"abyssland_global_shortcuts_manager_v1", false, "global shortcuts (xdg-desktop-portal-abyss)"},
        {"abyssland_ctm_control_manager_v1", false, "colour transform matrix (night light helpers)"},
        {"abyssland_input_capture_manager_v1", false, "input capture (portal, remote desktop)"},
        {"abyssland_surface_manager_v1", false, "Abyssland surface extensions (client side effects / rules)"},
        {"abyssland_toplevel_export_manager_v1", false, "own screencopy for windows (xdg-desktop-portal-abyss needs it)"},
        // ---- wlr ----
        {"zwlr_gamma_control_manager_v1", false, "night light (gammastep, wlsunset)"},
        {"zwlr_foreign_toplevel_manager_v1", false, "taskbars (waybar wlr/taskbar, ...)"},
        {"zwlr_output_power_manager_v1", false, "DPMS from clients (wlopm, idle helpers)"},
        {"zwlr_virtual_pointer_manager_v1", false, "virtual pointer (wlrctl, remote desktop)"},
        {"zwlr_output_manager_v1", false, "monitor configuration (kanshi, wlr-randr, nwg-displays)"},
        {"zwlr_layer_shell_v1", false, "bars, launchers, wallpapers, notification daemons: most shells stop working without it"},
        {"zwlr_data_control_manager_v1", false, "clipboard managers (wl-clipboard --watch, cliphist)"},
        {"zwlr_screencopy_manager_v1", false, "grim, wf-recorder, xdg-desktop-portal-wlr"},
        // ---- ext ----
        {"ext_foreign_toplevel_list_v1", false, "toplevel list (needed by ext-image-copy-capture window capture)"},
        {"ext_idle_notifier_v1", false, "idle detection of clients (hypridle, swayidle)"},
        {"ext_session_lock_manager_v1", false, "lock screens (hyprlock, swaylock, abysslock): without it nothing can lock the session"},
        {"ext_workspace_manager_v1", false, "workspace info for bars"},
        {"ext_data_control_manager_v1", false, "clipboard managers (ext variant)"},
        {"ext_background_effect_manager_v1", false, "blur for client surfaces"},
        {"ext_output_image_capture_source_manager_v1", false, "monitor capture sources (ext-image-copy-capture)"},
        {"ext_foreign_toplevel_image_capture_source_manager_v1", false, "window capture sources (ext-image-copy-capture)"},
        {"ext_image_copy_capture_manager_v1", false, "ext screencopy (portals, OBS, recent screenshot tools)"},
        // ---- wp ----
        {"wp_viewporter", false, "surface scaling / cropping (video players, browsers)"},
        {"wp_tearing_control_manager_v1", false, "tearing for games (allow_tearing)"},
        {"wp_fractional_scale_manager_v1", false, "fractional scaling: clients render blurry without it"},
        {"wp_cursor_shape_manager_v1", false, "cursor shapes"},
        {"wp_alpha_modifier_v1", false, "surface opacity"},
        {"wp_presentation", false, "presentation timing (video players, games)"},
        {"wp_single_pixel_buffer_manager_v1", false, "solid colour buffers"},
        {"wp_security_context_manager_v1", false, "sandboxed clients (Flatpak)"},
        {"wp_content_type_manager_v1", false, "content type hints (game / video)"},
        {"wp_pointer_warp_v1", false, "clients warping the pointer"},
        {"wp_fifo_manager_v1", false, "frame pacing"},
        {"wp_commit_timing_manager_v1", false, "commit timing (render:commit_timing_enabled)"},
        {"wp_color_manager_v1", false, "colour management / HDR (render:cm_enabled)"},
        {"wp_drm_lease_device_v1", false, "VR headset leases; only offered with a DRM backend"},
        {"wp_linux_drm_syncobj_manager_v1", false, "explicit sync. NVIDIA: keep it ON, turning it off causes flickering"},
        // ---- xdg ----
        {"xdg_activation_v1", false, "focus requests / startup notification"},
        {"xdg_wm_dialog_v1", false, "dialog hints"},
        {"xdg_toplevel_tag_manager_v1", false, "window tags"},
        {"xdg_system_bell_v1", false, "system bell"},
        {"zxdg_output_manager_v1", false, "output geometry for clients: many toolkits and bars need it"},
        {"zxdg_decoration_manager_v1", false, "server side decorations"},
        {"zxdg_exporter_v2", false, "window parenting across clients"},
        {"zxdg_importer_v2", false, "window parenting across clients"},
        // ---- zwp / misc ----
        {"zwp_idle_inhibit_manager_v1", false, "video players keeping the screen on"},
        {"zwp_relative_pointer_manager_v1", false, "games, 3D applications"},
        {"zwp_pointer_gestures_v1", false, "touchpad gestures in clients"},
        {"zwp_keyboard_shortcuts_inhibit_manager_v1", false, "VMs / remote desktops grabbing shortcuts"},
        {"zwp_text_input_manager_v1", false, "input methods (legacy)"},
        {"zwp_text_input_manager_v3", false, "input methods"},
        {"zwp_pointer_constraints_v1", false, "games (pointer lock / confine)"},
        {"zwp_input_method_manager_v2", false, "input methods (fcitx5, ibus)"},
        {"zwp_virtual_keyboard_manager_v1", false, "virtual keyboards (wtype, on-screen keyboards)"},
        {"zwp_tablet_manager_v2", false, "drawing tablets"},
        {"zwp_primary_selection_device_manager_v1", false, "middle-click paste"},
        {"org_kde_kwin_server_decoration_manager", false, "KDE style server side decorations"},
        {"xwayland_shell_v1", false, "Xwayland: X11 windows cannot be mapped without it"},
        {"vicinae_hotkey_manager_v1", false, "vicinae launcher hotkeys"},
        {"wl_drm", false, "legacy Mesa buffer sharing; only offered together with dmabuf"},
    };
    // clang-format on

    const SKnown* findKnown(std::string_view n) {
        for (const auto& k : KNOWN)
            if (n == k.name)
                return &k;
        return nullptr;
    }

    struct SChoice {
        bool enabled    = true;
        int  maxVersion = 0;
    };

    std::map<std::string, SChoice> g_desired;                    // from the last config load
    std::map<std::string, SChoice> g_running;                    // what the globals were created with (this process)
    std::map<std::string, int>     g_builtinVer;                 // for the report

    ::Config::Native::CConfigManager* M() {
        return ::Config::Native::self();
    }

    void err(std::string s) {
        if (auto* m = M())
            m->addError(std::move(s));
        else
            Log::logger->log(Log::ERR, "[abyss] {}", s);
    }

    SChoice desiredOf(const std::string& n) {
        auto it = g_desired.find(n);
        return it == g_desired.end() ? SChoice{} : it->second;
    }
}

void Config::Native::applyProtocols(const abyss::Protocols& p) {
    std::map<std::string, SChoice> want;

    if (p.hasOnly) {
        std::set<std::string> keep;
        for (const auto& n : p.onlyList) {
            if (!findKnown(n)) {
                err(std::format("protocols.enableOnly: unknown protocol \"{}\" (see `abyssctl protocols`)", n));
                continue;
            }
            keep.insert(n);
        }
        for (const auto& k : KNOWN)
            if (!k.core && !keep.contains(k.name))
                want[k.name].enabled = false;
    }

    for (const auto& e : p.entries) {
        const auto* k = findKnown(e.name);
        if (!k) {
            err(std::format("protocol: unknown protocol \"{}\" (see `abyssctl protocols` for the list)", e.name));
            continue;
        }
        if (e.opts.maxVersion < 0) {
            err(std::format("protocol \"{}\": maxVersion must be >= 0 (0 = no limit)", e.name));
            continue;
        }
        if (k->core && !e.opts.enabled) {
            err(std::format("protocol \"{}\" is a core protocol and cannot be disabled (nothing could start without it)", e.name));
            continue;
        }
        want[e.name] = SChoice{e.opts.enabled, e.opts.maxVersion};
    }

    g_desired = std::move(want);

    // reload while running: globals cannot be changed safely, tell the user
    std::string diff;
    for (const auto& [name, run] : g_running) {
        const auto d = desiredOf(name);
        if (d.enabled != run.enabled || (run.enabled && d.maxVersion != run.maxVersion))
            diff += std::format("\n  {}", name);
    }
    if (!diff.empty()) {
        Log::logger->log(Log::WARN, "[abyss] protocol selection changed, takes effect after a restart:{}", diff);
        if (Notification::overlay())
            Notification::overlay()->addNotification(std::format("Protocol selection changed: takes effect after a restart:{}", diff), 0, 7000, ICON_WARNING);
    }
}

int Config::Native::protocolGlobalVersion(const wl_interface* iface, int builtinVersion) {
    const std::string name = iface && iface->name ? iface->name : "";
    g_builtinVer[name]     = builtinVersion;

    if (!findKnown(name)) {
        Log::logger->log(Log::WARN, "[abyss] protocol \"{}\" is not in the Abysslang protocol list (always offered)", name);
        return builtinVersion;
    }

    const auto d = desiredOf(name);
    g_running.emplace(name, d); // first creation of this session wins (monitors re-create wl_output etc.)
    const auto& run = g_running.at(name);

    if (!run.enabled) {
        Log::logger->log(Log::INFO, "[abyss] protocol {} disabled by the config, global not created", name);
        return 0;
    }
    if (run.maxVersion > 0 && run.maxVersion < builtinVersion) {
        Log::logger->log(Log::INFO, "[abyss] protocol {} limited to version {} (built-in {})", name, run.maxVersion, builtinVersion);
        return run.maxVersion;
    }
    return builtinVersion;
}

std::vector<abyss::ProtocolInfo> abyss::knownProtocols() {
    std::vector<ProtocolInfo> out;
    for (const auto& k : KNOWN) {
        const auto d = desiredOf(k.name);
        const auto r = g_running.find(k.name);
        out.push_back({k.name, k.core, r != g_running.end() ? r->second.enabled : d.enabled, r != g_running.end() ? r->second.maxVersion : d.maxVersion, k.note});
    }
    return out;
}

bool abyss::protocolEnabled(std::string_view name) {
    if (!findKnown(name))
        return false;
    const std::string n{name};
    const auto        r = g_running.find(n);
    return r != g_running.end() ? r->second.enabled : desiredOf(n).enabled;
}

std::string Config::Native::protocolsReport(bool json) {
    std::string out;
    if (json)
        out = "[";
    bool first = true;
    for (const auto& k : abyss::knownProtocols()) {
        const bool        running = g_running.contains(k.name);
        const std::string state   = !k.enabled ? "off" : (running ? "on" : "not-created");
        // note: "not-created" = enabled but this session has no such global (hardware / config dependent protocol)
        if (json) {
            std::string note;
            for (char c : k.note)
                note += (c == '"' || c == '\\') ? std::string{'\\', c} : std::string{c};
            out += std::format("{}\n{{\"name\":\"{}\",\"core\":{},\"state\":\"{}\",\"maxVersion\":{},\"version\":{},\"note\":\"{}\"}}", first ? "" : ",", k.name, k.core, state,
                               k.maxVersion, g_builtinVer.contains(k.name) ? g_builtinVer.at(k.name) : 0, note);
            first = false;
        } else {
            out += std::format("{:<56} {:<12}{}{}{}{}\n", k.name, state, k.core ? " core" : "", k.maxVersion ? std::format(" maxVersion={}", k.maxVersion) : "",
                               k.note.empty() ? "" : "  # ", k.note);
        }
    }
    if (json)
        out += "\n]";
    return out;
}
