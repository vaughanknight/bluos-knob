# Anticater capture workflow

## Pair or connect

Pair the Anticater VK-01 through the operating system first. This CLI does not
perform Bluetooth pairing.

## List candidates

```bash
just list-devices
```

If HIDAPI is unavailable, install the optional dependency and native library:

```bash
just setup-hid
just doctor-hid
```

## Capture reports

For a full guided gesture pass, run:

```bash
just capture-gestures
```

Use a path printed by `list`:

```bash
python3 -m bluos_knob.cli.main capture --path "<device-path>" --output capture.local.jsonl --limit 20
```

Capture the operations separately or label a capture session:

- rotate right / volume up
- rotate left / volume down
- click / mute
- long-press + rotate right / brightness up
- long-press + rotate left / brightness down

If two operations produce indistinguishable reports, record that in
`tests/fixtures/anticater/README.md` or local capture notes.

## Replay without hardware

```bash
python3 -m bluos_knob.cli.main replay --fixture capture.local.jsonl
```

Replay emits the same normalized event shape used during live capture.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | OK |
| 10 | No matching device |
| 11 | Open failed |
| 12 | Bad output path |
| 13 | Capture interrupted |
| 14 | Invalid fixture |
| 15 | HIDAPI unavailable or enumeration failed |

## Privacy

Fixture rows may include local HID paths and serial numbers. Redact
`device.path` and `device.serial_number` before sharing.
