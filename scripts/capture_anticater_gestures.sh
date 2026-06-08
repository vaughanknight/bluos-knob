#!/usr/bin/env bash
set -euo pipefail

DEVICE_PATH="${1:-DevSrvsID:4295852156}"
SESSION_DIR="captures/$(date -u +%Y%m%dT%H%M%SZ)-anticater"

mkdir -p "$SESSION_DIR"

cat <<EOF
Anticater VK-01 guided capture
==============================

Device path: $DEVICE_PATH
Output dir:  $SESSION_DIR

For each step:
1. Press Enter to start capture.
2. Perform only the requested gesture a few times.
3. Press Enter again to stop that capture.

The script writes local, unredacted *.local.jsonl files. Do not commit them
until they have been reviewed/redacted.
EOF

capture_gesture() {
  local label="$1"
  local instruction="$2"
  local output="$SESSION_DIR/${label}.local.jsonl"

  printf '\n--- %s ---\n%s\n' "$label" "$instruction"
  read -r -p "Press Enter to START ${label} capture..."

  PYTHONUNBUFFERED=1 python3 -m bluos_knob.cli.main capture \
    --path "$DEVICE_PATH" \
    --output "$output" \
    --timeout-ms 1000 \
    --operation-label "$label" &

  local capture_pid=$!
  read -r -p "Capturing ${label}. Perform the gesture now, then press Enter to STOP..."
  kill -INT "$capture_pid" 2>/dev/null || true
  wait "$capture_pid" 2>/dev/null || true

  local count
  count="$(wc -l < "$output" | tr -d ' ')"
  printf 'Saved %s reports to %s\n' "$count" "$output"
}

capture_gesture "volume-up" "Rotate the knob clockwise / volume up several detents."
capture_gesture "volume-down" "Rotate the knob counter-clockwise / volume down several detents."
capture_gesture "single-click" "Click the knob once, several separate times."
capture_gesture "double-click" "Double-click the knob several times."
capture_gesture "hold-rotate-up" "Push and hold the knob, then rotate clockwise/up several detents."
capture_gesture "hold-rotate-down" "Push and hold the knob, then rotate counter-clockwise/down several detents."

cat <<EOF

Capture session complete.

Summary:
EOF

python3 - <<PY
import collections
import json
import pathlib

session = pathlib.Path("$SESSION_DIR")
for path in sorted(session.glob("*.local.jsonl")):
    raw = collections.Counter()
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        raw[json.loads(line)["data"]] += 1
    print(f"- {path}: {sum(raw.values())} reports {dict(raw.most_common())}")
PY

cat <<EOF

Bring this directory back to the agent:
$SESSION_DIR
EOF

