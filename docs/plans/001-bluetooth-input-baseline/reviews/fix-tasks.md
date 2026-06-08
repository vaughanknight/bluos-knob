# Fix Tasks: Simple Mode

Apply in order. Re-run review after fixes.

## Critical / High Fixes

### FT-001: Preserve raw HID bytes during real capture

- **Severity**: HIGH
- **File(s)**: `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/hidapi_reader.py`
- **Issue**: `read_report()` treats the first byte returned by HIDAPI as a report ID and drops it from the raw payload.
- **Fix**: Preserve the full byte sequence as `RawHidReport.data` and use `report_id=0` unless a future descriptor-backed contract proves a numbered report ID separately.
- **Patch hint**:

```diff
- report_id = int(data[0]) if data else 0
- payload = tuple(int(byte) for byte in data[1:])
+ report_id = 0
+ payload = tuple(int(byte) for byte in data)
```

### FT-002: Make harness recipes use the available Python executable

- **Severity**: HIGH
- **File(s)**: `/Users/vaughanknight/GitHub/bluos-knob/justfile`, `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/engineering-harness.md`
- **Issue**: Live harness validation showed `just smoke` fails when `python` is unavailable, while `python3` works.
- **Fix**: Use `python3` consistently in just recipes and harness commands.

## Medium / Low Fixes

### FT-003: Convert malformed fixture fields to FixtureSchemaError

- **Severity**: MEDIUM
- **File(s)**: `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fixture_schema.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_fixture_schema.py`
- **Issue**: Missing or malformed `timestamp`, `report_id`, `transport`, or `connection_state` can escape as `KeyError`/`ValueError`.
- **Fix**: Add required-field helpers and tests so replay diagnostics remain schema-specific.

### FT-004: Remove trailing whitespace from amended doctrine files

- **Severity**: LOW
- **File(s)**: `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/constitution.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/idioms.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/rules.md`
- **Issue**: `git diff --check` reports trailing whitespace.
- **Fix**: Remove trailing spaces.

### FT-005: Keep hardware smoke evidence explicitly pending

- **Severity**: LOW
- **File(s)**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-spec.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-plan.md`, `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/README.md`
- **Issue**: Real Anticater operation and vendor-defined capability evidence cannot be completed without the physical device.
- **Fix**: Keep the pending human/device smoke step explicit and never mark it as automated evidence.

## Re-Review Checklist

- [x] All critical/high fixes applied.
- [x] `python3 -m pytest` passes.
- [x] `python3 -m ruff check .` passes.
- [x] `python3 scripts/check_boundaries.py` passes.
- [x] `just smoke` passes.
- [x] `git diff --check` passes.
- [x] Re-run `/plan-7-v2-code-review` and achieve zero HIGH/CRITICAL findings.
