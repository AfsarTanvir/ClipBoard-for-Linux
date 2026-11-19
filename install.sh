#!/usr/bin/env bash
# Build clipboard-linux, install it for the current user and set it up:
#   ~/.local/bin/clipboard-linux        the app
#   ~/.config/autostart/...desktop      start at login ($XDG_CONFIG_HOME if set)
#   /etc/udev/rules.d/...rules          uinput access for auto-paste (asks for sudo)
#   GNOME shortcuts                     Super+V, Ctrl+Alt+1..9
set -euo pipefail

cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"
BINARY="$PREFIX/bin/clipboard-linux"
UDEV_RULE=/etc/udev/rules.d/60-clipboard-linux-uinput.rules

step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

step "Checking build tools"
missing=()
for tool in cmake g++ pkg-config; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
if ((${#missing[@]})); then
    echo "Missing: ${missing[*]}"
    echo "Install the dependencies first:"
    echo "  sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6sql6-sqlite libxcb1-dev"
    exit 1
fi

step "Building"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_TESTING=OFF
cmake --build build -j"$(nproc)"

step "Installing to $PREFIX"
cmake --install build

step "Enabling start at login"
# XDG variables only count when absolute (the XDG spec says to ignore relative ones).
CONFIG_HOME=$HOME/.config
if [[ ${XDG_CONFIG_HOME:-} == /* ]]; then
    CONFIG_HOME=$XDG_CONFIG_HOME
fi
AUTOSTART_DIR="$CONFIG_HOME/autostart"
mkdir -p "$AUTOSTART_DIR"
sed "s|@BINARY@|$BINARY|" packaging/clipboard-linux.desktop \
    > "$AUTOSTART_DIR/clipboard-linux.desktop"

step "Allowing auto-paste through /dev/uinput"
if [[ -f "$UDEV_RULE" ]] && cmp -s packaging/60-clipboard-linux-uinput.rules "$UDEV_RULE"; then
    echo "udev rule already installed."
else
    echo "This needs sudo to install $UDEV_RULE"
    sudo install -m 644 packaging/60-clipboard-linux-uinput.rules "$UDEV_RULE"
    echo uinput | sudo tee /etc/modules-load.d/clipboard-linux.conf >/dev/null
    sudo modprobe uinput
    sudo udevadm control --reload-rules
    sudo udevadm trigger --name-match=uinput
fi

step "Adding GNOME keyboard shortcuts"
python3 packaging/gnome-shortcuts.py install

step "Starting clipboard-linux"
# Stop an older version first: while it holds the D-Bus name the new one
# refuses to start.
stopped=0
bash packaging/stop-daemon.sh || stopped=$?
if ((stopped == 1)); then
    echo "clipboard-linux is still running (it did not quit), so the new version was not"
    echo "started. Log out and in (or end the process), then run ./install.sh again."
    exit 1
elif ((stopped == 2)); then
    echo "The new version was installed but not started; it starts when you next log in."
    echo "To start it now, run ./install.sh again from a terminal inside your desktop session."
    exit 1
fi
nohup "$BINARY" >/dev/null 2>&1 &
disown

echo
echo "Done. Copy some text, then press Super+V."
