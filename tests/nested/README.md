# Nested protocol contract test

Checks that the Wayland globals a running Abyssland offers match what
`abyssctl -j protocols` (and therefore the config) promises:

- (a) every protocol with state `on` is a visible global; if `maxVersion` is set the version is `min(maxVersion, built-in)`
- (b) every protocol with state `off` is absent
- (c) every global a client sees is listed in `KNOWN` (`src/config/native/AbyssProtocols.cpp`), i.e. can be switched off
- (d) with `--default-config`: the xdg-desktop-portal-wlr protocols are on (and the config disables nothing)

## Usage

    tests/nested/protocol-contract.sh [--default-config] <path-to-Abyssland-binary>

Needs gcc, pkg-config, wayland-client, dbus-run-session, python3 and a running
parent Wayland session (`WAYLAND_DISPLAY`). The binary is started nested with a
private `XDG_RUNTIME_DIR` under `/tmp/ac.*`, queried, then stopped with
`abyssctl dispatch exit` (killed by PID if it does not exit). `abyssctl` is taken
from the binary's directory, else `/opt/abyss/bin`.

Environment:

- `OUT` build/output directory (default `./_contract`); holds `wlglobals`, `globals.txt`, `protocols.json`, the last runtime dir in `run-last`
- `CONTRACT_FAKE_EXTRA=foo_v1` testing only: adds a fake global to the client output, check (c) must then FAIL

Exit code 0 = all checks passed, 1 = a check failed, 2 = usage/build error.
