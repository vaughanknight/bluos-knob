#!/bin/sh
# Keeps the Anticater knob with the ESP32 bridge: if the Mac has paired (or re-paired) the knob,
# disconnect and unpair it. Touches only the knob (by address or ANTICATER name). Run by launchd
# every 10 s (deploy/com.vaughanknight.bluos-knob-guard.plist); silent when there is nothing to do.
set -eu

BLUEUTIL="${BLUEUTIL:-/opt/homebrew/bin/blueutil}"
KNOB_ADDRESS="${KNOB_ADDRESS:-ee-aa-b8-d6-82-5e}"

if ! paired="$("$BLUEUTIL" --paired --format json 2>&1)"; then
    # e.g. blueutil missing or no Bluetooth permission for launchd: say so rather than silently doing nothing
    printf '{"time":"%s","guard":"error","detail":"%s"}\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" \
        "$(printf '%s' "$paired" | head -c 200 | tr '"\n' "' ")"
    exit 1
fi

ids="$(printf '%s' "$paired" | /usr/bin/python3 -c '
import json, sys
want = sys.argv[1].lower().replace(":", "-")
try:
    devices = json.load(sys.stdin)
except ValueError:
    devices = []
for d in devices:
    address = str(d.get("address", "")).lower().replace(":", "-")
    name = str(d.get("name") or "")
    if address == want or "ANTICATER" in name.upper():
        print(address)
' "$KNOB_ADDRESS")"

for id in $ids; do
    "$BLUEUTIL" --disconnect "$id" >/dev/null 2>&1 || true
    if "$BLUEUTIL" --unpair "$id" >/dev/null 2>&1; then result=unpaired; else result=unpair_failed; fi
    printf '{"time":"%s","guard":"%s","address":"%s"}\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')" "$result" "$id"
done
