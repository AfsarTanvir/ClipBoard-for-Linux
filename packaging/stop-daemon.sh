#!/usr/bin/env bash
# Ask a running clipboard-linux to quit, then wait until it is gone.
#   stop-daemon.sh [binary]    binary: used to send --quit when gdbus is missing
# Exit 0: not running (any more). 1: still running after about 5 s.
# 2: it seems to be running but the session bus can't be reached to ask it
#    (e.g. over SSH).
# install.sh calls this before starting a new version (which would otherwise
# refuse to start), uninstall.sh before deleting files.
set -uo pipefail

NAME=io.github.afsartanvir.ClipboardLinux
BINARY=${1:-clipboard-linux}

# A hung daemon never answers, so every step shares one deadline and each
# D-Bus call is short, keeping the whole script to about 5 s.
deadline=$((SECONDS + 5))

have() { command -v "$1" >/dev/null; }

bus_ok=false
# 0 = running, 1 = not running, 2 = cannot tell. Asks the session bus when it
# can (and remembers whether it could), else the process list.
running() {
    if have gdbus; then
        local owner
        if owner=$(gdbus call --session --timeout 1 --dest org.freedesktop.DBus \
                --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.NameHasOwner \
                "$NAME" 2>/dev/null); then
            bus_ok=true
            [[ $owner == *true* ]] && return 0
            return 1
        fi
    fi
    have pgrep || return 2
    pgrep -u "$(id -u)" -x clipboard-linux >/dev/null && return 0
    return 1
}

running
state=$?
if ((state == 1)); then
    exit 0
fi
if have gdbus && ! $bus_ok; then
    echo "Cannot reach your desktop session's D-Bus; run this from a terminal inside the session." >&2
    exit 2
fi

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
exit 1
