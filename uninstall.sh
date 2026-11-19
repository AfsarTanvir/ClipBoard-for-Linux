#!/usr/bin/env bash
# Remove clipboard-linux. Your history database is kept unless you pass --purge.
set -uo pipefail

cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"

"$PREFIX/bin/clipboard-linux" --quit 2>/dev/null

python3 packaging/gnome-shortcuts.py uninstall
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
STATE_HOME="${XDG_STATE_HOME:-$HOME/.local/state}"

rm -f "$CONFIG_HOME/autostart/clipboard-linux.desktop"
rm -f "$PREFIX/bin/clipboard-linux"

if [[ -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules ]]; then
    echo "Removing the udev rule (needs sudo)"
    sudo rm -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules /etc/modules-load.d/clipboard-linux.conf
    sudo udevadm control --reload-rules
fi

if [[ "${1:-}" == "--purge" ]]; then
    rm -rf "$DATA_HOME/clipboard-linux" "$CONFIG_HOME/clipboard-linux" "$STATE_HOME/clipboard-linux"
    # Where earlier builds read their settings from.
    rm -f "$CONFIG_HOME/Unknown Organization/clipboard-linux.conf"
    rmdir "$CONFIG_HOME/Unknown Organization" 2>/dev/null
    echo "History and settings deleted."
else
    echo "History kept in $DATA_HOME/clipboard-linux (run with --purge to delete it)."
fi
