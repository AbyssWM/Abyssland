#!/usr/bin/env python3
"""abysslang-coverage: prueft, welche Lua-Einstellungen/-Funktionen aus dem Hyprland-Wiki in Abysslang (C++) vorhanden sind.

Quellen (nur lesen, nichts wird gebaut):
  Wiki      <root>/_upstream/hyprland-wiki/content/configuring/**
  Abyssland <root>/Abyssland/include/abyss/*.hpp, src/config/values/ConfigValues.cpp,
            src/config/shared/animation/AnimationTree.cpp, src/config/native/Abysslang.cpp, src/**

Kategorien:
  options        Config-Optionen (config-options.md + Layout-Seiten) gegen die Optionstabelle (ConfigValues.cpp)
  toplevel       hl.* Funktionen (ohne hl.dsp.*, hl.get_*)
  queries        hl.get_* / hl.is_key_down
  dispatchers    hl.dsp.* gegen actions.hpp
  events         hl.on-Events gegen events.hpp
  window_match   Fensterregel-Match-Props gegen WindowMatch
  window_effects Fensterregel-Effekte gegen ABYSS_WINDOW_EFFECTS
  layer_rules    Layerregel-Props/-Effekte
  workspace_rules Workspace-Regel-Felder gegen WorkspaceRule
  monitor        Monitor-Felder
  device         Per-Device-Felder gegen ABYSS_DEVICE_FIELDS
  binds          Bind-Flags gegen BindOpts
  gestures       Gesten-Felder
  notification   Notification-Parameter
  animations     Animationsbaum-Knoten
  permissions    Permission-Typen

Mapping: die Tabellen DISPATCHERS, TOPLEVEL, QUERIES, EVENT_OVERRIDES usw. unten ordnen Wiki-Namen den Abysslang-Symbolen zu.
Ein Eintrag (Symbole, Notiz): Symbole = Liste von Regexen, die alle im (kommentarbereinigten) Header vorkommen muessen.
None als Symbole = in Abysslang nicht vorhanden (Notiz erklaert warum / was fehlt).

Aufruf:
  scripts/abysslang-coverage.py [--root DIR] [--json] [--allow FILE] [--md FILE] [--quiet]
Exit-Code 1, wenn etwas fehlt, das nicht in der Allow-Datei steht.
Allow-Datei: eine Zeile pro Punkt `kategorie:id  # Begruendung` (Leerzeilen und #-Zeilen ignoriert).
"""
import argparse
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT = os.path.dirname(HERE)

WIKI_REL = "_upstream/hyprland-wiki/content/configuring"
# im Abyssland-Repo selbst: Wurzel = Repo (include/abyss vorhanden); im Workspace-Layout: Unterordner Abyssland
ABY_REL = "." if os.path.isdir(os.path.join(DEFAULT_ROOT, "include", "abyss")) else "Abyssland"

CATEGORY_TITLES = [
    ("options", "Config-Optionen"),
    ("toplevel", "hl.* Toplevel-Funktionen"),
    ("queries", "Abfragen (hl.get_*)"),
    ("dispatchers", "Dispatcher (hl.dsp.*)"),
    ("events", "Events (hl.on)"),
    ("window_match", "Fensterregeln: Match-Props"),
    ("window_effects", "Fensterregeln: Effekte"),
    ("layer_rules", "Layerregeln"),
    ("workspace_rules", "Workspace-Regeln"),
    ("monitor", "Monitor-Felder"),
    ("device", "Device-Felder"),
    ("binds", "Bind-Flags"),
    ("gestures", "Gesten-Felder"),
    ("notification", "Notification-Parameter"),
    ("animations", "Animationsbaum"),
    ("permissions", "Permission-Typen"),
]


# ----------------------------------------------------------------------------------------------------------------------
# Mapping-Tabellen (Wiki-Name -> Abysslang). Aus include/abyss/*.hpp gefuellt.
# ----------------------------------------------------------------------------------------------------------------------

def fn(name):
    """Regex fuer eine deklarierte/benutzte Funktion `name(`."""
    return r"\b" + re.escape(name) + r"\s*\("


def ty(name):
    return r"\b" + re.escape(name) + r"\b"


# hl.dsp.*: Schluessel = Wiki-Id (Abschnitt.Methode, bei Ueberladungen mit [Variante])
DISPATCHERS = {
    "dpms": ([fn("dpms")], ""),
    "event": ([fn("event")], ""),
    "exec_cmd": ([fn("exec")], "exec() / Config::exec"),
    "exec_raw": (None, "bewusst: kein Shell-loses spawn in der Config-API (vault/abyss/README.md)"),
    "exit": ([fn("exit")], ""),
    "focus[direction]": ([fn("moveFocus")], ""),
    "focus[monitor]": ([fn("focusMonitor")], ""),
    "focus[workspace]": ([fn("workspace"), fn("focusWorkspaceOnCurrentMonitor")], "workspace() + focusWorkspaceOnCurrentMonitor()"),
    "focus[window]": ([fn("focusWindow")], ""),
    "focus[urgent_or_last]": ([fn("focusUrgentOrLast")], ""),
    "focus[last]": ([fn("focusCurrentOrLast")], ""),
    "force_idle": ([fn("forceIdle")], ""),
    "force_renderer_reload": ([fn("forceRendererReload")], ""),
    "global": ([fn("global")], ""),
    "layout": ([fn("layoutMsg")], ""),
    "no_op": ([fn("noop")], "Config-Aktion noop()"),
    "pass": ([fn("pass")], "Fenster-Selektor ist Pflichtargument"),
    "release_input_capture": ([fn("releaseInputCapture")], ""),
    "reload_config": (None, "kein typisierter Reload-Dispatcher (nur dispatch(\"reload\") / abyssctl reload)"),
    "send_key_state": ([fn("sendKeyState")], ""),
    "send_shortcut": ([fn("sendShortcut")], ""),
    "submap": ([fn("submap")], ""),
    "window.alter_zorder": ([fn("alterZOrder")], ""),
    "window.center": ([fn("centerWindow")], "ohne Fenster-Selektor (nur aktives Fenster)"),
    "window.clear_tags": ([fn("clearTags")], "clearTags(selector)"),
    "window.close": ([fn("closeWindow")], ""),
    "window.cycle_next": ([fn("cycleNext")], ""),
    "window.deny_from_group": ([fn("denyWindowFromGroup")], ""),
    "window.drag": ([fn("mouseMove")], ""),
    "window.float": ([fn("toggleFloating"), fn("setFloating"), fn("setTiled")], ""),
    "window.fullscreen_state": ([fn("fullscreenState")], ""),
    "window.fullscreen": ([fn("fullscreen")], ""),
    "window.kill": ([fn("killWindow")], ""),
    "window.move[direction]": ([fn("moveWindow")], ""),
    "window.move[workspace]": ([fn("moveToWorkspace")], ""),
    "window.move[monitor]": ([fn("moveWindowToMonitor")], ""),
    "window.move[x]": ([fn("moveWindowPixel")], ""),
    "window.move[into_group]": ([fn("moveIntoGroup")], ""),
    "window.move[into_or_create_group]": ([fn("moveIntoOrCreateGroup")], ""),
    "window.move[out_of_group]": ([fn("moveOutOfGroup")], ""),
    "window.pin": ([fn("pin")], ""),
    "window.pseudo": ([fn("pseudo")], ""),
    "window.resize[keep_aspect_ratio]": ([fn("mouseResize")], ""),
    "window.resize[x]": ([fn("resizeWindowPixel")], ""),
    "window.set_prop": ([fn("setProp")], ""),
    "window.signal": ([fn("signal"), fn("signalWindow")], "signalWindow(sig, selector)"),
    "window.swap[direction]": ([fn("swapWindow")], ""),
    "window.swap[target]": ([fn("swapWith")], ""),
    "window.swap[next]": ([fn("swapNext")], ""),
    "window.swap[prev]": ([fn("swapNext")], "swapNext(false)"),
    "window.tag": ([fn("tag")], ""),
    "window.toggle_swallow": ([fn("toggleSwallow")], ""),
    "workspace.move": ([fn("moveWorkspaceToMonitor"), fn("moveCurrentWorkspaceToMonitor")], ""),
    "workspace.rename": ([fn("renameWorkspace")], ""),
    "workspace.swap_monitors": ([fn("swapActiveWorkspaces")], ""),
    "workspace.toggle_special": ([fn("toggleSpecial")], ""),
    "group.active": ([fn("setGroupActive")], "setGroupActive(index, selector)"),
    "group.lock": ([fn("lockGroups")], ""),
    "group.lock_active": ([fn("lockActiveGroup")], ""),
    "group.move_window": ([fn("moveGroupWindow")], ""),
    "group.next": ([fn("changeGroupActive")], "changeGroupActive(true)"),
    "group.prev": ([fn("changeGroupActive")], "changeGroupActive(false)"),
    "group.toggle": ([fn("toggleGroup")], ""),
    "cursor.move": ([fn("moveCursor")], ""),
    "cursor.move_to_corner": ([fn("moveCursorToCorner")], ""),
}

