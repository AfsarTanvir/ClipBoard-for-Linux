#!/usr/bin/env bash
# Remove clipboard-linux. Your history database is kept unless you pass --purge.
set -uo pipefail

cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"

# XDG variables only count when absolute (XDG spec; Qt ignores relative ones too).
xdg_dir() { if [[ ${1:-} == /* ]]; then echo "$1"; else echo "$2"; fi; }
CONFIG_HOME=$(xdg_dir "${XDG_CONFIG_HOME:-}" "$HOME/.config")
DATA_HOME=$(xdg_dir "${XDG_DATA_HOME:-}" "$HOME/.local/share")
STATE_HOME=$(xdg_dir "${XDG_STATE_HOME:-}" "$HOME/.local/state")

still_running=false
if ! bash packaging/stop-daemon.sh "$PREFIX/bin/clipboard-linux"; then
    still_running=true
    echo "Warning: clipboard-linux did not quit within 5 seconds; it keeps running until you log out."
fi

python3 packaging/gnome-shortcuts.py uninstall

rm -f "$CONFIG_HOME/autostart/clipboard-linux.desktop"
rm -f "$PREFIX/bin/clipboard-linux"

if [[ -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules ]]; then
    echo "Removing the udev rule (needs sudo)"
    sudo rm -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules /etc/modules-load.d/clipboard-linux.conf
    sudo udevadm control --reload-rules
fi

if [[ "${1:-}" == "--purge" ]] && $still_running; then
    echo "Not deleting your history and settings while clipboard-linux is still running."
    echo "Quit it (or log out and in), then run ./uninstall.sh --purge again."
    exit 1
elif [[ "${1:-}" == "--purge" ]]; then
    rm -rf "$DATA_HOME/clipboard-linux" "$CONFIG_HOME/clipboard-linux" "$STATE_HOME/clipboard-linux"
    # Where earlier builds read their settings from.
    rm -f "$CONFIG_HOME/Unknown Organization/clipboard-linux.conf"
    rmdir "$CONFIG_HOME/Unknown Organization" 2>/dev/null
    echo "History and settings deleted."
else
    echo "History kept in $DATA_HOME/clipboard-linux (run with --purge to delete it)."
fi
