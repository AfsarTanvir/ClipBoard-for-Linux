#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait until it is gone.
#   stop-daemon.sh [binary]    binary: used to send --quit when gdbus is missing
# Exit 0: not running (any more). 1: still running after about 5 s.
# 2: can't tell, because the session bus can't be reached (e.g. over SSH).
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux
BINARY=${1:-clipboard-linux}

have() { command -v "$1" >/dev/null; }

# 0 = running, 1 = not running, 2 = cannot tell.
running() {
    if have gdbus; then
        local owner
        owner=$(gdbus call --session --timeout 2 --dest org.freedesktop.DBus \
            --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.NameHasOwner \
            "$NAME" 2>/dev/null) || return 2
        [[ $owner == *true* ]] && return 0
        return 1
    fi
    if have pgrep; then
        # Without gdbus, fall back to the process list.
        pgrep -u "$(id -u)" -x clipboard-linux >/dev/null && return 0
        return 1
    fi
    return 2
}

running
state=$?
if ((state == 1)); then
    exit 0
fi
if ((state == 2)) && have gdbus; then
    echo "Cannot reach your desktop session's D-Bus; run this from a terminal inside the session." >&2
    exit 2
fi

# A hung daemon never answers, so asking and waiting share one deadline.
deadline=$((SECONDS + 5))
if have gdbus; then
    gdbus call --session --timeout 3 --dest "$NAME" --object-path /ClipboardLinux \
        --method "$NAME.Quit" >/dev/null 2>&1
else
    timeout 3 "$BINARY" --quit >/dev/null 2>&1
fi

if ((state == 2)); then
    # Neither gdbus nor pgrep: nothing to watch, so just give it a moment.
    sleep 1
    exit 0
fi

while ((SECONDS < deadline)); do
    running
    (($? == 1)) && exit 0
    sleep 0.1
done
running
(($? == 1)) && exit 0
exit 1
