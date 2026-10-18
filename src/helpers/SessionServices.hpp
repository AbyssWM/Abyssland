#pragma once

// Session services, so a session works out of the box. Hyprland leaves them to systemd user units
// or the user's config, which on systems without systemd means no sound and no password prompts.
//  - PipeWire, only without systemd: always started fresh, because one left over from an earlier
//    session has lost access to the sound cards and only offers a dummy output. WirePlumber and the
//    PulseAudio bridge are started too if the PipeWire config does not start them itself.
//  - a polkit agent, on every system, when none runs a moment later (systemd may start one).
// Runs on its own thread, so the compositor start never waits for it. Off with
// ecosystem:no_session_services or ABYSSLAND_NO_SESSION_SERVICES=1.
namespace SessionServices {
    void start();
}
