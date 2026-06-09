set shell := ["bash", "-eu", "-o", "pipefail", "-c"]
bluos_host := env_var_or_default("BLUOS_HOST", "")
bluos_port := env_var_or_default("BLUOS_PORT", "11000")
daemon_pid := "/tmp/bluos-knob-daemon.pid"
daemon_log := "/tmp/bluos-knob-daemon.log"

install:
    python3 -m pip install -e ".[dev]"

install-hid:
    python3 -m pip install -e ".[dev,hid]"

setup-hid:
    if command -v brew >/dev/null; then brew list hidapi >/dev/null 2>&1 || brew install hidapi; else echo "Homebrew is required to install native hidapi on macOS." >&2; exit 1; fi
    python3 -m pip install -e ".[dev,hid]"

doctor-hid:
    python3 -c "import hid; print(f'hidapi import OK; {len(hid.enumerate())} HID devices visible')"

list-devices:
    python3 -m bluos_knob.cli.main list

capture-gestures:
    bash scripts/capture_anticater_gestures.sh

bluos-doctor host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-doctor <host>" >&2; exit 2)
    python3 scripts/bluos_readonly.py --host "{{host}}" --port "{{port}}" doctor

bluos-status host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-status <host>" >&2; exit 2)
    python3 scripts/bluos_readonly.py --host "{{host}}" --port "{{port}}" status

bluos-syncstatus host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-syncstatus <host>" >&2; exit 2)
    python3 scripts/bluos_readonly.py --host "{{host}}" --port "{{port}}" syncstatus

bluos-volume host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-volume <host>" >&2; exit 2)
    python3 scripts/bluos_readonly.py --host "{{host}}" --port "{{port}}" volume

bluos-step-down host=bluos_host step="1" max_db="-24" port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-step-down <host>" >&2; exit 2)
    python3 scripts/bluos_volume_step.py --host "{{host}}" --port "{{port}}" --step-db "-{{step}}" --max-db "{{max_db}}" --i-understand-this-changes-volume

bluos-step-up host=bluos_host step="1" max_db="-24" port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-step-up <host>" >&2; exit 2)
    python3 scripts/bluos_volume_step.py --host "{{host}}" --port "{{port}}" --step-db "{{step}}" --max-db "{{max_db}}" --i-understand-this-changes-volume

bluos-mute-off host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-mute-off <host>" >&2; exit 2)
    python3 scripts/bluos_mute.py --host "{{host}}" --port "{{port}}" --state off --i-understand-this-changes-mute

bluos-mute-on host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-mute-on <host>" >&2; exit 2)
    python3 scripts/bluos_mute.py --host "{{host}}" --port "{{port}}" --state on --i-understand-this-changes-mute

bluos-sources host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-sources <host>" >&2; exit 2)
    python3 scripts/bluos_source.py --host "{{host}}" --port "{{port}}" list

bluos-source-optical host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-source-optical <host>" >&2; exit 2)
    python3 scripts/bluos_source.py --host "{{host}}" --port "{{port}}" switch --source "Optical 1" --input-type-index "optical-2" --safe-db "-40" --i-understand-this-changes-source

bluos-source-spotify host=bluos_host port=bluos_port:
    test -n "{{host}}" || (echo "Set BLUOS_HOST or run: just bluos-source-spotify <host>" >&2; exit 2)
    python3 scripts/bluos_source.py --host "{{host}}" --port "{{port}}" switch --source "Spotify" --safe-db "-40" --i-understand-this-changes-source

daemon host="192.168.1.67" path="auto" max_db="-24":
    python3 scripts/bluos_daemon.py --anticater-path "{{path}}" --bluos-host "{{host}}" --max-db "{{max_db}}" --i-understand-this-controls-the-amplifier

daemon-dry-run host="192.168.1.67" path="auto":
    python3 scripts/bluos_daemon.py --anticater-path "{{path}}" --bluos-host "{{host}}" --dry-run

daemon-once host="192.168.1.67" path="auto" max_db="-24":
    python3 scripts/bluos_daemon.py --anticater-path "{{path}}" --bluos-host "{{host}}" --max-db "{{max_db}}" --limit 1 --i-understand-this-controls-the-amplifier

daemon-start host="192.168.1.67" path="auto" max_db="-24":
    rm -f "{{daemon_pid}}" "{{daemon_log}}"
    nohup sh -c 'echo $$ > "{{daemon_pid}}"; exec python3 scripts/bluos_daemon.py --anticater-path "{{path}}" --bluos-host "{{host}}" --max-db "{{max_db}}" --i-understand-this-controls-the-amplifier' </dev/null >"{{daemon_log}}" 2>&1 &
    sleep 2
    test -s "{{daemon_pid}}"
    ps -p "$(cat "{{daemon_pid}}")" -o pid=,ppid=,stat=,etime=,command=
    sed -n '1,20p' "{{daemon_log}}"

daemon-stop:
    test -s "{{daemon_pid}}" || (echo "No PID file at {{daemon_pid}}" >&2; exit 0)
    kill "$(cat "{{daemon_pid}}")"
    rm -f "{{daemon_pid}}"

daemon-log:
    tail -f "{{daemon_log}}"

test:
    python3 -m pytest

lint:
    python3 -m ruff check .
    python3 scripts/check_boundaries.py

smoke:
    python3 -m bluos_knob.cli.main --help >/dev/null
    python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl >/dev/null
