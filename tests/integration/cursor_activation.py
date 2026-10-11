#!/usr/bin/env python3
"""Exercise the cursor exporter on a private bus; never use the desktop bus."""

import os
from pathlib import Path
import select
import shlex
import subprocess
import sys
import tempfile


HARNESS = r'''
#include "CursorActivationEnvironment.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

using namespace Pointer::Cursor;
static std::string output;
static bool rejectUpdate = false;
static void method(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* parameters, GDBusMethodInvocation* invocation, void*) {
    if (rejectUpdate) {
        g_dbus_method_invocation_return_dbus_error(invocation, "org.freedesktop.DBus.Error.AccessDenied", "test rejection");
        return;
    }
    GVariant* values = g_variant_get_child_value(parameters, 0);
    std::ofstream file(output);
    for (gsize i = 0; i < g_variant_n_children(values); ++i) {
        GVariant* value = g_variant_get_child_value(values, i);
        file << g_variant_get_string(value, nullptr) << '\n';
        g_variant_unref(value);
    }
    g_variant_unref(values);
    file.close();
    g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
}
int main(int argc, char** argv) {
    const std::string mode = argv[1];
    if (mode == "capture") {
        std::ofstream file(argv[2]);
        for (const auto* name : {"XCURSOR_THEME", "XCURSOR_SIZE", "ABYSSCURSOR_THEME", "ABYSSCURSOR_SIZE", "ABYSS_TEST_UNRELATED"})
            file << name << '=' << (getenv(name) ? getenv(name) : "") << '\n';
        file.close();
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        assert(connection);
        assert(g_dbus_connection_call_sync(connection, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
            g_variant_new("(su)", "org.abyss.CursorActivationTest", 0), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, nullptr));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    if (mode == "systemd" || mode == "reject") {
        output = argv[2];
        rejectUpdate = mode == "reject";
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        assert(connection);
        const char* xml = "<node><interface name='org.freedesktop.systemd1.Manager'><method name='SetEnvironment'><arg type='as' direction='in'/></method></interface></node>";
        GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(xml, nullptr);
        const GDBusInterfaceVTable table = {method, nullptr, nullptr, {nullptr}};
        assert(g_dbus_connection_register_object(connection, "/org/freedesktop/systemd1", info->interfaces[0], &table, nullptr, nullptr, nullptr));
        g_bus_own_name_on_connection(connection, "org.freedesktop.systemd1", G_BUS_NAME_OWNER_FLAGS_NONE,
            [](GDBusConnection*, const char*, void*) { std::cout << "ready" << std::endl; }, nullptr, nullptr, nullptr);
        g_main_loop_run(g_main_loop_new(nullptr, false));
        return 0;
    }
    const auto start = std::chrono::steady_clock::now();
    {
        CActivationEnvironment exporter(mode == "failure" || mode == "cancel" ? argv[2] : getenv("DBUS_SESSION_BUS_ADDRESS"));
        exporter.update({"old", "old", 16});
        exporter.update({"final native", "final client", 32});
        if (mode == "cancel") {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } else {
            for (int i = 0; exporter.busy() && i < 300; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            assert(!exporter.busy());
            auto errors = exporter.takeErrors();
            assert(errors.size() == (mode == "failure" || mode == "rejected" ? 1 : 0));
        }
    }
    assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(4));
}
'''


def run_inside(binary, work):
    expected = ["XCURSOR_THEME=final client", "XCURSOR_SIZE=32",
                "ABYSSCURSOR_THEME=final native", "ABYSSCURSOR_SIZE=32"]
    subprocess.run([binary, "export"], check=True, timeout=6)
    subprocess.run(["gdbus", "call", "--session", "--dest", "org.freedesktop.DBus",
                    "--object-path", "/org/freedesktop/DBus", "--method",
                    "org.freedesktop.DBus.StartServiceByName", "org.abyss.CursorActivationTest", "0"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=6)
    assert (work / "capture").read_text().splitlines() == expected + ["ABYSS_TEST_UNRELATED=preserved"]
    print("D-Bus activation: current cursor values and unrelated environment preserved")

    for mode in ["systemd", "reject"]:
        process = subprocess.Popen([binary, mode, str(work / "systemd")], stdout=subprocess.PIPE, text=True)
        try:
            assert select.select([process.stdout], [], [], 4)[0], "mock manager did not start"
            assert process.stdout.readline().strip() == "ready"
            subprocess.run([binary, "export" if mode == "systemd" else "rejected"], check=True, timeout=6)
            if mode == "systemd":
                assert (work / "systemd").read_text().splitlines() == expected
            print(f"systemd {mode}: passed")
        finally:
            process.terminate()
            process.wait(timeout=4)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        run_inside(sys.argv[1], Path(sys.argv[2]))
        sys.exit(0)

    root = Path(__file__).resolve().parents[2]
    work = Path(tempfile.mkdtemp(prefix="abyss-cursor-activation-"))
    source = work / "harness.cpp"
    source.write_text(HARNESS)
    binary = str(work / "harness")
    flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "gio-2.0"], text=True))
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++23", "-pthread", "-I" + str(root / "src/pointer/cursor"),
                    str(source), str(root / "src/pointer/cursor/CursorActivationEnvironment.cpp"), "-o", binary, *flags], check=True)
    subprocess.run([binary, "failure", "unix:path=" + str(work / "missing-bus")], check=True, timeout=6)
    print("missing bus: asynchronous failure reported")

    import socket
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
        listener.bind(str(work / "unresponsive-bus"))
        listener.listen(1)
        subprocess.run([binary, "cancel", "unix:path=" + str(work / "unresponsive-bus")], check=True, timeout=6)
    print("unresponsive bus: shutdown cancels connection handshake")

    services = work / "data/dbus-1/services"
    services.mkdir(parents=True)
    (services / "org.abyss.CursorActivationTest.service").write_text(
        f"[D-BUS Service]\nName=org.abyss.CursorActivationTest\nExec={binary} capture {work / 'capture'}\n")
    environment = os.environ.copy()
    environment.update(XDG_DATA_HOME=str(work / "data"), GSETTINGS_BACKEND="memory", ABYSS_TEST_UNRELATED="preserved")
    subprocess.run(["dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()), binary, str(work)],
                   env=environment, check=True, timeout=25)
