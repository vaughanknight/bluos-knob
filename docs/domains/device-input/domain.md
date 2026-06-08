# device-input Domain

## Purpose

Capture physical Anticater input evidence and translate raw HID reports into
transport-neutral normalized events.

## Concepts

| Concept | Entry Points | Summary |
|---|---|---|
| Capturing raw HID reports | `RawHidReport`, `CaptureFixtureRow` | Preserves timestamp, report ID, raw bytes, sequence, transport, connection state, and local identity metadata. |
| Replaying fixtures with fakes | `FakeHidPlatform`, `FakeHidReportReader` | Provides deterministic report streams without physical hardware or mocks. |
| Normalizing knob input | `normalize_report` | Maps known HID consumer usages to volume, mute, brightness, no-op, or unknown events while preserving raw correlation. |

```python
from bluos_knob.device_input.fixture_schema import load_jsonl
from bluos_knob.device_input.normalizer import normalize_report

events = [normalize_report(row.raw_report) for row in load_jsonl("sample_reports.jsonl")]
```

## Contracts

| Contract | File | Notes |
|---|---|---|
| HID data contracts | `src/bluos_knob/device_input/contracts.py` | Public dataclasses/enums shared by CLI and platform adapter. |
| Fixture schema | `src/bluos_knob/device_input/fixture_schema.py` | JSONL serialization, validation, and redaction helpers. |
| Fake HID reader | `src/bluos_knob/device_input/fake_hid.py` | Behavior fake for deterministic boundary tests. |
| Normalizer | `src/bluos_knob/device_input/normalizer.py` | Shared live-capture and replay normalization path. |

## Boundary Owns

- Raw HID report shape.
- JSONL fixture schema.
- Fixture-backed HID fakes.
- Best-effort normalized knob event mapping.

## Boundary Excludes

- Terminal UX and exit codes.
- OS-specific HIDAPI behavior.
- NAD/BluOS volume policy or amplifier commands.
- LED/RGB control.

## Dependencies

This domain depends on Python standard library only.

Domains that depend on this: `cli-runtime`, `platform-adapter`, future
`volume-policy`.

## Composition

| Component | Kind | Source |
|---|---|---|
| Contracts | Dataclasses/enums | `src/bluos_knob/device_input/contracts.py` |
| Fixture schema | Parser/serializer | `src/bluos_knob/device_input/fixture_schema.py` |
| Fake HID | Test fake | `src/bluos_knob/device_input/fake_hid.py` |
| Normalizer | Pure function | `src/bluos_knob/device_input/normalizer.py` |

## Source Location

- `src/bluos_knob/device_input/`
- `tests/unit/test_fixture_schema.py`
- `tests/unit/test_fake_hid_reader.py`
- `tests/unit/test_normalizer.py`
- `tests/fixtures/anticater/`
- `docs/how/fixture-format.md`

## History

| Plan | Change | Date |
|---|---|---|
| 001-bluetooth-input-baseline | Created raw report, fixture, fake reader, and normalized event baseline. | 2026-06-05 |

