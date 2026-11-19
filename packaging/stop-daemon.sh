#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait until it is gone.
# Exit 0: not running (any more). 1: still running (gives up after about 10 s).
# 2: can't tell, because gdbus is missing or the session bus didn't answer
#    (e.g. over SSH); run from a terminal inside the desktop session instead.
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux

# 0 = owns its D-Bus name, 1 = doesn't, 2 = can't ask. $1 = timeout in seconds.
on_bus() {
    local owner
    owner=$(gdbus call --session --timeout "$1" --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.NameHasOwner \
        "$NAME" 2>/dev/null) || return 2
    [[ $owner == *true* ]] && return 0
    return 1
}
cannot_ask() {
    echo "Your desktop session's D-Bus didn't answer (no session here, or it is busy); run this" >&2
    echo "from a terminal inside the desktop session." >&2
    exit 2
}

if ! command -v gdbus >/dev/null; then
    echo "gdbus is missing (package libglib2.0-bin), so the running app can't be checked." >&2
    exit 2
fi
on_bus 3
state=$?
((state == 2)) && cannot_ask
((state == 1)) && exit 0

# Quit is sent again on every pass: a daemon that is still starting up owns
# its name before it can take commands. Every wait is short and the loop is
# bounded by elapsed time, so a hung daemon or bus can't block for long.
deadline=$((SECONDS + 8))
while ((SECONDS < deadline)); do
    gdbus call --session --timeout 2 --dest "$NAME" --object-path /ClipboardLinux \
        --method "$NAME.Quit" >/dev/null 2>&1
    on_bus 1
    state=$?
    ((state == 1)) && exit 0
    sleep 0.2
done
((state == 2)) && cannot_ask
exit 1
