// Abysslang: choose which Wayland protocols (globals) Abyssland offers to clients.
//
// By default every protocol is offered, exactly as without this file. Everything below is opt-out; whoever turns something off owns the
// consequences (a client that needs a protocol which is not offered either falls back or refuses to start).
// Names are the Wayland interface names, e.g. "zwlr_screencopy_manager_v1" (`abyssctl protocols` lists them all with their state).
//
//   c.protocol("zwlr_screencopy_manager_v1", false);                       // no wlr screencopy
//   c.protocols.disable({"abyssland_toplevel_export_manager_v1", "ext_image_copy_capture_manager_v1"});
//   c.protocols.enableOnly({"zwlr_layer_shell_v1", "zwlr_screencopy_manager_v1"}); // everything else (non-core) off; later calls still apply
//   c.protocol("zwlr_screencopy_manager_v1", {.maxVersion = 2});           // offer at most version 2
//
// Typical set-ups
//   * only wlr screencopy (grim, wf-recorder, xdg-desktop-portal-wlr):
//       c.protocols.disable({"abyssland_toplevel_export_manager_v1", "ext_image_copy_capture_manager_v1",
//                            "ext_output_image_capture_source_manager_v1", "ext_foreign_toplevel_image_capture_source_manager_v1"});
//   * only our own (Abyssland portal):
//       c.protocols.disable({"zwlr_screencopy_manager_v1"});
//   * ext-image-copy-capture only:  disable zwlr_screencopy_manager_v1 and abyssland_toplevel_export_manager_v1.
//
// Core protocols (wl_seat, wl_compositor, wl_subcompositor, wl_shm, wl_data_device_manager, wl_output, xdg_wm_base, zwp_linux_dmabuf_v1)
// cannot be switched off (err(): nothing could start without them). Unknown names are an error too, so a typo never goes unnoticed.
//
// Timing: protocols are created when the compositor starts (before the first client can connect). A reload without restart
// (`abyssctl reload --no-build`) cannot add or remove globals safely; if the config asks for something different than what is running,
// a notification says "takes effect after a restart" and nothing changes until the next start.
//
// NVIDIA: keep wp_linux_drm_syncobj_manager_v1 (explicit sync) switched ON. Turning it off on NVIDIA causes flickering / tearing artefacts.
// wp_drm_lease_device_v1, wp_tearing_control_manager_v1 and wp_color_manager_v1 may be turned off but are on by default.
//
// Protocols that are only created conditionally (drm lease, syncobj, commit timing, color management, wl_drm, ...) simply stay absent when
// the hardware / config does not support them, regardless of this setting.
#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace abyss {

    struct ProtocolOpts {
        bool enabled    = true;
        int  maxVersion = 0; // 0 = no limit; otherwise the global is offered with min(maxVersion, built-in version)
    };

    // pending choices of one config load (c.protocols). Validation happens when the config is applied.
    struct Protocols {
        struct Entry {
            std::string  name;
            ProtocolOpts opts;
        };
        std::vector<Entry>       entries;
        std::vector<std::string> onlyList; // set by enableOnly: everything non-core not listed here is off
        bool                     hasOnly = false;

        void set(std::string name, ProtocolOpts o) { entries.push_back({std::move(name), o}); }
        void set(std::string name, bool enabled) { entries.push_back({std::move(name), ProtocolOpts{.enabled = enabled}}); }
        void disable(std::initializer_list<std::string_view> names) {
            for (auto n : names)
                set(std::string{n}, false);
        }
        void enable(std::initializer_list<std::string_view> names) {
            for (auto n : names)
                set(std::string{n}, true);
        }
        // everything that is not core and not listed gets switched off (explicit disable()/set() calls afterwards still apply)
        void enableOnly(std::initializer_list<std::string_view> names) {
            hasOnly = true;
            onlyList.clear();
            for (auto n : names)
                onlyList.emplace_back(n);
        }
    };

    struct ProtocolInfo {
        std::string name;       // wayland interface name
        bool        core;       // cannot be disabled
        bool        enabled;    // currently offered (running compositor) or configured, see protocolEnabled
        int         maxVersion; // configured limit, 0 = none
        std::string note;       // what depends on it / what breaks without it (may be empty)
    };

    // all protocol names Abyssland knows (also the ones that depend on the hardware and may not exist at runtime)
    std::vector<ProtocolInfo> knownProtocols();
    // true when the global is offered (or, if it was not created in this session, would be offered). false for unknown names.
    bool protocolEnabled(std::string_view name);

} // namespace abyss
