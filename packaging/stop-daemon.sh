#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait until it is gone.
# Exit 0: not running (any more). 1: still running (gives up after about 8 s).
# 2: can't tell, because gdbus is missing or the session bus didn't answer
#    (e.g. over SSH); run from a terminal inside the desktop session instead.
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux

# 0 = owns its D-Bus name, 1 = doesn't, 2 = can't ask. $1 = timeout in seconds.
on_bus() {
    local owner
    owner=$(gdbus call --session --timeout "${1:-1}" --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.NameHasOwner \
        "$NAME" 2>/dev/null) || return 2
    [[ $owner == *true* ]] && return 0
    return 1
}

if ! command -v gdbus >/dev/null; then
    echo "gdbus is missing (package libglib2.0-bin), so the running app can't be checked." >&2
    exit 2
fi
on_bus 3
state=$?
if ((state == 2)); then
    echo "Your desktop session's D-Bus didn't answer (no session here, or it is busy); run this" >&2
    echo "from a terminal inside the desktop session." >&2
    exit 2
fi
((state == 1)) && exit 0

# A hung daemon never answers, so the reply gets 3 s and the wait 3 s more.
gdbus call --session --timeout 3 --dest "$NAME" --object-path /ClipboardLinux \
    --method "$NAME.Quit" >/dev/null 2>&1
deadline=$((SECONDS + 3))
while :; do
    on_bus
    (($? == 1)) && exit 0
    ((SECONDS >= deadline)) && exit 1
    sleep 0.1
done
