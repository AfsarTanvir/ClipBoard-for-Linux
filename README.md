# ClipBoard for Linux

A clipboard history manager for Linux (GNOME on Wayland), modelled on the Windows **Win + V** clipboard.

## Features

- Clipboard history that survives reboots (SQLite)
- **Super + V** opens the history popup
- Navigate with the mouse wheel or arrow keys; paste with **Enter** or a click
- **1–9** in the popup pastes that item immediately
- **Ctrl + Alt + 1 … 9** pastes item 1–9 without opening the popup
- Pin items so they are never removed
- Delete a single item, or clear all unpinned items
- Duplicate copies are merged and moved to the top
- Copies marked secret by password managers are never stored
- Copied files (Ctrl + C in Files) are skipped; only text is kept

## Install (Ubuntu)

For Ubuntu **24.04** or **26.04** with the GNOME desktop.

1. Download the `.deb` for your Ubuntu version from the
   [**latest release**](https://github.com/AfsarTanvir/ClipBoard-for-Linux/releases/latest).
   Not sure which version you have? Run `lsb_release -r`.

   | Your Ubuntu | File |
   |---|---|
   | 24.04 | `clipboard-linux_<version>_ubuntu24.04_amd64.deb` |
   | 26.04 | `clipboard-linux_<version>_ubuntu26.04_amd64.deb` |

2. Install it from the folder you downloaded it to (keep the `./`):

   ```bash
   cd ~/Downloads
   sudo apt install ./clipboard-linux_*_ubuntu24.04_amd64.deb
   ```

That's all. The app starts right away and starts again automatically every time you log in,
including after a restart or shutdown. Copy some text and press **Super + V**.

It runs in the background with no window. The first time it runs, it adds the
keyboard shortcuts (**Super + V**, **Ctrl + Alt + 1 … 9**) and moves GNOME's notification list
from Super + V to **Super + M**.

**Update:** install the newer `.deb` the same way; the running app is restarted on the new version.

**Uninstall:**

```bash
clipboard-linux-shortcuts uninstall     # give Super+V back to GNOME (run as yourself, not sudo)
sudo apt remove clipboard-linux
```

Your history stays in `~/.local/share/clipboard-linux`; delete that folder to remove it too.

## Build from source

Tested target: Ubuntu 26.04, GNOME 50, Wayland.

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev
./install.sh
```

`install.sh` asks for `sudo` once, to install a udev rule that lets your desktop session use
`/dev/uinput` (needed for automatic pasting). It also:

- installs the app to `~/.local/bin/clipboard-linux`
- starts it at login (`~/.config/autostart/clipboard-linux.desktop`, or under `$XDG_CONFIG_HOME` if set)
- adds the GNOME shortcuts, and takes **Super + V** away from GNOME's notification list (it gets
  **Super + M** if no other shortcut is left; GNOME's default already has Super + M)

To remove everything: `./uninstall.sh` (add `--purge` to also delete your history and settings).
Both scripts stop the running app first. If it doesn't quit within about 10 seconds, `install.sh`
stops and asks you to log out and in (or end it) and run it again, and `--purge` removes
nothing until the app is gone. Run them from a terminal inside your desktop session (they
need `gdbus`, which GNOME includes).
Uninstalling undoes what the installer changed on GNOME's notification-list shortcut: it gives
Super + V back if the installer took it, and removes Super + M only if the installer added it.

Earlier builds read their settings from `~/.config/Unknown Organization/clipboard-linux.conf`
(and `~/.config/Unknown Organization.conf`) by mistake. Those files are still used for any
setting missing from the file above, so upgrading keeps your settings; move them over and
delete them when convenient. If a setting in the file above can't be read, the older files are
not used for it: `maxItems` then keeps all history and `auto` turns pasting off.

## Usage

| Action | Keyboard | Mouse |
|---|---|---|
| Open / close the popup | Super + V, Esc | – |
| Move | ↑ ↓ PgUp PgDn Home End | Wheel |
| Paste the selected item | Enter, or 1–9 | Click |
| Pin / unpin | P | Pin button |
| Remove one item | Delete | ✕ button |
| Clear all (pinned items stay) | Ctrl + Delete | "Clear all" |
| Paste item N directly | Ctrl + Alt + N | – |

Command line (talks to the running app):

```bash
clipboard-linux                 # start the background service
clipboard-linux --paste 2       # paste item 2
clipboard-linux --clear-all
clipboard-linux --quit
```

## Settings

`~/.config/clipboard-linux/clipboard-linux.conf` (restart the app after editing):

```ini
[history]
maxItems=100         ; unpinned items to keep (minimum 1; not a number: all kept)

[paste]
auto=true            ; false = only copy, you press Ctrl+V yourself
                     ; (true/false, yes/no, on/off, 1/0; anything else means off)
keys=ctrl+v          ; ctrl+v | ctrl+shift+v (terminals) | shift+insert
```

## How it works on Wayland

Wayland deliberately blocks three things a clipboard manager needs, so each one has a workaround:

| Need | Workaround |
|---|---|
| Read every copy in the background | The app runs through XWayland (`QT_QPA_PLATFORM=xcb`), where GNOME shares clipboard changes |
| Global shortcuts | GNOME custom shortcuts call the app over D-Bus (`gdbus call …`) |
| Paste into the other app | A virtual keyboard on `/dev/uinput` presses Ctrl+V |

## Development

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/clipboard-linux
```

Build the `.deb` packages (needs Docker; one per Ubuntu release, written to `dist/`):

```bash
packaging/build-deb-docker.sh            # Ubuntu 24.04 and 26.04
packaging/build-deb-docker.sh 24.04      # one release
packaging/build-deb.sh                   # only for the Ubuntu you're running (needs dpkg-dev)
```

Tech stack: C++20 · Qt 6 (Widgets, SQL, D-Bus) · SQLite · libxcb · Linux uinput

## License

Copyright (C) 2026 Afsar Tanvir and contributors.

ClipBoard for Linux is free software, released under the
[GNU General Public License v3.0](LICENSE) (GPL-3.0-only). You may use, study, change and share
it. If you share or sell a changed or unchanged copy, you must keep this copyright notice and
provide the full source code under the same license.
