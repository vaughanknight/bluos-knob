# Backpressure Coverage — Anticater VK-01 Bluetooth Input Baseline

**Spec**: [bluetooth-input-baseline-spec.md](./bluetooth-input-baseline-spec.md)  
**Generated**: 2026-06-05  
**Certainty**: Weak

> Advisory only — informs `plan-3`. Never blocks, never gates, no scores. (See
> plan-2d-backpressure-survey.)

## Existing Sensors (inventory)

| Sensor | Command | Dimension |
|---|---|---|
| (none found) | — | — |

No deterministic build, test, lint, typecheck, smoke, CI, schema, architecture,
or engineering-harness sensors exist yet.

## Coverage Matrix

| Criterion / failure mode | Deterministic sensor | Status | Tier |
|---|---|---|---|
| AC1: CLI lists candidate HID interfaces with useful metadata | HID enumeration fake test + hardware smoke capture script | BUILDABLE | computational |
| AC2: CLI opens a selected interface and prints timestamped raw reports | fake HID reader integration test + hardware smoke capture script | BUILDABLE | computational |
| AC3: real Anticater operations produce distinguishable evidence or explicit indistinguishable markers | Anticater hardware smoke checklist and captured fixture validator | BUILDABLE | computational |
| AC4: raw reports are saved as replayable JSONL fixtures | JSONL schema validator + fixture replay test | BUILDABLE | computational |
| AC5: replay emits the same normalized events as capture | fixture replay golden tests using fake HID reader | BUILDABLE | computational |
| AC6: obvious consumer-control reports map to normalized events | parser/normalizer unit tests with captured and synthetic fixtures | BUILDABLE | computational |
| AC7: vendor-defined reports are reported as capabilities, not used for LED/RGB control | descriptor parser/capability test with fixture descriptor examples | BUILDABLE | computational |
| AC8: no compatible device / failed open exits clearly and does not crash | CLI error-path tests with fake platform adapter | BUILDABLE | computational |
| AC9: tests run without hardware using fixture-backed fake HID readers | test suite command once project substrate exists | BUILDABLE | computational |
| AC10: README and docs/how explain workflow and limitations | documentation existence/content check | BUILDABLE | computational |
| Failure mode: Python project lacks a runnable CLI entry point | boot/smoke command such as `python -m bluos_knob --help` | BUILDABLE | computational |
| Failure mode: HIDAPI/native dependency missing on developer machine | dependency/install check documented in setup and exercised in smoke | BUILDABLE | computational |
| Failure mode: device-input imports CLI runtime or future amplifier-control internals | dependency-direction check or import boundary test | BUILDABLE | computational |
| Failure mode: hardware capture evidence is misunderstood by the agent | human review of smoke notes and captured real-device fixture | ABSENT | human-judgement |
| Failure mode: live LED/RGB control is desirable but not proven possible | descriptor evidence review; reverse-engineering decision remains human | ABSENT | human-judgement |

## Certainty: Weak

No deterministic sensors exist today, so behaviour and architecture coverage is
currently unproven even though the required sensors are mostly buildable within
the plan.

## Recommended Phase 0: Establish Backpressure

| Sensor to build | Proves | Suggested form |
|---|---|---|
| Minimal Python project harness | The repo has install/test/run commands before feature code | build/test/smoke substrate |
| CLI boot smoke | The CLI can start and print help without hardware | smoke |
| Unit test command | Parser, normalizer, config, and fixture replay logic can be proven without hardware | test |
| JSONL fixture schema check | Captured reports remain replayable and structurally valid | schema |
| Fake HID reader contract tests | AC4, AC5, AC6, AC8, AC9 without physical hardware | fake-backed integration tests |
| Hardware smoke script/checklist | Real Anticater capture evidence is recorded consistently | smoke |
| Dependency boundary check | device-input, cli-runtime, configuration, and platform-adapter do not collapse into each other | architecture/dependency rule |
| Documentation check | README and docs/how contain pairing/capture/replay/LED limitation guidance | data-script |

## Note on computational vs inferential tiers

This survey covers the computational tier before architecture. `plan-3` should
turn the BUILDABLE rows into planned tasks where useful. `plan-7` remains the
inferential review tier and should still review design quality, captured evidence,
and any human judgement calls around Anticater LED/RGB feasibility.
