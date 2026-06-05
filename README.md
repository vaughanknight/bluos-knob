# Exotic Knob

```text
 _______  _____ _____ ___ ____   _  ___   _  ___  ____
| ____\ \/ / _ \_   _|_ _/ ___| | |/ / \ | |/ _ \| __ )
|  _|  \  / | | || |  | | |     | ' /|  \| | | | |  _ \
| |___ /  \ |_| || |  | | |___  | . \| |\  | |_| | |_) |
|_____/_/\_\___/ |_| |___\____| |_|\_\_| \_|\___/|____/
```

![Safety](https://img.shields.io/badge/safety-zero%20blown%20speakers-brightgreen)
![BluOS](https://img.shields.io/badge/BluOS-NAD%20M33-blue)
![Knob](https://img.shields.io/badge/knob-Anticater%20VK--01-purple)
![Harness](https://img.shields.io/badge/harness-first-orange)

**Tiny knob. Big stereo energy. Zero blown speakers.**

Exotic Knob is your local, safety-first volume knob for the **NAD M33** and
other **BluOS** players. It takes a tiny Anticater VK-01 Bluetooth knob and
turns it into a proper hi-fi control surface: volume nudges, mute, source
shortcuts, replayable hardware captures, and guardrails that keep the party from
turning into an accidental speaker stress test.

Okay, technically it is "some script that sends volume commands" — but it is
some script that has had some thought put into it. It learns the real HID
reports from the knob, replays them deterministically, talks to BluOS over its
local API, and only changes the amplifier through harnessed commands with
explicit safety limits.

## What works today

| Area | Status |
|---|---|
| Anticater HID discovery | Lists the VK-01 HID interfaces on macOS via HIDAPI. |
| Live knob capture | Captures raw HID reports to JSONL fixtures. |
| Replay | Replays fixtures without hardware and emits normalized events. |
| Gesture learning | Volume up/down, mute, hold-rotate brightness reports, and release/no-op reports are identified. |
| BluOS/NAD probing | Reads `/Status`, `/SyncStatus`, and `/Volume` from a NAD M33. |
| Guarded live commands | Supports bounded dB steps, explicit mute on/off, and guarded source switching. |
| Safety harness | Named `just` commands, tests, lint, boundary checks, smoke checks, and live readbacks. |

## Current gesture plan

| Anticater event | Intended action |
|---|---|
| `volume_up` | Raise NAD volume by a bounded dB step. |
| `volume_down` | Lower NAD volume by a bounded dB step. |
| `mute_toggle` | Toggle or explicitly set BluOS mute through policy. |
| `brightness_down` / hold-rotate down | Switch to **Optical 1** via `inputTypeIndex=optical-2`. |
| `brightness_up` / hold-rotate up | Resume **Spotify** via `Spotify%3Aplay`. |
| release/no-op | Do nothing. |

Source switching sets a post-switch safety target of **`-40 dB`**. Positive
volume steps are guarded by a configured maximum, currently tested with
**`-24 dB`** as the loudness ceiling.

## Get started

```bash
python3 -m pip install -e ".[dev]"
python3 -m exotic_knob.cli.main --help
python3 -m exotic_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
```

## Anticater hardware harness

Pair/connect the knob through the operating system first. The CLI does not do
Bluetooth pairing and does not attempt LED/RGB control.

```bash
just setup-hid
just doctor-hid
just list-devices
just capture-gestures
```

Direct capture is also available:

```bash
python3 -m exotic_knob.cli.main capture \
  --path "DevSrvsID:4295852156" \
  --output capture.local.jsonl \
  --operation-label live-probe
```

`*.local.jsonl` captures can contain local HID paths or serials. Review and
redact local identity fields before sharing or committing fixtures.

## BluOS / NAD harness

Set the NAD/BluOS host explicitly; local IPs are not hardcoded into the app.

```bash
export BLUOS_HOST=192.168.1.67
just bluos-doctor
just bluos-status
just bluos-syncstatus
just bluos-volume
```

Guarded mutation harness:

```bash
just bluos-step-down 192.168.1.67 1 -24
just bluos-step-up 192.168.1.67 1 -24
just bluos-mute-off 192.168.1.67
just bluos-mute-on 192.168.1.67
just bluos-sources 192.168.1.67
just bluos-source-optical 192.168.1.67
just bluos-source-spotify 192.168.1.67
```

Important safety notes:

- There are no unbounded volume commands.
- Mutating commands require explicit acknowledgement flags inside the scripts.
- Source switching sets `abs_db=-40` after switching.
- Spotify is a service/play context, not a passive `inputTypeIndex` source.
- Optical 1 is, delightfully, `inputTypeIndex=optical-2`.

## Checks

```bash
just test
just lint
just smoke
just doctor-hid
```

If `just` is unavailable, run the commands from `justfile` directly.

## Design principles

1. **Capture before control**: learn real device reports before mapping actions.
2. **Fakes, not mocks**: tests use behavior-oriented fakes and fixtures.
3. **Safety before spectacle**: every amplifier mutation is bounded and explicit.
4. **Transport-neutral events**: knob events stay useful for future desktop,
   daemon, and ESP32 paths.
5. **Harness is product**: every setup, probe, and smoke check gets a named
   command so future agents and humans can repeat it.

## Roadmap

- Promote redacted real Anticater gesture fixtures.
- Turn the harness learnings into a daemon loop.
- Add policy-backed knob-to-BluOS control with config for step dB, max dB,
  source shortcuts, and group behavior.
- Package the daemon for local startup once the behavior is boringly safe.
