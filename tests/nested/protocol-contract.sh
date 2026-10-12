#!/usr/bin/env bash
# Protocol contract test for Abyssland. See README.md.
# usage: protocol-contract.sh [--default-config] <path-to-Abyssland-binary>
set -u
ulimit -v 4194304

DEFAULTCFG=0
BIN=
for a in "$@"; do
    case "$a" in
        --default-config) DEFAULTCFG=1 ;;
        *) BIN=$a ;;
    esac
done
[ -n "$BIN" ] && [ -x "$BIN" ] || { echo "usage: $0 [--default-config] <path-to-Abyssland-binary>" >&2; exit 2; }
BIN=$(readlink -f "$BIN")

OUT=${OUT:-./_contract}
mkdir -p "$OUT" || exit 2
OUT=$(cd "$OUT" && pwd)
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# abyssctl: next to the binary if present, else /opt/abyss/bin
CTLDIR=$(dirname "$BIN")
[ -x "$CTLDIR/abyssctl" ] || CTLDIR=/opt/abyss/bin
export PATH="$CTLDIR:/opt/abyss/bin:$PATH"

gcc -O1 -o "$OUT/wlglobals" "$HERE/wlglobals.c" $(pkg-config --cflags --libs wayland-client) || { echo "FAIL build wlglobals"; exit 2; }

PARENT_RT=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
PARENT_WL=${WAYLAND_DISPLAY:-wayland-0}
case "$PARENT_WL" in /*) ;; *) PARENT_WL=$PARENT_RT/$PARENT_WL ;; esac

RT=$(mktemp -d /tmp/ac.XXXX)
mkdir -p "$RT/rt" && chmod 700 "$RT/rt"
export XDG_RUNTIME_DIR="$RT/rt"
PID=

cleanup() {
    if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
        [ -n "${ABYSSLAND_INSTANCE_SIGNATURE:-}" ] && abyssctl dispatch exit >/dev/null 2>&1
        for _ in $(seq 1 50); do kill -0 "$PID" 2>/dev/null || break; sleep 0.1; done
        if kill -0 "$PID" 2>/dev/null; then kill "$PID" 2>/dev/null; sleep 1; fi
        kill -0 "$PID" 2>/dev/null && kill -9 "$PID" 2>/dev/null
    fi
    # temp dir is left for the caller to move away; it only holds sockets/logs
    [ -d "$RT" ] && mv "$RT" "$OUT/run-last" 2>/dev/null
}
trap cleanup EXIT

WAYLAND_DISPLAY=$PARENT_WL GSETTINGS_BACKEND=memory ABYSSLAND_NO_CRASHREPORTER=1 ABYSSLAND_NO_RT=1 \
    dbus-run-session -- "$BIN" >"$RT/log" 2>&1 &
PID=$!

SIG=
for _ in $(seq 1 150); do
    s=$(ls "$RT/rt/abyss" 2>/dev/null | head -n1)
    if [ -n "$s" ] && [ -S "$RT/rt/abyss/$s/.socket.sock" ]; then SIG=$s; break; fi
    kill -0 "$PID" 2>/dev/null || break
    sleep 0.2
done
[ -n "$SIG" ] || { echo "FAIL start: no IPC socket"; tail -n 20 "$RT/log"; exit 1; }
export ABYSSLAND_INSTANCE_SIGNATURE=$SIG

WLSOCK=
for _ in $(seq 1 50); do
    WLSOCK=$(cd "$RT/rt" && ls -d wayland-[0-9]* 2>/dev/null | grep -v '\.lock$' | head -n1)
    [ -n "$WLSOCK" ] && break
    sleep 0.2
done
[ -n "$WLSOCK" ] || { echo "FAIL start: no nested wayland socket"; exit 1; }

# the client talks to the nested compositor
GLOBALS=$(WAYLAND_DISPLAY=$WLSOCK "$OUT/wlglobals") || { echo "FAIL wlglobals"; exit 1; }
# test hook, documented in README: pretend the compositor offers an extra global
[ -n "${CONTRACT_FAKE_EXTRA:-}" ] && GLOBALS=$(printf '%s\n%s 1\n' "$GLOBALS" "$CONTRACT_FAKE_EXTRA")
JSON=$(abyssctl -j protocols) || { echo "FAIL abyssctl -j protocols"; exit 1; }

printf '%s\n' "$GLOBALS" >"$OUT/globals.txt"
printf '%s\n' "$JSON" >"$OUT/protocols.json"

python3 - "$OUT/globals.txt" "$OUT/protocols.json" "$DEFAULTCFG" <<'PY'
import json, sys
globs = {}
for line in open(sys.argv[1]):
    p = line.split()
    if len(p) == 2:
        globs[p[0]] = max(globs.get(p[0], 0), int(p[1]))
protos = json.load(open(sys.argv[2]))
by = {p["name"]: p for p in protos}
fails = {"a": [], "b": [], "c": [], "d": []}

for p in protos:
    n, st = p["name"], p["state"]
    if st == "on":
        if n not in globs:
            fails["a"].append(f"{n}: state on but no global")
            continue
        want = p["version"]
        if p["maxVersion"] > 0:
            want = min(p["maxVersion"], want)
        if want and globs[n] != want:
            fails["a"].append(f"{n}: version {globs[n]}, expected {want}")
    elif st == "off":
        if n in globs:
            fails["b"].append(f"{n}: state off but global present")
for g in sorted(globs):
    if g not in by:
        fails["c"].append(f"{g}: global not in KNOWN (cannot be disabled)")

mode_default = sys.argv[3] == "1"
portal = ["zwlr_screencopy_manager_v1", "zxdg_output_manager_v1", "zwlr_layer_shell_v1",
          "ext_image_copy_capture_manager_v1", "ext_output_image_capture_source_manager_v1",
          "ext_foreign_toplevel_list_v1"]
anyoff = any(p["state"] == "off" or p["maxVersion"] > 0 for p in protos)
if mode_default:
    if anyoff:
        fails["d"].append("--default-config given but config disables/limits protocols")
    for n in portal:
        if n not in globs:
            fails["d"].append(f"{n}: missing")

names = {"a": "(a) on => global present, version = min(maxVersion, built-in)",
         "b": "(b) off => global absent",
         "c": "(c) every global is in KNOWN",
         "d": "(d) portal protocols on by default"}
bad = 0
for k in "abc":
    if fails[k]:
        bad = 1
        print("FAIL", names[k])
        for f in fails[k]: print("   ", f)
    else:
        print("PASS", names[k])
if mode_default:
    if fails["d"]:
        bad = 1
        print("FAIL", names["d"])
        for f in fails["d"]: print("   ", f)
    else:
        print("PASS", names["d"])
else:
    print("SKIP", names["d"], "(use --default-config)")
sys.exit(bad)
PY
RC=$?
exit $RC
