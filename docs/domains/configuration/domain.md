# configuration Domain

## Purpose

Hold minimal Anticater selection/profile behavior without hardcoding a local user
device path or private identifier.

## Concepts

| Concept | Entry Points | Summary |
|---|---|---|
| Filtering candidates | `AnticaterProfile.matches` | Matches optional product, manufacturer, VID/PID, usage, and usage page fields. |
| Default Anticater profile | `default_anticater_profile` | Broad default that prefers Anticater/VK product names without requiring local IDs. |

```python
from bluos_knob.configuration.profiles import default_anticater_profile

matches = [device for device in devices if default_anticater_profile().matches(device)]
```

## Contracts

| Contract | File | Notes |
|---|---|---|
| Anticater profile | `src/bluos_knob/configuration/profiles.py` | Device filtering without local path or serial coupling. |

## Boundary Owns

- Candidate profile fields.
- Selection defaults.

## Boundary Excludes

- Secrets.
- Amplifier target configuration.
- HID read behavior.

## Dependencies

This domain depends on `device-input` contracts.

## Composition

| Component | Kind | Source |
|---|---|---|
| Profiles | Pure configuration helpers | `src/bluos_knob/configuration/profiles.py` |

## Source Location

- `src/bluos_knob/configuration/`
- `tests/unit/test_profiles.py`

## History

| Plan | Change | Date |
|---|---|---|
| 001-bluetooth-input-baseline | Created configurable Anticater candidate profile. | 2026-06-05 |

