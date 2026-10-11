#pragma once

// Exit codes and environment shared between the compositor and start-abyssland.
// start-abyssland includes this header directly (no other dependencies here).

// Abyssland exits with this code after `abyssctl restart`; the watchdog relaunches it immediately.
#define ABYSSLAND_EXIT_RESTART 64

// Set to "1" by start-abyssland in the compositor's environment.
#define ABYSSLAND_WATCHDOG_ENV "ABYSSLAND_WATCHDOG"
