#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -ne 1 ]]; then
  cat <<'EOF'
Usage:
  ./run.sh <your-bluos-ip-address>

What it does:
  - Checks macOS/Homebrew HIDAPI dependency when available
  - Creates .venv if needed
  - Installs BluOS Knob with dev + HID extras
  - Verifies HIDAPI and BluOS connectivity
  - Starts the live daemon in the background
EOF
  exit 0
fi

BLUOS_HOST="$1"
PYTHON_BIN="${PYTHON_BIN:-python3}"
VENV_DIR="${VENV_DIR:-.venv}"
DAEMON_PID="/tmp/bluos-knob-daemon.pid"
DAEMON_LOG="/tmp/bluos-knob-daemon.log"

echo "==> BluOS Knob setup"

if [[ "$(uname -s)" == "Darwin" ]]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required on macOS for native hidapi." >&2
    echo "Install Homebrew, then run this script again." >&2
    exit 1
  fi
  if ! brew list hidapi >/dev/null 2>&1; then
    echo "==> Installing native hidapi with Homebrew"
    brew install hidapi
  fi
fi

if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  echo "==> Creating virtual environment: $VENV_DIR"
  "$PYTHON_BIN" -m venv "$VENV_DIR"
fi

echo "==> Installing Python package"
"$VENV_DIR/bin/python" -m pip install --upgrade pip >/dev/null
"$VENV_DIR/bin/python" -m pip install -e ".[dev,hid]" >/dev/null

echo "==> Checking HIDAPI"
"$VENV_DIR/bin/python" - <<'PY'
import hid
print(f"hidapi import OK; {len(hid.enumerate())} HID devices visible")
PY

echo "==> Checking BluOS player: $BLUOS_HOST"
"$VENV_DIR/bin/python" scripts/bluos_readonly.py --host "$BLUOS_HOST" doctor

echo "==> Listing Anticater candidates"
"$VENV_DIR/bin/python" -m bluos_knob.cli.main list

if [[ -s "$DAEMON_PID" ]] && ps -p "$(cat "$DAEMON_PID")" >/dev/null 2>&1; then
  echo "==> Stopping existing daemon $(cat "$DAEMON_PID")"
  kill "$(cat "$DAEMON_PID")"
  sleep 1
fi

rm -f "$DAEMON_PID" "$DAEMON_LOG"

echo "==> Starting daemon"
nohup sh -c "echo \$\$ > '$DAEMON_PID'; exec '$PWD/$VENV_DIR/bin/python' '$PWD/scripts/bluos_daemon.py' --anticater-path auto --bluos-host '$BLUOS_HOST' --max-db '-24' --i-understand-this-controls-the-amplifier" \
  </dev/null >"$DAEMON_LOG" 2>&1 &

sleep 2

if [[ ! -s "$DAEMON_PID" ]] || ! ps -p "$(cat "$DAEMON_PID")" >/dev/null 2>&1; then
  echo "Daemon failed to start. Log:" >&2
  cat "$DAEMON_LOG" >&2 || true
  exit 1
fi

echo
echo "BluOS Knob is running."
echo "  PID:  $(cat "$DAEMON_PID")"
echo "  Log:  $DAEMON_LOG"
echo
echo "Turn the knob."
echo
echo "Watch:"
echo "  tail -f $DAEMON_LOG"
echo
echo "Stop:"
echo "  kill \"\$(cat $DAEMON_PID)\""
