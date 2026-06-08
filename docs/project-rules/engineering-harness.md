# Engineering Harness

**Maturity**: L2 target for the Bluetooth input baseline.

## Boot

Boot command:

```bash
python3 -m bluos_knob.cli.main --help
```

Health check:

```bash
just smoke
```

`just smoke` is hardware-free: it verifies the CLI imports and replays the
committed Anticater sample fixture.

## Interact

Primary interaction is terminal CLI:

```bash
python3 -m bluos_knob.cli.main list
python3 -m bluos_knob.cli.main capture --path "<device-path>" --output capture.local.jsonl --limit 20
python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
```

Real `list` and `capture` require OS-level pairing and HIDAPI. Replay does not
require hardware.

## HID Setup

Native HID capture is part of the engineering harness, not an ad hoc setup step.
Use the named commands:

```bash
just setup-hid
just doctor-hid
just list-devices
just capture-gestures
```

`just setup-hid` installs the native macOS HIDAPI library through Homebrew and
the Python HID extra. `just doctor-hid` verifies the Python `hid` import and
enumeration path. `just list-devices` runs the real candidate listing command.
`just capture-gestures` runs an interactive guided capture session and writes
local ignored `captures/**/*.local.jsonl` files.

## BluOS Read-Only Setup

NAD/BluOS probing is part of the harness, but this layer is intentionally
read-only until safe volume policy exists. Set `BLUOS_HOST` or pass the host as a
recipe argument:

```bash
export BLUOS_HOST=192.168.1.67
just bluos-doctor
just bluos-status
just bluos-syncstatus
just bluos-volume
```

Equivalent one-off form:

```bash
just bluos-doctor 192.168.1.67
```

These commands call `/SyncStatus`, `/Status`, and `/Volume` without parameters.
General volume up/down commands are deliberately absent. The only live mutation
allowed in the harness is a guarded one-step test with an explicit max dB:

```bash
just bluos-step-down 192.168.1.67 1 -24
just bluos-step-up 192.168.1.67 1 -24
```

The command refuses positive steps that would exceed the configured maximum dB.

Guarded mute tests use explicit state, not toggle:

```bash
just bluos-mute-off 192.168.1.67
just bluos-mute-on 192.168.1.67
```

Guarded source switching maps Anticater hold-rotate gestures to explicit BluOS
source shortcuts:

```bash
just bluos-sources 192.168.1.67
just bluos-source-optical 192.168.1.67
just bluos-source-spotify 192.168.1.67
```

`brightness_down` maps to Optical 1 via `inputTypeIndex=optical-2`;
`brightness_up` maps to "resume Spotify" via the discovered `Spotify%3Aplay`
source. Spotify is a service/play context, not a passive `inputTypeIndex`.
Both source-switch commands set an absolute post-switch safety target of
`-40 dB` and refuse configured targets louder than `-24 dB`.

## Observe

Evidence sources:

1. `just test` and `just lint` output.
2. JSONL fixture files from capture.
3. Replay output from `python3 -m bluos_knob.cli.main replay`.
4. Hardware smoke notes under `tests/fixtures/anticater/README.md`.

## History

| Plan | Change | Date |
|---|---|---|
| 001-bluetooth-input-baseline | Established Python CLI boot, smoke, test, lint, and replay evidence loop. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added repeatable HID setup, doctor, and device-list commands after live `list` exposed missing HIDAPI. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added guided gesture capture command for labeled real-device fixtures. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added read-only BluOS doctor/status/syncstatus/volume harness commands; no volume mutation commands. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added guarded one-step BluOS volume test commands with max dB enforcement. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added guarded explicit BluOS mute on/off harness commands. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added guarded BluOS source discovery and Optical/Spotify source switch commands. | 2026-06-05 |
| 001-bluetooth-input-baseline | Added `-40 dB` post-switch safety target for source switching. | 2026-06-05 |
