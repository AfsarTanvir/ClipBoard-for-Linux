#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait (up to 5 s) until it is gone.
#   stop-daemon.sh [binary]    binary: used to send --quit when gdbus is missing
# Exit 0: not running (any more). Exit 1: still running.
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux
BINARY=${1:-clipboard-linux}

# 0 = running, 1 = not running, 2 = cannot ask the session bus.
on_bus() {
    local owner
    owner=$(gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.NameHasOwner "$NAME" 2>/dev/null) || return 2
    [[ $owner == *true* ]] && return 0
    return 1
}

running() {
    on_bus
    local rc=$?
    if ((rc == 2)); then
        # No gdbus or no session bus: fall back to the process list.
        pgrep -u "$(id -u)" -x clipboard-linux >/dev/null
        return
    fi
    return "$rc"
}

running || exit 0

# A hung daemon never answers, so don't wait for the reply for long.
if command -v gdbus >/dev/null; then
    gdbus call --session --timeout 5 --dest "$NAME" --object-path /ClipboardLinux \
        --method "$NAME.Quit" >/dev/null 2>&1
else
    timeout 5 "$BINARY" --quit >/dev/null 2>&1
fi

for _ in $(seq 50); do
    running || exit 0
    sleep 0.1
done
exit 1
