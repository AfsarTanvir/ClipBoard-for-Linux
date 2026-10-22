#!/usr/bin/env python3
"""Add or remove the GNOME keyboard shortcuts for clipboard-linux.

    gnome-shortcuts.py install     Super+V and Ctrl+Alt+1..9
    gnome-shortcuts.py uninstall   remove them and give Super+V back to GNOME

GNOME on Wayland does not let apps grab global shortcuts, so we register
"custom shortcuts" (the same as Settings > Keyboard > Custom Shortcuts)
that call the running app over D-Bus.
"""
import ast
import subprocess
import sys

MEDIA_KEYS = "org.gnome.settings-daemon.plugins.media-keys"
CUSTOM = MEDIA_KEYS + ".custom-keybinding"
BASE_PATH = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"
OUR_PREFIX = BASE_PATH + "clipboard-linux-"

SHELL_KEYS = "org.gnome.shell.keybindings"
MESSAGE_TRAY = "toggle-message-tray"  # GNOME binds Super+V to the notification list

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


def set_list(schema, key, items):
    gsettings("set", schema, key, "[" + ", ".join(f"'{i}'" for i in items) + "]" if items else "@as []")


def install():
    paths = [p for p in get_list(MEDIA_KEYS, "custom-keybindings") if not p.startswith(OUR_PREFIX)]
    for slug, name, binding, command in SHORTCUTS:
        path = f"{OUR_PREFIX}{slug}/"
        schema = f"{CUSTOM}:{path}"
        gsettings("set", schema, "name", f"'{name}'")
        gsettings("set", schema, "command", f"'{command}'")
        gsettings("set", schema, "binding", f"'{binding}'")
        paths.append(path)
    set_list(MEDIA_KEYS, "custom-keybindings", paths)

    tray = [b for b in get_list(SHELL_KEYS, MESSAGE_TRAY) if b.lower() != "<super>v"]
    if not tray:
        tray = ["<Super>m"]
    set_list(SHELL_KEYS, MESSAGE_TRAY, tray)

    print("Shortcuts installed: Super+V opens the history, Ctrl+Alt+1..9 paste item 1..9.")
    print("The notification list is now on: " + ", ".join(tray))


def uninstall():
    paths = get_list(MEDIA_KEYS, "custom-keybindings")
    for path in paths:
        if path.startswith(OUR_PREFIX):
            gsettings("reset-recursively", f"{CUSTOM}:{path}")
    set_list(MEDIA_KEYS, "custom-keybindings", [p for p in paths if not p.startswith(OUR_PREFIX)])

    tray = get_list(SHELL_KEYS, MESSAGE_TRAY)
    if not any(b.lower() == "<super>v" for b in tray):
        set_list(SHELL_KEYS, MESSAGE_TRAY, ["<Super>v"] + tray)

    print("Shortcuts removed; Super+V opens the notification list again.")


if __name__ == "__main__":
    action = sys.argv[1] if len(sys.argv) > 1 else ""
    if action == "install":
        install()
    elif action == "uninstall":
        uninstall()
    else:
        print(__doc__.strip())
        sys.exit(2)
