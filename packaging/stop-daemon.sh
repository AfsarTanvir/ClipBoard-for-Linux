#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait until it is gone.
# Exit 0: not running (any more). 1: still running after about 5 s.
# 2: can't tell, because gdbus is missing or the session bus can't be reached
#    (e.g. over SSH); run from a terminal inside the desktop session instead.
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux

# 0 = owns its D-Bus name, 1 = doesn't, 2 = can't ask.
on_bus() {
    local owner
    owner=$(gdbus call --session --timeout 1 --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.NameHasOwner \
        "$NAME" 2>/dev/null) || return 2
    [[ $owner == *true* ]] && return 0
    return 1
}

if ! command -v gdbus >/dev/null; then
    echo "gdbus is missing (package libglib2.0-bin), so the running app can't be checked." >&2
    exit 2
fi
on_bus
state=$?
if ((state == 2)); then
    echo "Cannot reach your desktop session's D-Bus; run this from a terminal inside the session." >&2
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
