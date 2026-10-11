#!/usr/bin/env python3
"""Nested exec/cursor regression, using a private D-Bus and memory GSettings.

Build first with -DABYSS_CONFIG="$PWD/tests/config/native/exec.hpp", then run
python3 tests/integration/native_exec.py inside an existing Wayland session.
Requires xprop and dbus-run-session. Never starts the real user's autostart.
"""

import json
import os
import subprocess
import tempfile
import time
from pathlib import Path

root = Path(__file__).resolve().parents[2]
work = Path(tempfile.mkdtemp(prefix='ae', dir='/tmp'))
env = os.environ.copy()
env.update(XDG_RUNTIME_DIR=str(work), WAYLAND_DISPLAY=str(Path(os.environ['XDG_RUNTIME_DIR']) / os.environ['WAYLAND_DISPLAY']), GSETTINGS_BACKEND='memory', ABYSSLAND_NO_CRASHREPORTER='1', ABYSSLAND_NO_RT='1', ABYSS_EXEC_TEST_DIR=str(work), XCURSOR_THEME='default', XCURSOR_SIZE='18', ABYSSCURSOR_THEME='default', ABYSSCURSOR_SIZE='20')
env['PATH'] = '/opt/abyss/bin:' + env['PATH']
log = (work / 'compositor.log').open('w')
p = subprocess.Popen(['dbus-run-session', '--', str(root / 'build/Abyssland')], cwd=root, env=env, stdout=log, stderr=log)

def await_file(file, expected=None):
    for _ in range(100):
        if file.exists() and file.stat().st_size and (expected is None or file.read_text() == expected):
            return file.read_text()
        time.sleep(.05)
    raise AssertionError(f'File not ready {file}; log {work}')

try:
    signature = None
    for _ in range(150):
        sockets = list((work / 'abyss').glob('*/.socket.sock'))
        if sockets:
            signature = sockets[0].parent.name
            break
        if p.poll() is not None:
            raise RuntimeError(f'Compositor exited: {p.returncode}, log: {work}')
        time.sleep(.1)
    assert signature, f'No IPC socket: {work}'
    env['ABYSSLAND_INSTANCE_SIGNATURE'] = signature
    def ctl(*args):
        return subprocess.check_output([str(root / 'build/abyssctl/abyssctl'), *args], env=env, text=True, timeout=10).strip()
    def exec_cmd(command):
        assert ctl('dispatch', 'exec', command) == 'ok'
    def state(theme, size):
        result = work / 'env'
        # overwrite, do not accept previous state
        exec_cmd("printf '%s\\n' \"$XCURSOR_THEME\" \"$XCURSOR_SIZE\" \"$ABYSSCURSOR_THEME\" \"$ABYSSCURSOR_SIZE\" > '" + str(result) + "'")
        expected = f'{theme}\n{size}\n{theme}\n{size}\n'
        await_file(result, expected)
        for _ in range(100):
            props = subprocess.check_output(['xprop', '-display', display, '-root', 'RESOURCE_MANAGER'], text=True, env=env)
            if f'Xcursor.theme: {theme}' in props and f'Xcursor.size: {size}' in props:
                break
            time.sleep(.05)
        assert f'Xcursor.theme: {theme}' in props and f'Xcursor.size: {size}' in props, props
        return props
    await_file(work / 'start', 'started\n')
    await_file(work / 'compat', 'compat\n')
    assert not (work / 'bind').exists(), 'bind ran while configuring'
    exec_cmd("printf '%s\\n' \"$DISPLAY\" > '" + str(work / 'display') + "'")
    display = await_file(work / 'display').strip()
    assert display.startswith(':')
    print('typed cursor startup:', state('Sweet-cursors', 32))
    subprocess.run(['xprop', '-display', display, '-root', '-f', 'RESOURCE_MANAGER', '8s', '-set', 'RESOURCE_MANAGER', 'Xft.dpi: 144\n! unrelated\nXcursor.theme: old\nXcursor.size: 18\n'], env=env, check=True)
    assert ctl('setcursor', 'default', '24') == 'ok'
    props = state('default', 24)
    assert 'Xft.dpi: 144' in props and '! unrelated' in props
    assert 'old' not in props
    assert ctl('dispatch', 'test_native') == 'ok'
    await_file(work / 'bind', '42\n')
    assert ctl('dispatch', 'test_run') == 'ok'
    await_file(work / 'run', 'run\n')
    print('deferred registered bind callback + run(Action): /bin/sh arithmetic + pipe correct')
    assert ctl('dispatch', 'test_cursor', 'changed') == 'ok'
    state('default', 24)
    assert ctl('dispatch', 'test_cursor', 'restore') == 'ok'
    state('Sweet-cursors', 32)
    # A true option change makes reload restore the compiled value.
    assert ctl('dispatch', 'test_cursor', 'changed') == 'ok'
    state('default', 24)
    for _ in range(2):
        assert ctl('reload', '--no-build') == 'ok'
    state('Sweet-cursors', 32)
    time.sleep(.2)
    assert (work / 'start').read_text() == 'started\n'
    await_file(work / 'compat', 'compat\ncompat\ncompat\n')
    assert (work / 'bind').read_text() == '42\n'
    assert ctl('configerrors') == ''
    print('two no-build reloads: startup callbacks stay once, typed cursor restored, no config errors')
    ctl('dispatch', 'exit')
    p.wait(timeout=10)
    assert p.returncode == 0, p.returncode
    await_file(work / 'shutdown', 'stopped\n')
    print('clean compositor exit; test directory:', work)
finally:
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill(); p.wait()
    log.close()