# hl.* Toplevel (ohne hl.dsp.*, hl.get_*)
TOPLEVEL = {
    "hl.animation": ([r"\bvoid\s+animation\("], "Config::animation"),
    "hl.bind": ([r"\bBindHandle\s+bind\("], "Config::bind"),
    "hl.config": ([r"class\s+Config\s*:\s*public\s+Options"], "getippte Felder c.general.gaps_in = 5"),
    "hl.curve": ([r"\bvoid\s+bezier\(", r"\bvoid\s+spring\("], "Config::bezier / Config::spring"),
    "hl.define_submap": ([r"\bvoid\s+submap\("], "Config::submap"),
    "hl.device": ([r"\bvoid\s+device\("], "Config::device"),
    "hl.dispatch": ([r"\brun\s*\(\s*const\s+Action"], "run(Action)"),
    "hl.env": ([r"\bvoid\s+env\("], "Config::env"),
    "hl.exec_cmd": ([r"\bSDispatchResult\s+exec\("], "exec()"),
    "hl.exec_scheduled_prop_refresh_immediately": ([fn("execScheduledPropRefreshImmediately")], "execScheduledPropRefreshImmediately()"),
    "hl.gesture": ([r"\bvoid\s+gesture\("], "Config::gesture"),
    "hl.layer_rule": ([r"\bRuleHandle\s+layerRule\("], "Config::layerRule"),
    "hl.layout.register": ([r"\bvoid\s+addLayout\("], "Config::addLayout"),
    "hl.monitor": ([r"\bvoid\s+monitor\("], "Config::monitor / disableMonitor"),
    "hl.notification.create": ([r"\bNotificationHandle\s+notify\("], "abyss::notify -> NotificationHandle"),
    "hl.notification.get": ([r"\bstd::vector<NotificationHandle>\s+notifications\("], "notifications() + dismissNotifications()"),
    "hl.on": ([r"\bEventHandle\s+on\("], "Config::on(Event::X, fn)"),
    "hl.permission": ([r"\bvoid\s+permission\("], "Config::permission"),
    "hl.timer": ([r"\bTimerHandle\s+timer\("], "Config::timer"),
    "hl.unbind": ([r"\bvoid\s+unbind\(", r"\bvoid\s+unbindAll\("], "Config::unbind / unbindAll"),
    "hl.window_rule": ([r"\bRuleHandle\s+windowRule\("], "Config::windowRule"),
    "hl.workspace_rule": ([r"\bRuleHandle\s+workspaceRule\("], "Config::workspaceRule"),
}

QUERIES = {
    "hl.get_active_monitor": ([fn("activeMonitor")], ""),
    "hl.get_active_special_workspace": ([fn("activeSpecialWorkspace")], ""),
    "hl.get_active_window": ([fn("activeWindow")], ""),
    "hl.get_active_workspace": ([fn("activeWorkspace")], ""),
    "hl.get_config": ([r"\bgetOption\s*\("], "getOption()"),
    "hl.get_current_submap": ([fn("currentSubmap")], ""),
    "hl.get_cursor_pos": ([fn("cursorPos")], ""),
    "hl.get_last_window": ([fn("lastWindow")], ""),
    "hl.get_last_workspace": ([fn("lastWorkspace")], ""),
    "hl.get_layers": ([fn("layers")], ""),
    "hl.get_loaded_plugins": (None, "Plugins sind in Abyssland komplett entfernt"),
    "hl.get_monitor": ([fn("monitorBySelector")], ""),
    "hl.get_monitor_at": ([fn("monitorAt")], ""),
    "hl.get_monitor_at_cursor": ([fn("monitorAtCursor")], ""),
    "hl.get_monitors": ([fn("monitors")], ""),
    "hl.get_urgent_window": ([fn("urgentWindow")], ""),
    "hl.get_window": ([fn("windowBySelector")], ""),
    "hl.get_windows": ([fn("windows")], ""),
    "hl.get_workspace": ([fn("workspaceBySelector")], ""),
    "hl.get_workspace_windows": ([fn("workspaceWindows")], ""),
    "hl.get_workspaces": ([fn("workspaces")], ""),
    "hl.is_key_down": ([fn("isKeyDown")], ""),
    "hl.version": ([fn("version")], ""),
}

# Rauschen, das im Wiki als hl.* auftaucht, aber keine API ist (Platzhalter, Plugin-Beispiele, Tippfehler)
WIKI_NOISE = {"hl.dsp", "hl.dsp.whatever", "hl.dsp.window", "hl.dsp.workspace", "hl.dsp.group", "hl.dsp.group.active",
              "hl.plugin.csgo_vulkan_fix", "hl.plugin.csgo_vulkan_fix.vkfix_app", "hl.focus.workspace", "hl.workspace",
              "hl.dsp.focus", "hl.dsp.layout", "hl.layout"}
# hl.plugin.* ist Plugin-API: wird als ein Punkt gefuehrt
PLUGIN_ID = "hl.plugin.load"

