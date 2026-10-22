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

## Setup

Tested target: Ubuntu 26.04, GNOME 50, Wayland.

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev
./install.sh
```

`install.sh` asks for `sudo` once, to install a udev rule that lets your desktop session use
`/dev/uinput` (needed for automatic pasting). It also:

- installs the app to `~/.local/bin/clipboard-linux`
- starts it at login (`~/.config/autostart/clipboard-linux.desktop`)
- adds the GNOME shortcuts, and moves GNOME's notification list from **Super + V** to **Super + M**

To remove everything: `./uninstall.sh` (add `--purge` to also delete your history).

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
maxItems=25          ; unpinned items to keep

[paste]
auto=true            ; false = only copy, you press Ctrl+V yourself
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

Tech stack: C++20 · Qt 6 (Widgets, SQL, D-Bus) · SQLite · libxcb · Linux uinput
