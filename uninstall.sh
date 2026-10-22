#!/usr/bin/env bash
# Remove clipboard-linux. Your history database is kept unless you pass --purge.
set -uo pipefail

cd "$(dirname "$0")"
PREFIX="${PREFIX:-$HOME/.local}"

"$PREFIX/bin/clipboard-linux" --quit 2>/dev/null

python3 packaging/gnome-shortcuts.py uninstall
rm -f "$HOME/.config/autostart/clipboard-linux.desktop"
rm -f "$PREFIX/bin/clipboard-linux"

if [[ -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules ]]; then
    echo "Removing the udev rule (needs sudo)"
    sudo rm -f /etc/udev/rules.d/60-clipboard-linux-uinput.rules /etc/modules-load.d/clipboard-linux.conf
    sudo udevadm control --reload-rules
fi

if [[ "${1:-}" == "--purge" ]]; then
    rm -rf "$HOME/.local/share/clipboard-linux" "$HOME/.config/clipboard-linux"
    echo "History and settings deleted."
else
    echo "History kept in ~/.local/share/clipboard-linux (run with --purge to delete it)."
fi