# Event-Id (Wiki) -> Abysslang-Eventname; None = fehlt. Alles andere: Punkt/Unterstrich -> CamelCase
EVENT_OVERRIDES = {
    "hyprland.start": ("Start", ""),
    "hyprland.shutdown": ("Shutdown", ""),
    "input.keyboard.key": ("KeyboardKey", ""),
    "keybinds.submap": ("Submap", ""),
    "config.props_refreshed": ("PropsRefreshed", ""),
    "config.unload": (None, "kein Gegenstueck (Lua-Zustand gibt es nicht, aber Config::onShutdown/execShutdown decken Aufraeumen beim Beenden ab)"),
    "window.bell": (None, "Event fehlt"),
    "window.minimize": (None, "Event fehlt"),
}

# Fensterregel-Match-Props: Wiki-Prop -> Feld in WindowMatch
WINDOW_MATCH_FIELDS = {
    "class": "cls", "content": "content", "focus": "focus", "fullscreen": "fullscreen",
    "fullscreen_state_client": "fullscreenStateClient", "fullscreen_state_internal": "fullscreenStateInternal",
    "float": "floating", "group": "group", "initial_class": "initialClass", "initial_title": "initialTitle",
    "modal": "modal", "pin": "pinned", "tag": "tag", "title": "title", "workspace": "workspace",
    "xdg_tag": "xdgTag", "xwayland": "xwayland",
}

# Workspace-Regel-Felder: Wiki-Feld -> Feld in WorkspaceRule (sonst snake->camel)
WORKSPACE_FIELDS = {"default": "isDefault"}

# Monitor-Felder: Wiki-Feld -> ("fn", name) | ("opt", feld)
MONITOR_FIELDS = {
    "output": ("fn", "monitor"), "mode": ("fn", "monitor"), "position": ("fn", "monitor"), "scale": ("fn", "monitor"),
    "disabled": ("fn", "disableMonitor"),
    "transform": ("opt", "transform"), "mirror": ("opt", "mirror"), "bitdepth": ("opt", "bitdepth"), "cm": ("opt", "cm"),
    "sdr_eotf": ("opt", "sdrEotf"), "vrr": ("opt", "vrr"), "icc": ("opt", "icc"), "reserved_area": ("opt", "reserved"),
    "supports_wide_color": ("opt", "supportsWideColor"), "supports_hdr": ("opt", "supportsHdr"),
    "sdrbrightness": ("opt", "sdrBrightness"), "sdrsaturation": ("opt", "sdrSaturation"),
    "sdr_min_luminance": ("opt", "sdrMinLuminance"), "sdr_max_luminance": ("opt", "sdrMaxLuminance"),
    "min_luminance": ("opt", "minLuminance"), "max_luminance": ("opt", "maxLuminance"),
    "max_avg_luminance": ("opt", "maxAvgLuminance"),
}

# Bind-Flags: Wiki-Flag -> Feld in BindOpts
BIND_FIELDS = {
    "locked": "locked", "release": "release", "click": "click", "drag": "drag", "long_press": "longPress",
    "repeating": "repeat", "non_consuming": "nonConsuming", "auto_consuming": "autoConsuming", "mouse": "mouse",
    "transparent": "transparent", "ignore_mods": "ignoreMods", "description": "description", "dont_inhibit": "dontInhibit",
    "submap_universal": "submapUniversal", "device": "devices", "allow_input_capture": "allowInputCapture",
}

GESTURE_FIELDS = {  # Wiki-Feld -> Regex im Header
    "action": r"std::string\s+action\b", "direction": r"std::string\s+direction\b", "fingers": r"int\s+fingers\b",
    "disable_inhibit": r"\bdisableInhibit\b", "mods": r"std::string\s+mods\b", "scale": r"\bdouble\s+scale\b",
}

NOTIFICATION_FIELDS = {  # notify(text, ms, color, icon) bzw. NotifyOpts{ms, color, icon, fontSize}
    "text": [r"\bNotificationHandle\s+notify\(const\s+std::string&\s+text\b"],
    "timeout": [r"\bNotificationHandle\s+notify\([^)]*\bint\s+ms\b", r"struct\s+NotifyOpts\s*\{[^}]*\bint\s+ms\b"],
    "icon": [r"\bNotificationHandle\s+notify\([^)]*std::string&\s+icon\b", r"struct\s+NotifyOpts\s*\{[^}]*\bstd::string\s+icon\b"],
    "color": [r"\bNotificationHandle\s+notify\([^)]*std::optional<Color>\s+color\b", r"struct\s+NotifyOpts\s*\{[^}]*std::optional<Color>\s+color\b"],
    "font_size": [r"struct\s+NotifyOpts\s*\{[^}]*\bfloat\s+fontSize\b"],
}

# Notiz fuer fehlende Felder (nur Hinweis im Report)
NOTES = {
    "notification:font_size": "NotifyOpts hat kein fontSize",
}


# ----------------------------------------------------------------------------------------------------------------------
# Hilfsfunktionen
# ----------------------------------------------------------------------------------------------------------------------

def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def snake_to_camel(s):
    parts = s.split("_")
    return parts[0] + "".join(p[:1].upper() + p[1:] for p in parts[1:])


def snake_to_pascal(s):
    return "".join(p[:1].upper() + p[1:] for p in re.split(r"[_.]", s))


def struct_fields(text, name):
    """Feldnamen einer C++-Struct (einfache Deklarationen `Typ name [= x];`)."""
    m = re.search(r"struct\s+" + re.escape(name) + r"\s*\{(.*?)\n\s*\};", text, flags=re.S)
    if not m:
        return set()
    body = strip_comments(m.group(1))
    fields = set(re.findall(r"([A-Za-z_]\w*)\s*(?:=\s*[^;]*)?;", body))
    # Makro-Felder (ABYSS_X) werden separat behandelt
    return fields


def macro_table(text, macro):
    """ABYSS_X(field, "name", type)-Zeilen eines #define-Makros -> {rulename: field}."""
    m = re.search(r"#define\s+" + macro + r"\(X\)(.*?)(?:\n\s*\n|\n#define|\Z)", text, flags=re.S)
    out = {}
    if m:
        for f, n in re.findall(r"X\(\s*(\w+)\s*,\s*\"([^\"]+)\"\s*,", m.group(1)):
            out[n] = f
    return out


class Item:
    __slots__ = ("cat", "id", "status", "note", "via")

    def __init__(self, cat, id, status, note="", via=""):
        self.cat, self.id, self.status, self.note, self.via = cat, id, status, note, via

    def as_dict(self):
        return {"category": self.cat, "id": self.id, "status": self.status, "note": self.note, "via": self.via}


# ----------------------------------------------------------------------------------------------------------------------
# Wiki-Extraktion
# ----------------------------------------------------------------------------------------------------------------------

def table_rows(lines, first_cell_re=r"[A-Za-z][A-Za-z0-9_.\-]*"):
    for ln in lines:
        m = re.match(r"^\|\s*`?(" + first_cell_re + r")`?\s*\|", ln)
        if m and m.group(1).lower() not in ("name", "prop", "effect", "field", "flag", "method", "event", "rule", "direction"):
            yield m.group(1), ln


