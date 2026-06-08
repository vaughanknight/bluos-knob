# platform-adapter Domain

## Purpose

Isolate OS/HIDAPI behavior from CLI runtime and device-input contracts.

## Concepts

| Concept | Entry Points | Summary |
|---|---|---|
| Enumerating HID devices | `HidapiPlatform.enumerate_devices` | Converts HIDAPI metadata into `HidDeviceInfo`. |
| Opening HID reports | `HidapiPlatform.open` | Opens a selected HID path and returns timestamped `RawHidReport` objects. |
| Lazy HID dependency | `PlatformHidError` | Keeps help, replay, and tests runnable when HIDAPI is not installed. |

```python
from bluos_knob.platform_adapter.hidapi_reader import HidapiPlatform

devices = HidapiPlatform().enumerate_devices()
```

## Contracts

| Contract | File | Notes |
|---|---|---|
| HIDAPI adapter | `src/bluos_knob/platform_adapter/hidapi_reader.py` | Real hardware adapter; imports HIDAPI lazily. |

## Boundary Owns

- HIDAPI import/use.
- Device enumeration from the local OS.
- Opening paths and reading raw HID reports.

## Boundary Excludes

- CLI output decisions.
- Normalized event semantics.
- Configuration defaults.

## Dependencies

This domain depends on `device-input` contracts and optional HIDAPI runtime.

## Composition

| Component | Kind | Source |
|---|---|---|
| HIDAPI platform | Adapter | `src/bluos_knob/platform_adapter/hidapi_reader.py` |

## Source Location

- `src/bluos_knob/platform_adapter/`
- `docs/how/hid-capabilities.md`

## History

| Plan | Change | Date |
|---|---|---|
| 001-bluetooth-input-baseline | Created lazy HIDAPI adapter boundary. | 2026-06-05 |

