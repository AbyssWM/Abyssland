#pragma once

// Session services, so a session works out of the box on every distribution, like a full desktop.
// Hyprland leaves these to systemd user units or the user's config, which on systems without
// systemd means no sound, no password prompts and no autostart.
//  - without systemd: PipeWire, always fresh, because one left over from an earlier session has
//    lost access to the sound cards and only offers a dummy output (plus WirePlumber and the
//    PulseAudio bridge if the PipeWire config does not start them), then the XDG autostart entries
//  - on every system: a polkit agent when none runs a moment later
// With systemd, abyssland-session.target starts the rest (see CCompositor::startCompositor).
// Runs on its own thread, so the compositor start never waits for it. Off with
// ecosystem:no_session_services or ABYSSLAND_NO_SESSION_SERVICES=1.
namespace SessionServices {
    void start();
}
