#!/usr/bin/env python3
"""Add or remove the GNOME keyboard shortcuts for clipboard-linux.

    gnome-shortcuts.py install     Super+V and Ctrl+Alt+1..9
    gnome-shortcuts.py uninstall   remove them and undo what install changed on
                                   GNOME's notification-list shortcut

GNOME on Wayland does not let apps grab global shortcuts, so we register
"custom shortcuts" (the same as Settings > Keyboard > Custom Shortcuts)
that call the running app over D-Bus.
"""
import ast
import os
import subprocess
import sys

MEDIA_KEYS = "org.gnome.settings-daemon.plugins.media-keys"
CUSTOM = MEDIA_KEYS + ".custom-keybinding"
BASE_PATH = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"
OUR_PREFIX = BASE_PATH + "clipboard-linux-"

SHELL_KEYS = "org.gnome.shell.keybindings"
MESSAGE_TRAY = "toggle-message-tray"  # GNOME binds Super+V to the notification list

# Records what install changed on the notification list ("removed <Super>v",
# "added <Super>m"), so uninstall undoes only that and keeps the user's own
# bindings. Kept in the state folder, apart from the settings, so resetting
# the settings does not lose it.
STATE_DIR = os.path.join(os.environ.get("XDG_STATE_HOME") or os.path.expanduser("~/.local/state"),
                         "clipboard-linux")
TRAY_MARKER = os.path.join(STATE_DIR, "notification-shortcut-changes")

DBUS_CALL = ("gdbus call --session --dest io.github.afsartanvir.ClipboardLinux "
             "--object-path /ClipboardLinux "
             "--method io.github.afsartanvir.ClipboardLinux.")

SHORTCUTS = [("toggle", "Clipboard history", "<Super>v", DBUS_CALL + "Toggle")] + [
    (f"paste-{n}", f"Paste clipboard item {n}", f"<Control><Alt>{n}", DBUS_CALL + f"PasteIndex {n}")
    for n in range(1, 10)
]


def gsettings(*args):
    return subprocess.run(["gsettings", *args], check=True, text=True,
                          capture_output=True).stdout.strip()


def get_list(schema, key):
    value = gsettings("get", schema, key)
    if value.startswith("@as "):
        value = value[len("@as "):]
    return list(ast.literal_eval(value))


UNREADABLE = "unreadable"


def read_marker():
    """The recorded changes, None if there is no record, or UNREADABLE."""
    try:
        with open(TRAY_MARKER) as f:
            return {line.strip() for line in f if line.strip()}
    except FileNotFoundError:
        return None
    except OSError as e:
        print(f"Warning: cannot read {TRAY_MARKER}: {e}", file=sys.stderr)
        return UNREADABLE


def set_list(schema, key, items):
    gsettings("set", schema, key, "[" + ", ".join(f"'{i}'" for i in items) + "]" if items else "@as []")


def install():
    existing = get_list(MEDIA_KEYS, "custom-keybindings")
    was_installed = any(p.startswith(OUR_PREFIX) for p in existing)
    paths = [p for p in existing if not p.startswith(OUR_PREFIX)]
    for slug, name, binding, command in SHORTCUTS:
        path = f"{OUR_PREFIX}{slug}/"
        schema = f"{CUSTOM}:{path}"
        gsettings("set", schema, "name", f"'{name}'")
        gsettings("set", schema, "command", f"'{command}'")
        gsettings("set", schema, "binding", f"'{binding}'")
        paths.append(path)
    set_list(MEDIA_KEYS, "custom-keybindings", paths)

    before = get_list(SHELL_KEYS, MESSAGE_TRAY)
    removed_super_v = any(b.lower() == "<super>v" for b in before)
    tray = [b for b in before if b.lower() != "<super>v"]
    added_super_m = removed_super_v and not tray
    if added_super_m:
        tray = ["<Super>m"]
    if tray != before:
        set_list(SHELL_KEYS, MESSAGE_TRAY, tray)

    # Only after the change is saved, and merged with an earlier install's.
    # Written even when empty: it tells uninstall this install kept a record.
    recorded = read_marker()
    if recorded == UNREADABLE:
        print("Warning: leaving that record as it is.", file=sys.stderr)
    else:
        changes = set(recorded or ())
        if recorded is None and was_installed:
            # Upgrading from a version that kept no record. Its uninstall
            # always gave Super+V back, so keep doing that. Whether it added
            # Super+M is unknown, so leave Super+M alone.
            changes.add("removed <Super>v")
        if removed_super_v:
            changes.add("removed <Super>v")
        if added_super_m:
            changes.add("added <Super>m")
        try:
            os.makedirs(STATE_DIR, exist_ok=True)
            with open(TRAY_MARKER, "w") as f:
                f.write("".join(f"{c}\n" for c in sorted(changes)))
        except OSError as e:
            print(f"Warning: cannot write {TRAY_MARKER}: {e}; uninstall will only restore Super+V.",
                  file=sys.stderr)

    print("Shortcuts installed: Super+V opens the history, Ctrl+Alt+1..9 paste item 1..9.")
    print("The notification list is now on: " + (", ".join(tray) or "no shortcut"))


def uninstall():
    paths = get_list(MEDIA_KEYS, "custom-keybindings")
    was_installed = any(p.startswith(OUR_PREFIX) for p in paths)
    for path in paths:
        if path.startswith(OUR_PREFIX):
            gsettings("reset-recursively", f"{CUSTOM}:{path}")
    set_list(MEDIA_KEYS, "custom-keybindings", [p for p in paths if not p.startswith(OUR_PREFIX)])

    before = get_list(SHELL_KEYS, MESSAGE_TRAY)
    tray = before
    recorded = read_marker()
    if isinstance(recorded, set):
        if "added <Super>m" in recorded:
            tray = [b for b in tray if b.lower() != "<super>m"]
        restore_super_v = "removed <Super>v" in recorded
    else:
        # No usable record: installed by a version that kept none, which took
        # Super+V. If our shortcuts are gone too, there is nothing to undo.
        restore_super_v = was_installed
    if restore_super_v and not any(b.lower() == "<super>v" for b in tray):
        tray = ["<Super>v"] + tray
    if tray != before:
        set_list(SHELL_KEYS, MESSAGE_TRAY, tray)
    try:
        if os.path.exists(TRAY_MARKER):
            os.remove(TRAY_MARKER)
            os.rmdir(STATE_DIR)
    except OSError:
        pass  # folder not empty, or nothing we can do

    print("Shortcuts removed. The notification list is on: " + (", ".join(tray) or "no shortcut"))


if __name__ == "__main__":
    action = sys.argv[1] if len(sys.argv) > 1 else ""
    if action == "install":
        install()
    elif action == "uninstall":
        uninstall()
    else:
        print(__doc__.strip())
        sys.exit(2)
