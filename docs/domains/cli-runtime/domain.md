# cli-runtime Domain

## Purpose

Provide user-facing commands for listing HID candidates, capturing reports, and
replaying fixtures.

## Concepts

| Concept | Entry Points | Summary |
|---|---|---|
| Listing candidates | `main(["list"])` | Prints candidate HID metadata as JSON lines. |
| Capturing reports | `main(["capture", ...])` | Opens a selected device, writes JSONL fixtures, and prints normalized event output. |
| Replaying fixtures | `main(["replay", ...])` | Reads fixture rows and emits normalized events without hardware. |

```python
from bluos_knob.cli.main import main

exit_code = main(["replay", "--fixture", "tests/fixtures/anticater/sample_reports.jsonl"])
```

## Contracts

| Contract | File | Notes |
|---|---|---|
| CLI entry point | `src/bluos_knob/cli/main.py` | `bluos-knob` and `python -m bluos_knob.cli.main`. |
| Exit codes | `src/bluos_knob/cli/main.py` | Stable diagnostics for no-device, open failure, bad output, interrupted capture, invalid fixture, unavailable HID. |

## Boundary Owns

- Argument parsing.
- Terminal output.
- Exit codes and user diagnostics.
- Dependency injection seam for fake platform adapters in tests.

## Boundary Excludes

- HIDAPI import and OS-specific behavior.
- Fixture schema internals beyond calling public helpers.
- Amplifier or volume policy behavior.

## Dependencies

This domain depends on `device-input`, `configuration`, and `platform-adapter`.

## Composition

| Component | Kind | Source |
|---|---|---|
| CLI main | Runtime | `src/bluos_knob/cli/main.py` |
| CLI tests | Integration | `tests/integration/` |

## Source Location

- `src/bluos_knob/cli/`
- `tests/integration/`
- `README.md`
- `docs/how/anticater-capture.md`

## History

| Plan | Change | Date |
|---|---|---|
| 001-bluetooth-input-baseline | Created list, capture, and replay CLI baseline. | 2026-06-05 |