def parse_option_rows(text, cat_from_heading=None):
    """Optionsnamen aus einem Wiki-Text. Nach `Category: x` zaehlt nur die direkt folgende Tabelle (danach folgen
    z.B. Dispatcher-Tabellen der Layout-Seiten)."""
    names = set()
    cat, in_table = None, False
    for ln in text.splitlines():
        m = re.match(r"^Category:\s*`([^`]+)`", ln)
        if m:
            cat, in_table = m.group(1), False
            continue
        if cat and ln.startswith("|"):
            in_table = True
            m = re.match(r"^\|\s*`?([a-z][a-z0-9_.\-]*)`?\s*\|", ln)
            if m and m.group(1) not in ("name", "prop", "effect", "field"):
                names.add((cat + "." + m.group(1)).replace("-", "_"))
        elif cat and in_table and not ln.startswith("|"):
            cat, in_table = None, False
    return names


def wiki_options(wiki):
    names = {}
    files = [os.path.join(wiki, "core/config-options.md")] + sorted(glob.glob(os.path.join(wiki, "layouts/*.md")))
    for path in files:
        for n in parse_option_rows(read(path)):
            names.setdefault(n, os.path.basename(path))
    return names


def git(wiki, *args):
    import subprocess
    try:
        r = subprocess.run(["git", "-C", wiki] + list(args), capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    return r.stdout if r.returncode == 0 else None


def old_wiki_options(wiki, rev):
    """Optionen im alten Wiki-Stand (Variables.md mit `### Abschnitt` / `_Subcategory x._`)."""
    text = git(wiki, "show", f"{rev}:content/Configuring/Basics/Variables.md")
    if text is None:
        return None
    names, cat = set(), None
    for ln in text.splitlines():
        m = re.match(r"^###\s+(.+)$", ln)
        if m:
            cat = re.sub(r"[^a-z0-9]+", "-", m.group(1).lower()).strip("-")
            continue
        m = re.match(r"^_Subcategory `([^`]+?)\.?`_", ln)
        if m:
            cat = m.group(1)
            continue
        m = re.match(r"^\|\s*`?([a-z][a-z0-9_.\-]*)`?\s*\|", ln)
        if cat and m and m.group(1) not in ("name", "prop", "effect", "field"):
            names.add((cat + "." + m.group(1)).replace("-", "_"))
    return names


def old_wiki_has_word(wiki, rev, word):
    if not word:
        return True
    import subprocess
    try:
        r = subprocess.run(["git", "-C", wiki, "grep", "-q", "-w", "-F", "-e", word, rev], capture_output=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return True
    return r.returncode != 1  # 1 = nirgends gefunden; alles andere (auch Fehler) = vorsichtshalber "kommt vor"


def rebrand(name):
    """Optionen, die beim Umbenennen Hyprland -> Abyssland mitgezogen wurden (misc.disable_hyprland_logo -> disable_abyssland_logo)."""
    return name.replace("hyprland", "abyssland").replace("hyprcursor", "abysscursor").replace("hypr", "abyss")


def aby_options(aby):
    text = read(os.path.join(aby, "src/config/values/ConfigValues.cpp"))
    out = set()
    for n in re.findall(r"\bM[A-Z]\w*<[A-Za-z0-9_:]+>\(\s*\"([^\"]+)\"", text):
        out.add(n.replace(":", ".").replace("-", "_"))
    return out


def wiki_dispatchers(wiki):
    lines = read(os.path.join(wiki, "core/dispatchers.md")).splitlines()
    section, prefix = None, ""
    rows = []
    prefixes = {"General": "", "Window": "window.", "Workspace": "workspace.", "Group": "group.", "Cursor": "cursor."}
    for ln in lines:
        m = re.match(r"^### (.+)$", ln)
        if m:
            section = m.group(1).strip()
            prefix = prefixes.get(section)
            continue
        if prefix is None:
            continue
        m = re.match(r"^\| `([a-z_]+)\((.*?)\)` \|", ln)
        if m:
            rows.append((prefix, m.group(1), m.group(2)))
    count = {}
    for p, n, _ in rows:
        count[p + n] = count.get(p + n, 0) + 1
    ids = []
    for p, n, args in rows:
        key = p + n
        if count[key] > 1:
            # erstes Pflicht-Argument (ohne `?`) bestimmt die Variante
            toks = [t for t in re.findall(r"[a-z_]+\??", args) if not t.endswith("?") and t not in ("mods", "key", "state", "relative", "y")]
            if not toks:  # nur optionale Argumente (z.B. resize({ keep_aspect_ratio? }))
                toks = [t.rstrip("?") for t in re.findall(r"[a-z_]+\??", args) if t.rstrip("?") not in ("window", "action")]
            variant = toks[0] if toks else "?"
            ids.append(f"{key}[{variant}]")
        else:
            ids.append(key)
    return ids


def wiki_events(wiki):
    text = read(os.path.join(wiki, "core/advanced-configuration/events.md"))
    return [m for m in re.findall(r"^\|\s*([a-z_]+(?:\.[a-z_]+)+)\s*\|", text, flags=re.M)]


def wiki_hl_functions(wiki):
    found = set()
    for path in glob.glob(os.path.join(wiki, "**/*.md"), recursive=True):
        found.update(re.findall(r"\bhl\.[a-z_]+(?:\.[a-z_]+)*", read(path)))
    return found


def wiki_table_names(path, header_word):
    """Erste Spalte aller Tabellen, deren Kopf `header_word` ist."""
    out = []
    active = False
    for ln in read(path).splitlines():
        if ln.startswith("|"):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if cells and cells[0].strip("` ") == header_word:
                active = True
                continue
            if active and not re.match(r"^[\s\-|:]+$", ln):
                m = re.match(r"^`?([A-Za-z0-9_\-]+)`?$", cells[0])
                if m:
                    out.append(m.group(1))
        else:
            active = False
    return out


def wiki_tables(path):
    """Alle Tabellen einer Seite: Liste von (Kopf-Zelle 1, Kopf-Zelle 2, [Zeilen als Zellenlisten])."""
    tables, cur = [], None
    for ln in read(path).splitlines():
        if ln.startswith("|"):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if cur is None:
                cur = [cells[0].strip("` "), cells[1] if len(cells) > 1 else "", []]
                tables.append(cur)
            elif re.match(r"^[\s\-|:]+$", ln):
                continue
            else:
                cur[2].append(cells)
        else:
            cur = None
    return tables


def wiki_animation_nodes(wiki):
    text = read(os.path.join(wiki, "core/animations.md"))
    m = re.search(r"### Animation tree.*?```plain\n(.*?)```", text, flags=re.S)
    nodes = []
    if m:
        for ln in m.group(1).splitlines():
            mm = re.match(r"^\s*(?:↳\s*)?([A-Za-z]+)\b", ln)
            if mm:
                nodes.append(mm.group(1))
    return nodes


def wiki_device_fields(wiki):
    text = read(os.path.join(wiki, "core/devices.md"))
    excluded = set()
    m = re.search(r"\*\*EXCEPT\*\*:(.*?)\n\n", text, flags=re.S)
    if m:
        excluded = set(re.findall(r"`([a-z_0-9]+)`", m.group(1)))
    extras = set(re.findall(r"^- `([a-z_]+)` \(only", text, flags=re.M))
    # Input-Optionen (input, input.touchpad, input.touchdevice, input.tablet, ...)
    leaf = set()
    for name in wiki_options(wiki):
        if name.startswith("input."):
            leaf.add(name.split(".")[-1])
    return sorted((leaf - excluded) | extras), excluded


def wiki_permission_types(wiki):
    text = read(os.path.join(wiki, "core/advanced-configuration/permissions.md"))
    m = re.search(r"### Permission list(.*?)\n## ", text, flags=re.S)
    return re.findall(r"^#### ([a-z\-]+)", m.group(1), flags=re.M) if m else []


# ----------------------------------------------------------------------------------------------------------------------
# Pruefungen
# ----------------------------------------------------------------------------------------------------------------------

def src_has_string(aby, literal, subdirs=("src",)):
    """Kommt "literal" (in Anfuehrungszeichen) irgendwo im Quelltext vor? Fuer die 'nur via extra'-Pruefung."""
    pat = re.compile(r'"' + re.escape(literal) + r'"')
    for sd in subdirs:
        for dp, _, fs in os.walk(os.path.join(aby, sd)):
            for f in fs:
                if f.endswith((".cpp", ".hpp")):
                    try:
                        if pat.search(read(os.path.join(dp, f))):
                            return True
                    except OSError:
                        pass
    return False


def check_mapping(cat, ids, table, header, items, note_for_unmapped="nicht in der Mapping-Tabelle (Skript ergaenzen)"):
    for i in ids:
        if i not in table:
            items.append(Item(cat, i, "missing", note_for_unmapped))
            continue
        syms, note = table[i]
        if syms is None:
            items.append(Item(cat, i, "missing", note))
            continue
        bad = [s for s in syms if not re.search(s, header)]
        if bad:
            items.append(Item(cat, i, "missing", "Symbol im Header nicht gefunden: " + ", ".join(bad)))
        else:
            items.append(Item(cat, i, "ok", note))


def run(root, rev=None, tag=None):
    wiki = os.path.join(root, WIKI_REL)
    aby = os.path.join(root, ABY_REL)
    if not os.path.isdir(wiki):
        sys.exit(f"Wiki nicht gefunden: {wiki}")
    if not os.path.isdir(aby):
        sys.exit(f"Abyssland nicht gefunden: {aby}")

    hdr_raw = {os.path.basename(p): read(p) for p in glob.glob(os.path.join(aby, "include/abyss/*.hpp"))}
    header = "\n".join(strip_comments(t) for t in hdr_raw.values())
    config_hpp = hdr_raw.get("config.hpp", "")
    effects_hpp = hdr_raw.get("effects.hpp", "")
    items = []
    extra_in_aby = {}

    # --- options ---
    w_opts = wiki_options(wiki)
    a_opts = aby_options(aby)
    renamed = set()
    for n in sorted(w_opts):
        if n in a_opts:
            items.append(Item("options", n, "ok"))
            continue
        alt = rebrand(n)
        if alt in a_opts:
            renamed.add(alt)
            items.append(Item("options", n, "ok", f"umbenannt: {alt}"))
        else:
            items.append(Item("options", n, "missing", f"nicht in ConfigValues.cpp, Compositor-Feature fehlt (Wiki: {w_opts[n]})"))
    extra_in_aby["options"] = sorted(a_opts - set(w_opts) - renamed)

    # --- toplevel ---
    fns = {f for f in wiki_hl_functions(wiki) if f not in WIKI_NOISE and not f.startswith("hl.dsp") and not f.startswith("hl.get_")
           and f != "hl.is_key_down" and f != "hl.version" and not f.startswith("hl.plugin")}
    fns.add(PLUGIN_ID)
    table = dict(TOPLEVEL)
    table[PLUGIN_ID] = (None, "Plugins sind in Abyssland komplett entfernt (hl.plugin.*)")
    check_mapping("toplevel", sorted(fns), table, header, items)

    # --- queries ---
    q = {f for f in wiki_hl_functions(wiki) if f.startswith("hl.get_") and f not in WIKI_NOISE}
    q |= {"hl.is_key_down", "hl.version"}
    check_mapping("queries", sorted(q), QUERIES, header, items)

    # --- dispatchers ---
    check_mapping("dispatchers", wiki_dispatchers(wiki), DISPATCHERS, header, items)

    # --- events ---
    aby_events = set(re.findall(r"inline constexpr EventTag<[^>]*(?:<[^>]*>[^>]*)*>\s+(\w+)\{\}", strip_comments(hdr_raw.get("events.hpp", ""))))
    aby_events |= set(re.findall(r">\s+(\w+)\{\}\s*;", hdr_raw.get("events.hpp", "")))
    for e in wiki_events(wiki):
        if e in EVENT_OVERRIDES:
            name, note = EVENT_OVERRIDES[e]
        else:
            name, note = snake_to_pascal(e), ""
        if name is None:
            items.append(Item("events", e, "missing", note))
        elif name in aby_events:
            items.append(Item("events", e, "ok", f"Event::{name}"))
        else:
            items.append(Item("events", e, "missing", f"Event::{name} nicht in events.hpp"))

    # --- window match ---
    wm_fields = struct_fields(config_hpp, "WindowMatch")
    for p in wiki_table_names(os.path.join(wiki, "core/rules/window-rules.md"), "Prop"):
        f = WINDOW_MATCH_FIELDS.get(p)
        if f and f in wm_fields:
            items.append(Item("window_match", p, "ok", f"WindowMatch::{f}"))
        elif src_has_string(aby, p, ("src/desktop/rule", "src/config")):
            items.append(Item("window_match", p, "extra", "nur ueber WindowMatch::extra"))
        else:
            items.append(Item("window_match", p, "missing", "kein Feld in WindowMatch"))

    # --- window effects ---
    we = macro_table(effects_hpp, "ABYSS_WINDOW_EFFECTS")
    seen = set()
    for e in wiki_table_names(os.path.join(wiki, "core/rules/window-rules.md"), "Effect"):
        if e in seen:
            continue
        seen.add(e)
        if e in we:
            items.append(Item("window_effects", e, "ok", f"WindowEffects::{we[e]}"))
        elif src_has_string(aby, e, ("src/desktop/rule", "src/config")):
            items.append(Item("window_effects", e, "extra", "nur ueber WindowEffects::extra"))
        else:
            items.append(Item("window_effects", e, "missing", "weder Feld in WindowEffects noch in der Regel-Engine (Compositor-Feature fehlt)"))
    extra_in_aby["window_effects"] = sorted(set(we) - seen)

    # --- layer rules ---
    le = macro_table(effects_hpp, "ABYSS_LAYER_EFFECTS")
    lm = struct_fields(config_hpp, "LayerMatch")
    lpath = os.path.join(wiki, "core/rules/layer-rules.md")
    for p in wiki_table_names(lpath, "Prop"):
        if p == "namespace" and "nspace" in lm:
            items.append(Item("layer_rules", "match:" + p, "ok", "LayerMatch::nspace"))
        else:
            items.append(Item("layer_rules", "match:" + p, "missing", "kein Feld in LayerMatch"))
    for e in wiki_table_names(lpath, "Effect"):
        if e in le:
            items.append(Item("layer_rules", e, "ok", f"LayerEffects::{le[e]}"))
        elif src_has_string(aby, e, ("src/desktop/rule", "src/config")):
            items.append(Item("layer_rules", e, "extra", "nur ueber LayerEffects::extra"))
        else:
            items.append(Item("layer_rules", e, "missing", "weder Feld in LayerEffects noch Regel-Engine"))

    # --- workspace rules ---
    wr = struct_fields(config_hpp, "WorkspaceRule")
    for r in wiki_table_names(os.path.join(wiki, "core/rules/workspace-rules.md"), "Rule"):
        f = WORKSPACE_FIELDS.get(r, snake_to_camel(r))
        if f in wr:
            items.append(Item("workspace_rules", r, "ok", f"WorkspaceRule::{f}"))
        else:
            items.append(Item("workspace_rules", r, "missing", f"WorkspaceRule::{f} fehlt"))

    # --- monitor ---
    mo = struct_fields(config_hpp, "MonitorOpts")
    for fld in wiki_table_names(os.path.join(wiki, "core/monitors/_index.md"), "Field"):
        kind, nm = MONITOR_FIELDS.get(fld, ("opt", snake_to_camel(fld)))
        if kind == "fn":
            ok = re.search(fn(nm), header) is not None
            items.append(Item("monitor", fld, "ok" if ok else "missing", f"Config::{nm}" if ok else f"{nm}() fehlt"))
        else:
            ok = nm in mo
            items.append(Item("monitor", fld, "ok" if ok else "missing", f"MonitorOpts::{nm}" if ok else f"MonitorOpts::{nm} fehlt"))

    # --- device ---
    dev = macro_table(effects_hpp, "ABYSS_DEVICE_FIELDS")
    dfields, _excl = wiki_device_fields(wiki)
    for d in dfields:
        items.append(Item("device", d, "ok" if d in dev else "missing", f"DeviceOpts::{dev[d]}" if d in dev else "kein Feld in DeviceOpts"))
    extra_in_aby["device"] = sorted(set(dev) - set(dfields))

    # --- binds ---
    bo = struct_fields(config_hpp, "BindOpts")
    for fl in wiki_table_names(os.path.join(wiki, "core/binds/flags.md"), "Flag"):
        f = BIND_FIELDS.get(fl, snake_to_camel(fl))
        items.append(Item("binds", fl, "ok" if f in bo else "missing", f"BindOpts::{f}" if f in bo else f"BindOpts::{f} fehlt"))

    # --- gestures ---
    gpath = os.path.join(wiki, "core/binds/gestures.md")
    gt = wiki_tables(gpath)
    gest_event = {"delta.x": r"\bVec2\s+delta\b", "delta.y": r"\bVec2\s+delta\b", "fingers": r"\bint\s+fingers\b",
                  "rotation": r"\bdouble\s+rotation\b", "scale": r"\bdouble\s+scale\b", "time_ms": r"\btimeMs\b",
                  "type": r"std::string\s+type\b", "cancelled": r"\bbool\s+cancelled\b"}
    seen_g = set()
    nfield = 0
    for head, _h2, rows in gt:
        names = [re.sub(r"[`_]", "", r[0]) if r[0].startswith("_") else r[0].strip("`") for r in rows]
        if head == "Field":
            nfield += 1
            for n in names:
                if nfield == 1:
                    pat, gid = GESTURE_FIELDS.get(n), n
                else:
                    pat, gid = gest_event.get(n), "event." + n
                if gid in seen_g:
                    continue
                seen_g.add(gid)
                ok = pat is not None and re.search(pat, header) is not None
                items.append(Item("gestures", gid, "ok" if ok else "missing", "" if ok else "kein Feld"))
        elif head == "Direction":
            for n in names:
                ok = src_has_string(aby, n, ("src/config/native", "src/managers/input/trackpad")) or re.search(r"\b" + n + r"\b", config_hpp, flags=re.I) is not None
                items.append(Item("gestures", "direction." + n, "ok" if ok else "missing", "" if ok else "Richtung unbekannt"))
        elif head == "Action":
            for n in names:
                if n.lower().replace(" ", "") == "luafunction":
                    ok = "GestureCallbacks" in header
                    items.append(Item("gestures", "action.callback", "ok" if ok else "missing", "GestureCallbacks / std::function" if ok else ""))
                else:
                    ok = re.search(r"\b" + n + r"\b", config_hpp) is not None
                    items.append(Item("gestures", "action." + n, "ok" if ok else "missing", "" if ok else "Aktion unbekannt"))

    # --- notification ---
    npath = os.path.join(wiki, "core/advanced-configuration/notifications.md")
    nt = wiki_tables(npath)
    if nt:
        for r in nt[0][2]:
            fl = r[0].strip("` ")
            pats = NOTIFICATION_FIELDS.get(fl)
            ok = pats is not None and all(re.search(p, header, flags=re.S) for p in pats)
            items.append(Item("notification", fl, "ok" if ok else "missing", "" if ok else NOTES.get("notification:" + fl, "kein Parameter")))
    if len(nt) > 1:
        for r in nt[1][2]:
            for alias in re.split(r"/", r[1].strip("` ")):
                alias = alias.strip()
                ok = src_has_string(aby, alias, ("src/config/native",)) or re.search(r"\b" + re.escape(alias) + r"\b", hdr_raw.get("actions.hpp", "")) is not None
                items.append(Item("notification", "icon." + alias, "ok" if ok else "missing", "" if ok else "Icon-Name unbekannt"))

    # --- animations ---
    nodes = set(re.findall(r'createNode\(\s*"([^"]+)"', read(os.path.join(aby, "src/config/shared/animation/AnimationTree.cpp"))))
    for n in dict.fromkeys(wiki_animation_nodes(wiki)):
        items.append(Item("animations", n, "ok" if n in nodes else "missing", "" if n in nodes else "Knoten fehlt im AnimationTree"))

    # --- permissions ---
    abysslang = read(os.path.join(aby, "src/config/native/Abysslang.cpp"))
    for t in wiki_permission_types(wiki):
        if t == "plugin":
            items.append(Item("permissions", t, "missing", "Plugins sind in Abyssland komplett entfernt"))
        else:
            ok = ('"%s"' % t) in abysslang
            items.append(Item("permissions", t, "ok" if ok else "missing", "" if ok else "Typ in Config::permission unbekannt"))

    if tag and mark_newer_tag(items, aby, tag):
        pass
    elif rev:
        mark_newer(items, os.path.join(root, "_upstream/hyprland-wiki"), rev)
    return items, extra_in_aby


def leaf_of(it):
    i = it.id
    if it.cat == "dispatchers":
        i = re.sub(r"\[.*\]$", "", i).split(".")[-1]
    elif it.cat == "gestures":
        i = i.split(".")[-1]
    elif it.cat == "layer_rules":
        i = i.replace("match:", "")
    else:
        i = re.split(r"[.]", i)[-1]
    return i


def mark_newer(items, wiki, rev):
    """Fehlende Punkte, die im Wiki-Stand der Release (rev, Standard d5f07c6 = vor v0.56.2) noch nicht standen, bekommen den
    Status `newer` (neuer als 0.56.2, zaehlt nicht als Luecke)."""
    old_opts = old_wiki_options(wiki, rev)
    if old_opts is None and git(wiki, "rev-parse", "--verify", rev + "^{commit}") is None:
        return  # Revision nicht im Klon: nichts markieren
    for it in items:
        if it.status != "missing":
            continue
        if it.cat == "options":
            if old_opts is not None and it.id not in old_opts:
                it.status, it.note = "newer", "erst nach 0.56.2 im Wiki"
        else:
            if not old_wiki_has_word(wiki, rev, leaf_of(it)):
                it.status, it.note = "newer", "erst nach 0.56.2 im Wiki"


def tag_has(aby, tag, it):
    """Gibt es den fehlenden Punkt im Upstream-Tag (Quelltext von Hyprland 0.56.2, im Abyssland-Repo)? None = nicht pruefbar."""
    if git(aby, "rev-parse", "--verify", tag + "^{commit}") is None:
        return None

    def grep(lit, *paths):
        import subprocess
        r = subprocess.run(["git", "-C", aby, "grep", "-q", "-F", "-e", lit, tag, "--"] + list(paths or ["src"]), capture_output=True, timeout=60)
        return r.returncode == 0

    if it.cat == "options":
        return grep('"' + it.id.replace(".", ":") + '"', "src/config/values/ConfigValues.cpp")
    if it.cat == "events":
        ev = git(aby, "show", f"{tag}:src/config/lua/LuaEventHandler.cpp") or ""
        return ('"' + it.id + '"') in ev
    leaf = leaf_of(it)
    if it.cat in ("window_effects", "layer_rules"):
        return grep('"' + leaf + '"', "src/config/lua", "src/desktop/rule")
    if it.cat == "toplevel":  # hl.notification.get -> "get", hl.x -> "x"
        return grep('"' + leaf + '"', "src/config/lua")
    return grep('"' + leaf + '"', "src/config/lua")


def mark_newer_tag(items, aby, tag):
    """Fehlende Punkte, die im Upstream-Tag (Standard v0.56.2) nicht existieren, bekommen `newer` (keine Luecke).
    Rueckgabe False, wenn der Tag nicht im Repo ist."""
    if git(aby, "rev-parse", "--verify", tag + "^{commit}") is None:
        return False
    for it in items:
        if it.status == "missing" and tag_has(aby, tag, it) is False:
            it.status, it.note = "newer", f"nicht in {tag} (Upstream-Quelltext)"
    return True


def load_allow(path):
    allow = {}
    if path and os.path.isfile(path):
        for ln in read(path).splitlines():
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            key, _, why = ln.partition("#")
            key = key.strip()
            allow[key] = why.strip()
    return allow


def summarize(items, allow):
    stats = {}
    unknown_allow = set(allow)
    for it in items:
        key = f"{it.cat}:{it.id}"
        unknown_allow.discard(key)
        s = stats.setdefault(it.cat, {"total": 0, "ok": 0, "extra": 0, "newer": 0, "allowed": 0, "missing": 0})
        s["total"] += 1
        if it.status == "ok":
            s["ok"] += 1
        elif key in allow:
            it.status = "allowed"
            it.note = allow[key] or it.note
            s["allowed"] += 1
        elif it.status == "extra":
            s["extra"] += 1
        elif it.status == "newer":
            s["newer"] += 1
        else:
            s["missing"] += 1
    return stats, sorted(unknown_allow)


def fmt_text(items, stats, unknown_allow, extra_in_aby, quiet):
    out = []
    out.append("Abysslang-Abdeckung gegen das Hyprland-Wiki (Lua)\n")
    out.append(f"{'Kategorie':<30} {'Wiki':>5} {'ok':>5} {'via extra':>9} {'bewusst':>8} {'neuer':>6} {'fehlt':>6}")
    tot = {"total": 0, "ok": 0, "extra": 0, "newer": 0, "allowed": 0, "missing": 0}
    for cat, title in CATEGORY_TITLES:
        s = stats.get(cat)
        if not s:
            continue
        out.append(f"{title:<30} {s['total']:>5} {s['ok']:>5} {s['extra']:>9} {s['allowed']:>8} {s['newer']:>6} {s['missing']:>6}")
        for k in tot:
            tot[k] += s[k]
    out.append(f"{'GESAMT':<30} {tot['total']:>5} {tot['ok']:>5} {tot['extra']:>9} {tot['allowed']:>8} {tot['newer']:>6} {tot['missing']:>6}")
    covered = tot["ok"] + tot["extra"] + tot["allowed"]
    out.append(f"\nAbgedeckt (ok + via extra + bewusst): {covered} von {tot['total']}; "
               f"davon abgesehen {tot['newer']} Punkte, die erst nach 0.56.2 ins Wiki kamen (zaehlen nicht als Luecke), {tot['missing']} echte Luecken")
    if not quiet:
        miss = [i for i in items if i.status == "missing"]
        if miss:
            out.append("\nFehlt:")
            for cat, title in CATEGORY_TITLES:
                ms = [i for i in miss if i.cat == cat]
                if ms:
                    out.append(f"  [{title}]")
                    for i in ms:
                        out.append(f"    {i.id}" + (f"  -- {i.note}" if i.note else ""))
        nw = [i for i in items if i.status == "newer"]
        if nw:
            out.append("\nNeuer als 0.56.2 (im Tag v0.56.2 nicht vorhanden, kein Fehler):")
            for i in nw:
                out.append(f"    {i.cat}:{i.id}")
        ex = [i for i in items if i.status == "extra"]
        if ex:
            out.append("\nNur ueber das generische `extra`-Feld erreichbar (kein eigenes Feld):")
            for i in ex:
                out.append(f"    {i.cat}:{i.id}")
    if unknown_allow:
        out.append("\nAllow-Eintraege ohne Treffer (veraltet?): " + ", ".join(unknown_allow))
    return "\n".join(out)


def fmt_md(items, stats, unknown_allow, extra_in_aby, allow, date):
    tot = {"total": 0, "ok": 0, "extra": 0, "newer": 0, "allowed": 0, "missing": 0}
    L = []
    L.append("# Abdeckung Hyprland-Lua gegen Abysslang\n")
    L.append(f"Stand: {date}. Erzeugt von `tools/abysslang-coverage.py` (Aufruf: `python3 tools/abysslang-coverage.py --md docs/abdeckung-abysslang.md`).\n")
    L.append("Verglichen wird das Hyprland-Wiki (Lua-Referenz, Klon unter `_upstream/hyprland-wiki`) mit `Abyssland/include/abyss/*.hpp`, der Optionstabelle "
             "`src/config/values/ConfigValues.cpp` und dem Animationsbaum. Neu erzeugen statt von Hand pflegen.\n")
    L.append("Spalten: **ok** = vorhanden, **via extra** = nur ueber das generische `extra`-Feld einer Regel erreichbar, **bewusst** = in "
             "`tools/abysslang-coverage.allow` mit Begruendung ausgelassen, **neuer** = steht nicht im Upstream-Tag `v0.56.2` des Abyssland-Repos (git grep auf den Quelltext; ohne Tag ersatzweise Wiki-Stand `d5f07c6`), **fehlt** = echte Luecke.\n")
    L.append("| Kategorie | Wiki | ok | via extra | bewusst | neuer | fehlt |")
    L.append("|---|---:|---:|---:|---:|---:|---:|")
    for cat, title in CATEGORY_TITLES:
        s = stats.get(cat)
        if not s:
            continue
        L.append(f"| {title} | {s['total']} | {s['ok']} | {s['extra']} | {s['allowed']} | {s['newer']} | {s['missing']} |")
        for k in tot:
            tot[k] += s[k]
    L.append(f"| **Gesamt** | **{tot['total']}** | **{tot['ok']}** | **{tot['extra']}** | **{tot['allowed']}** | **{tot['newer']}** | **{tot['missing']}** |\n")
    covered = tot["ok"] + tot["extra"] + tot["allowed"]
    L.append(f"Abgedeckt (ok + via extra + bewusst): **{covered} von {tot['total']}**. Neuer als 0.56.2: {tot['newer']}. Echte Luecken: **{tot['missing']}**.\n")

    L.append("## Fehlende Punkte\n")
    any_missing = False
    for cat, title in CATEGORY_TITLES:
        ms = [i for i in items if i.cat == cat and i.status == "missing"]
        if not ms:
            continue
        any_missing = True
        L.append(f"### {title}\n")
        for i in ms:
            L.append(f"- `{i.id}`" + (f": {i.note}" if i.note else ""))
        L.append("")
    if not any_missing:
        L.append("Keine.\n")

    nw = [i for i in items if i.status == "newer"]
    if nw:
        L.append("## Neuer als 0.56.2\n")
        L.append("Diese Punkte stehen im Wiki-HEAD, existieren aber im Upstream-Quelltext `v0.56.2` nicht (Optionen in ConfigValues.cpp, Events in LuaEventHandler.cpp, Effekte/Dispatcher in src/config/lua). Kein Fehler, aber vorgemerkt fuer spaeter.\n")
        for cat, title in CATEGORY_TITLES:
            ns = [i for i in nw if i.cat == cat]
            if ns:
                L.append(f"- {title}: " + ", ".join(f"`{i.id}`" for i in ns))
        L.append("")
    ex = [i for i in items if i.status == "extra"]
    if ex:
        L.append("## Nur ueber `extra` erreichbar\n")
        L.append("Die Regel-Engine kennt den Namen, aber es gibt kein typisiertes Feld (`{\"name\", \"wert\"}` in `extra`).\n")
        for i in ex:
            L.append(f"- `{i.cat}:{i.id}`")
        L.append("")

    al = [i for i in items if i.status == "allowed"]
    if al:
        L.append("## Bewusst ausgelassen\n")
        for i in al:
            L.append(f"- `{i.cat}:{i.id}`: {i.note}")
        L.append("")

    L.append("## Nur in Abyssland, nicht im Wiki\n")
    for k, v in extra_in_aby.items():
        if v:
            L.append(f"- {k} ({len(v)}): " + ", ".join(f"`{x}`" for x in v[:40]) + (" ..." if len(v) > 40 else ""))
    L.append("")
    L.append("## Grenzen der Pruefung\n")
    L.append("- Geprueft wird, ob ein Name/Symbol existiert, nicht ob es dasselbe Verhalten hat. Einschraenkungen (z.B. Aktionen ohne Fenster-Selektor) stehen in `docs/luecken-lua-abysslang.md`.")
    L.append("- Objekt-Felder (Window-/Monitor-/Workspace-Eigenschaften in Lua) und Layout-Nachrichten (`hl.dsp.layout(\"...\")`) sind im Wiki keine feste Tabelle und werden nicht automatisch verglichen; `layoutMsg()` reicht Strings durch.")
    L.append("- Die Mapping-Tabellen im Skript (`DISPATCHERS`, `TOPLEVEL`, `QUERIES`, `EVENT_OVERRIDES`, ...) muessen bei neuen Wiki-Eintraegen ergaenzt werden; unbekannte Eintraege erscheinen als fehlend mit dem Hinweis \"nicht in der Mapping-Tabelle\".")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--root", default=DEFAULT_ROOT, help="AbysslandWC-Wurzel (Standard: Elternordner von tools/)")
    ap.add_argument("--json", action="store_true", help="Maschinenausgabe als JSON")
    ap.add_argument("--allow", default=None, help="Allow-Datei (Standard: neben dem Skript, falls vorhanden)")
    ap.add_argument("--md", default=None, help="zusaetzlich einen Markdown-Bericht (deutsch) in diese Datei schreiben")
    ap.add_argument("--date", default="2026-10-12", help="Stand-Datum im Markdown-Bericht")
    ap.add_argument("--wiki-rev", default="d5f07c6", help="Wiki-Commit der Release 0.56.2 (Punkte, die dort fehlten, gelten als 'neuer'); 'none' schaltet das ab")
    ap.add_argument("--tag", default="v0.56.2", help="Upstream-Tag im Abyssland-Repo als Wahrheit fuer 'neuer als 0.56.2' (hat Vorrang vor --wiki-rev); 'none' schaltet ab")
    ap.add_argument("--quiet", action="store_true", help="nur die Zusammenfassung ausgeben")
    args = ap.parse_args()

    allow_path = args.allow or os.path.join(HERE, "abysslang-coverage.allow")
    allow = load_allow(allow_path)
    items, extra_in_aby = run(args.root, None if args.wiki_rev == "none" else args.wiki_rev, None if args.tag == "none" else args.tag)
    stats, unknown_allow = summarize(items, allow)
    missing = sum(s["missing"] for s in stats.values())

    if args.md:
        with open(args.md, "w", encoding="utf-8") as f:
            f.write(fmt_md(items, stats, unknown_allow, extra_in_aby, allow, args.date))
    if args.json:
        json.dump({
            "summary": stats,
            "missing_total": missing,
            "items": [i.as_dict() for i in items],
            "only_in_abyssland": extra_in_aby,
            "stale_allow_entries": unknown_allow,
        }, sys.stdout, ensure_ascii=False, indent=2)
        print()
    else:
        print(fmt_text(items, stats, unknown_allow, extra_in_aby, args.quiet))
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
