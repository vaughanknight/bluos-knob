# Code Review: Simple Mode

**Plan**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-plan.md`
**Spec**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-spec.md`
**Phase**: Simple Mode
**Date**: 2026-06-05
**Reviewer**: Automated (plan-7-v2)
**Testing Approach**: Hybrid

## A) Verdict

**APPROVE WITH NOTES**

All HIGH findings from the first review were fixed. The software baseline is
approved for merge analysis. The only remaining note is intentional: real
Anticater hardware smoke evidence is still pending physical device capture and
must not be represented as automated evidence.

## B) Summary

The phase establishes the intended Python CLI substrate, domain boundaries,
fixture-backed fakes, shared capture/replay normalization, HIDAPI adapter
boundary, and user documentation. The first review caught a raw HID evidence
fidelity bug and a harness command issue; both were fixed and covered by
regression checks. Domain compliance, reinvention, doctrine, lint, tests,
boundary checks, smoke, and whitespace checks are clean. Real-device operation
evidence remains a documented human/device follow-up.

## C) Checklist

**Testing Approach: Hybrid**

- [x] Deterministic tests present for fixture schema, fake HID reader, HIDAPI reader, normalizer, profiles, CLI errors, list, and replay.
- [x] Fakes used instead of mocks for external boundaries.
- [x] Hardware smoke step documented separately from automated checks.
- [x] Only in-scope implementation/domain/doc files changed.
- [x] Linters and boundary checks pass.
- [x] Domain compliance checks pass.
- [x] Review-blocking HID capture bug fixed.
- [x] Harness health check uses the available Python executable.

## D) Findings Table

| ID | Severity | File:Lines | Category | Summary | Recommendation |
|----|----------|------------|----------|---------|----------------|
| N001 | LOW | `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/README.md` | evidence | Real Anticater operation/capability evidence is pending physical-device exercise. | Keep this as an explicit smoke follow-up; do not mark it complete until real capture is recorded. |

## E) Detailed Findings

### E.1) Implementation Quality

✅ No blocking implementation findings remain.

Previously fixed:

- `HidapiReportReader.read_report()` now preserves the full byte stream returned
  by HIDAPI as raw evidence and uses `report_id=0` until descriptor evidence
  justifies splitting report IDs.
- Fixture schema parsing now raises `FixtureSchemaError` for missing/malformed
  required fields, including `transport` and `connection_state`.
- `justfile` recipes and engineering harness commands use `python3`.

### E.2) Domain Compliance

| Check | Status | Details |
|-------|--------|---------|
| File placement | ✅ | New files are under planned source/docs/test locations. |
| Contract-only imports | ✅ | Device contracts are imported through public modules; no HIDAPI import leaks into device-input. |
| Dependency direction | ✅ | Platform adapter does not depend on CLI/config; device-input is standard-library only. |
| Domain.md updated | ✅ | Domain docs include concepts, contracts, composition, dependencies, source locations, and history. |
| Registry current | ✅ | Registry lists all six domains and active/deferred status. |
| No orphan files | ✅ | Implementation files map to the Domain Manifest; plan/review artifacts live under the active plan tree. |
| Map nodes current | ✅ | Domain map contains active and deferred domains. |
| Map edges current | ✅ | Dependency edges are labeled. |
| No circular business deps | ✅ | No circular business dependencies found. |
| Concepts documented | ✅ | Contract-owning domains include concepts tables and examples. |

### E.3) Anti-Reinvention

| New Component | Existing Match? | Domain | Status |
|--------------|----------------|--------|--------|
| HID contracts | None | None | Proceed |
| Fixture schema | None | None | Proceed |
| Fake HID platform/reader | None | None | Proceed |
| Normalizer | None | None | Proceed |
| Anticater profile filters | None | None | Proceed |
| HIDAPI platform adapter | None | None | Proceed |
| CLI list/capture/replay | None | None | Proceed |
| Boundary checker | None | None | Proceed |

### E.4) Testing & Evidence

**Coverage confidence**: 91%

