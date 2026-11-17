# BluOS Knob

```text
 ____  _    _   _  ___  ____    _  ___   _  ___  ____
| __ )| |  | | | |/ _ \/ ___|  | |/ / \ | |/ _ \| __ )
|  _ \| |  | | | | | | \___ \  | ' /|  \| | | | |  _ \
| |_) | |__| |_| | |_| |___) | | . \| |\  | |_| | |_) |
|____/|_____\___/ \___/|____/  |_|\_\_| \_|\___/|____/
```

![Safety](https://img.shields.io/badge/safety-zero%20blown%20speakers-brightgreen)
![BluOS](https://img.shields.io/badge/BluOS-NAD%20M33-blue)
![Knob](https://img.shields.io/badge/knob-Anticater%20VK--01-purple)

**Tiny knob. Big stereo energy. Zero blown speakers.**

![Anticater VK-01 wireless knob](docs/assets/vk-01-wireless-knob.png)

BluOS Knob is a local, safety-first volume knob for the **NAD M33** and other
**BluOS** players. It takes a tiny Anticater VK-01 Bluetooth knob and teaches it
some hi-fi manners: volume nudges, mute, source shortcuts, replayable hardware
captures, and guardrails that keep the party from turning into an accidental
speaker stress test.

Okay, technically it is "some script that sends volume commands" — but it is
some script that has had some thought put into it. It learns the real HID
reports from the knob, replays them deterministically, talks to BluOS over its
local API, and only changes the amplifier through harnessed commands with
explicit safety limits.

## Why this exists

I built this for my NAD M33 because I wanted a wireless, rechargeable volume
knob. The VK-01 was the perfect controller: rechargeable, wireless, a proper
dial, tap, and hold controls.

But it needed a bridge to talk to the NAD M33, so _this_ project brings it all
together.

There's now an ESP32 version too, so the Mac doesn't need to run the daemon:
a cheap M5StickC Plus, powered from USB, sits quietly near the amp doing the
bridge work and shows the volume on its little screen (see
[Run it on an ESP32](#run-it-on-an-esp32)).

Theoretically this should work with any BluOS device, but bear in mind I've only
tested it with the NAD M33. Enjoy!

## Just run it

For the "I have the knob, I have a BluOS player, let me try it" path:

```bash
git clone https://github.com/vaughanknight/bluos-knob.git
cd bluos-knob

./run.sh <your-bluos-ip-address>
```

Then turn the knob.

The script installs prerequisites, verifies HID/BluOS connectivity, starts the
daemon, and prints the log/stop commands. The daemon auto-discovers the current
Anticater HID path, so you normally do not need to copy a `DevSrvsID` manually.
It writes activity to:

```bash
tail -f /tmp/bluos-knob-daemon.log
```

To stop it:

```bash
just daemon-stop
```

Prefer manual steps? The harness commands below are the long way around.

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
| Daemon loop | Runs the knob-to-BluOS loop so the VK-01 controls the NAD directly. |
| ESP32 bridge | The same job on an M5StickC Plus, with a status screen. See [Run it on an ESP32](#run-it-on-an-esp32). |

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

## Development smoke test

```bash
python3 -m pip install -e ".[dev]"
python3 -m bluos_knob.cli.main --help
python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
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
python3 -m bluos_knob.cli.main capture \
  --path "<path from just list-devices>" \
  --output capture.local.jsonl \
  --operation-label live-probe
```

The `DevSrvsID` path can change after sleep or reconnect. Re-run
`just list-devices` when in doubt.

`*.local.jsonl` captures can contain local HID paths or serials. Review and
redact local identity fields before sharing or committing fixtures.

## BluOS / NAD harness

Set the NAD/BluOS host explicitly; local IPs are not hardcoded into the app.

```bash
export BLUOS_HOST=<your-bluos-ip-address>
just bluos-doctor
just bluos-status
just bluos-syncstatus
just bluos-volume
```

Guarded mutation harness:

```bash
just bluos-step-down <your-bluos-ip-address> 1 -24
just bluos-step-up <your-bluos-ip-address> 1 -24
just bluos-mute-off <your-bluos-ip-address>
just bluos-mute-on <your-bluos-ip-address>
just bluos-sources <your-bluos-ip-address>
just bluos-source-optical <your-bluos-ip-address>
just bluos-source-spotify <your-bluos-ip-address>
```

Important safety notes:

- There are no unbounded volume commands.
- Mutating commands require explicit acknowledgement flags inside the scripts.
- Source switching sets `abs_db=-40` after switching.
- Spotify is a service/play context, not a passive `inputTypeIndex` source.
- Optical 1 is, delightfully, `inputTypeIndex=optical-2`.

## Daemon

Run the live knob-to-BluOS loop:

```bash
just daemon-dry-run
just daemon
just daemon-start
```

The daemon maps knob events to the same guarded BluOS commands:

| Knob event | Daemon action |
|---|---|
| `volume_up` | `+1 dB`, refused above the configured max dB. |
| `volume_down` | `-1 dB`. |
| `mute_toggle` | Query current mute, then send explicit mute on/off. |
| `brightness_down` | Switch Optical 1 and set `abs_db=-40`. |
| `brightness_up` | Resume Spotify and set `abs_db=-40`. |

The daemon auto-discovers the current Anticater HID path, which can change after
sleep/reconnect, and logs what it does:

```bash
just daemon-log
```

## Checks

```bash
just test
just lint
just smoke
just doctor-hid
```

If `just` is unavailable, run the commands from `justfile` directly.

## Run it on an ESP32

No Mac needed: an M5StickC Plus (ESP32) talks to the knob over BLE and to the
amp over Wi-Fi (**2.4 GHz only**), and shows the volume, source and any fault on
its screen. It starts in DRY-RUN; `just fw-live` switches it live (remembered
until `just fw-dry-run`).

```bash
brew install platformio
just fw-test && just fw-flash   # flash over USB
just fw-wifi                    # Wi-Fi name, hidden password, amp IP
```

Pairing, going live, the serial console, the safety rules and the cut-over and
rollback from the Mac daemon are in [docs/how/esp32-bridge.md](docs/how/esp32-bridge.md).

## Roadmap

- Promote redacted real Anticater gesture fixtures.
- Add richer config for step dB, max dB, source shortcuts, and group behavior.
- Package the daemon for local startup.
