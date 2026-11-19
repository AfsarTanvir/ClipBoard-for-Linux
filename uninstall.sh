#!/usr/bin/env bash
# Remove clipboard-linux. Your history database is kept unless you pass --purge.
set -uo pipefail

cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"

# XDG variables only count when absolute (the XDG spec says to ignore relative ones).
xdg_dir() { if [[ ${1:-} == /* ]]; then echo "$1"; else echo "$2"; fi; }
CONFIG_HOME=$(xdg_dir "${XDG_CONFIG_HOME:-}" "$HOME/.config")
DATA_HOME=$(xdg_dir "${XDG_DATA_HOME:-}" "$HOME/.local/share")
STATE_HOME=$(xdg_dir "${XDG_STATE_HOME:-}" "$HOME/.local/state")

still_running=false
if ! bash packaging/stop-daemon.sh; then
    still_running=true
fi
# Decide before removing anything, so a refused --purge leaves everything in place.
if [[ "${1:-}" == "--purge" ]] && $still_running; then
    echo "clipboard-linux is still running (or its state can't be checked), so nothing was"
    echo "removed: its history can't be deleted while it is in use. Log out and in (or end"
    echo "the process), then run ./uninstall.sh --purge again."
    exit 1
fi
if $still_running; then
    echo "Warning: clipboard-linux may still be running; it stops when you log out."
fi

python3 packaging/gnome-shortcuts.py uninstall

rm -f "$CONFIG_HOME/autostart/clipboard-linux.desktop"
rm -f "$PREFIX/bin/clipboard-linux" "$PREFIX/bin/clipboard-linux-shortcuts"

if [[ -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules ]]; then
    echo "Removing the udev rule (needs sudo)"
    sudo rm -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules /etc/modules-load.d/clipboard-linux.conf
    sudo udevadm control --reload-rules
fi

if [[ "${1:-}" == "--purge" ]]; then
    rm -rf "$DATA_HOME/clipboard-linux" "$CONFIG_HOME/clipboard-linux" "$STATE_HOME/clipboard-linux"
    rm -f "$CONFIG_HOME/clipboard-linux.conf" # Qt's organization-wide settings file
    # Where earlier builds read their settings from.
    rm -f "$CONFIG_HOME/Unknown Organization/clipboard-linux.conf" "$CONFIG_HOME/Unknown Organization.conf"
    rmdir "$CONFIG_HOME/Unknown Organization" 2>/dev/null
    echo "History and settings deleted."
else
    echo "History kept in $DATA_HOME/clipboard-linux (run with --purge to delete it)."
fi