| AC | Confidence | Evidence |
|----|------------|----------|
| AC1 | 88% | `tests/integration/test_cli_list.py` verifies fake-backed listing metadata. |
| AC2 | 90% | Fake-backed capture path and `tests/unit/test_hidapi_reader.py` verify raw-byte preservation. |
| AC3 | 63% | Real operation evidence is explicitly pending in execution log and fixture README. |
| AC4 | 92% | Fixture roundtrip and replay behavior covered by schema and CLI replay tests. |
| AC5 | 90% | `test_cli_replay.py` asserts replayed events equal captured normalized events. |
| AC6 | 89% | `tests/unit/test_normalizer.py` covers volume, mute, brightness, release, and unknown reports. |
| AC7 | 74% | Capability reporting exists structurally; real vendor-defined evidence remains pending hardware. |
| AC8 | 95% | `tests/integration/test_cli_errors.py` covers no-device, open failure, invalid output, and interrupt paths. |
| AC9 | 97% | Unit/integration suite uses fakes and fixtures only. |
| AC10 | 93% | README and `docs/how/` cover pairing, capture, replay, fixture schema, redaction, and LED/RGB limitations. |

### E.5) Doctrine Compliance

✅ No doctrine violations found. The implementation preserves fakes-only testing,
adapter boundaries, no amplifier side effects, explicit configuration
boundaries, and no LED/RGB control.

### E.6) Harness Live Validation

- Agent harness status: **HEALTHY**
- Boot: `python3 -m bluos_knob.cli.main --help` passed.
- Health: `just smoke` passed.
- Interact: fixture replay emitted six JSONL normalized events.
- Observe: `python3 -m pytest`, `python3 -m ruff check .`,
  `python3 scripts/check_boundaries.py`, and `git diff --check` passed.
- Hardware: real VK-01 smoke skipped because no physical device capture was
  performed in this automated pass.

## F) Coverage Map

| AC | Description | Evidence | Confidence |
|----|-------------|----------|------------|
| AC1 | CLI lists candidate HID interfaces | Fake-backed list integration test | 88% |
| AC2 | CLI opens selected interface and prints raw reports | Fake-backed capture/replay plus HIDAPI raw-byte regression test | 90% |
| AC3 | Real operation evidence or indistinguishable marking | Pending physical smoke template; no fabricated evidence | 63% |
| AC4 | JSONL fixtures replay without hardware | Sample fixture + replay command | 92% |
| AC5 | Replay emits same normalized events as capture | Shared normalizer and equality assertion | 90% |
| AC6 | Consumer-control reports map to normalized events | Normalizer unit tests | 89% |
| AC7 | Vendor-defined capabilities reported, no LED/RGB control | Adapter capability shape and docs; hardware evidence pending | 74% |
| AC8 | No-device/open-failure diagnostics | CLI error integration tests | 95% |
| AC9 | Fake-backed parser/replay/normalizer tests | 23 passing tests; no mocks | 97% |
| AC10 | Docs explain pairing/capture/replay/limitations | README and `docs/how/` | 93% |

**Overall coverage confidence**: 91%

## G) Commands Executed

```bash
git diff --stat
git diff --staged --stat
git diff --no-index -- /dev/null <untracked-file>
python3 -m pytest
python3 -m ruff check .
python3 scripts/check_boundaries.py
python3 -m bluos_knob.cli.main --help
python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
just smoke
git diff --check
```

## H) Handover Brief

> Copy this section to the implementing agent. It has no context on the review —
> only context on the work that was done before the review.

**Review result**: APPROVE WITH NOTES

**Plan**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-plan.md`
**Spec**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-spec.md`
**Phase**: Simple Mode
**Tasks dossier**: inline in plan
**Execution log**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/execution.log.md`
**Review file**: `/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/reviews/review.md`

### Files Reviewed

| File (absolute path) | Status | Domain | Action Needed |
|---------------------|--------|--------|---------------|
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/hidapi_reader.py` | Approved | platform-adapter | None |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fixture_schema.py` | Approved | device-input | None |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fake_hid.py` | Approved | device-input | None |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/normalizer.py` | Approved | device-input | None |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/cli/main.py` | Approved | cli-runtime | None |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/configuration/profiles.py` | Approved | configuration | None |
| `/Users/vaughanknight/GitHub/bluos-knob/justfile` | Approved | cli-runtime | None |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/engineering-harness.md` | Approved | platform-adapter | None |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/README.md` | Approved with note | device-input | Add real smoke evidence after physical capture. |

### Required Fixes

None.

### Domain Artifacts to Update

None.

### Next Step

Implementation review is approved. Continue to merge analysis:

```text
/plan-8-v2-merge --plan "/Users/vaughanknight/GitHub/bluos-knob/docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-plan.md"
```

