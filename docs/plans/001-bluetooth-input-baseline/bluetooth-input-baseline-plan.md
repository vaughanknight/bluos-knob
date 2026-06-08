# Anticater VK-01 Bluetooth Input Baseline Implementation Plan

**Mode**: Simple  
**Plan Version**: 1.0.0  
**Created**: 2026-06-05  
**Spec**: [bluetooth-input-baseline-spec.md](./bluetooth-input-baseline-spec.md)  
**Status**: READY

## Gate Matrix

| Gate | Check | Status | Notes |
|---|---|---|---|
| G1 | Clarify | PASS | Spec has no blocking clarification markers and Round 2 resolved domains + harness readiness. |
| G2 | Constitution | PASS | Plan preserves safety-first capture-before-control, adapter boundaries, fakes-only tests, and no time estimates. |
| G3 | Architecture | PASS | Planned files keep HID/platform access outside core event contracts and exclude BluOS control from this baseline. |
| G4 | ADR Compliance | N/A | No accepted ADRs exist. |
| G5 | Structure | PASS | Required Simple-mode sections and task table are present. |
| G6 | Testing Alignment | PASS | Hybrid strategy is reflected by fake-backed tests and schema checks before implementation tasks, plus hardware smoke as evidence. |
| G7 | Domain Completeness | PASS | Every spec domain appears below; new domains have setup tasks and planned domain docs/dirs. |

## Summary

This plan creates a minimal Python/HIDAPI CLI baseline for the Anticater VK-01 volume knob. It starts with a Phase 0-style harness/backpressure substrate because the repository currently has no runnable code, test command, boot command, or engineering harness. The implementation keeps raw HID capture, normalized event contracts, fixture replay, CLI UX, configuration, and platform-specific HID concerns separated so future ESP32 and BluOS dB volume policy work can reuse the evidence without inheriting desktop-specific assumptions.

## Target Domains

| Domain | Status | Relationship | Role |
|---|---|---|---|
| device-input | NEW | create | Own Anticater HID descriptors, raw reports, fixture schema, fake HID reader, and normalized input events. |
| cli-runtime | NEW | create | Own command-line listing, selection, capture, replay, diagnostics, and exit codes. |
| configuration | NEW | create | Own profiles and non-secret selection defaults without hardcoding user-specific identifiers. |
| platform-adapter | NEW | create | Own Python/HIDAPI access and future OS-specific HID differences. |
| volume-policy | NEW | consume later | Document future pure policy contract; no implementation in this baseline. |
| amplifier-control | NEW | consume later | Document future BluOS/NAD adapter contract; no implementation in this baseline. |

## Domain Manifest

| File | Domain | Classification | Rationale |
|---|---|---|---|
| `/Users/vaughanknight/GitHub/bluos-knob/.gitignore` | configuration | contract | Keeps generated Python/test/local capture artifacts out of source control. |
| `/Users/vaughanknight/GitHub/bluos-knob/pyproject.toml` | cli-runtime | contract | Defines package, CLI entry point, test/lint dependencies, and runnable substrate. |
| `/Users/vaughanknight/GitHub/bluos-knob/justfile` | cli-runtime | contract | Provides canonical install/test/lint/smoke commands for agents and humans. |
| `/Users/vaughanknight/GitHub/bluos-knob/README.md` | cli-runtime | contract | Quick-start commands for pairing, listing, capture, and replay. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/engineering-harness.md` | platform-adapter | contract | Documents Boot → Interact → Observe for the Python CLI substrate. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/registry.md` | configuration | contract | Authoritative list of project domains. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/domain-map.md` | configuration | contract | Documents contract edges between new domains. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/device-input/domain.md` | device-input | contract | Defines raw report, fixture, fake HID reader, and normalized event concepts. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/cli-runtime/domain.md` | cli-runtime | contract | Defines CLI command and terminal-output contracts. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/configuration/domain.md` | configuration | contract | Defines profile and capture option concepts. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/platform-adapter/domain.md` | platform-adapter | contract | Defines platform HID reader boundaries. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/volume-policy/domain.md` | volume-policy | contract | Future-domain placeholder documenting consumed normalized-event contract. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/amplifier-control/domain.md` | amplifier-control | contract | Future-domain placeholder documenting eventual BluOS dB contract. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/__init__.py` | device-input | internal | Package boundary for device input code. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/contracts.py` | device-input | contract | Public data contracts for raw reports, fixtures, capabilities, and normalized events. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fixture_schema.py` | device-input | internal | JSONL fixture validation and serialization helpers. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fake_hid.py` | device-input | internal | Fixture-backed fake HID reader for deterministic tests. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/normalizer.py` | device-input | internal | Best-effort mapping from observed reports to normalized events. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/__init__.py` | platform-adapter | internal | Package boundary for platform-specific adapters. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/hidapi_reader.py` | platform-adapter | internal | Real HIDAPI enumeration/open/read implementation. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/configuration/__init__.py` | configuration | internal | Package boundary for configuration code. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/configuration/profiles.py` | configuration | internal | Anticater profile and selection/filtering options. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/cli/__init__.py` | cli-runtime | internal | Package boundary for CLI runtime. |
| `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/cli/main.py` | cli-runtime | internal | CLI commands for list, capture, and replay. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_fixture_schema.py` | device-input | internal | Proves fixture schema accepts/rejects expected JSONL rows. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_fake_hid_reader.py` | device-input | internal | Proves fake HID reader replays fixtures deterministically. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_normalizer.py` | device-input | internal | Proves best-effort normalized events for known report examples. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_profiles.py` | configuration | internal | Proves profile/filter behavior does not hardcode one user device. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/integration/test_cli_replay.py` | cli-runtime | internal | Proves replay command emits deterministic output without hardware. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/integration/test_cli_errors.py` | cli-runtime | internal | Proves no-device/open-failure diagnostics and exit codes. |
| `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/sample_reports.jsonl` | device-input | internal | Synthetic fixture until real Anticater captures are added. |
| `/Users/vaughanknight/GitHub/bluos-knob/scripts/check_boundaries.py` | configuration | cross-domain | Lightweight import-boundary check covering planned domain separation. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/how/anticater-capture.md` | cli-runtime | contract | Detailed pairing/list/capture/replay and smoke evidence guidance. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/how/fixture-format.md` | device-input | contract | Documents JSONL fixture schema and replay expectations. |
| `/Users/vaughanknight/GitHub/bluos-knob/docs/how/hid-capabilities.md` | platform-adapter | contract | Documents descriptor/capability capture and LED/RGB limitation handling. |

## Key Findings

| # | Impact | Finding | Action |
|---|---|---|---|
| 01 | Critical | No implementation substrate exists: no source files, manifest, tests, smoke command, or engineering harness. | Start with harness/backpressure tasks before any real HID capture. |
| 02 | Critical | Fakes-only policy is mandatory and supersedes mock-based boundary tests. | Build fixture-backed fake HID reader and platform fakes before CLI implementation. |
| 03 | High | Anticater VK-01 likely emits HID consumer/keyboard events, but descriptors/report IDs must be captured rather than assumed. | Add descriptor/capability capture and keep unknown/vendor-defined reports explicit. |
| 04 | High | HID access may be filtered or unavailable depending on macOS/PC permissions and OS behavior. | Keep OS pairing as prerequisite, provide clear diagnostics, and separate hardware smoke from automated tests. |
| 05 | High | JSONL fixture schema can make or break replay fidelity and future ESP32/BluOS reuse. | Define timestamp, session/device identity, report ID, raw bytes, sequence/counter, transport/mode, and connection fields early. |
| 06 | High | Future ESP32 and BluOS work need transport-neutral events and dB-oriented policy data, not HID-specific or percentage semantics. | Keep normalized events as deltas/actions with correlation metadata; do not implement amplifier policy here. |

## Implementation

**Objective**: Establish a runnable, fake-backed Python CLI baseline that captures Anticater VK-01 HID evidence and replays fixtures deterministically.

**Testing Approach**: Hybrid — TDD-style deterministic tests for contracts, fixture schema, fake HID reader, replay, normalizer, profiles, and CLI error paths; hardware smoke checks only for real Anticater capture evidence.

### Tasks

| Status | ID | Task | Domain | Path(s) | Done When | Notes |
|---|---|---|---|---|---|---|
| [x] | T001 | Create domain registry, domain map, and domain docs | configuration | `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/registry.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/domain-map.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/device-input/domain.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/cli-runtime/domain.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/configuration/domain.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/platform-adapter/domain.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/volume-policy/domain.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/domains/amplifier-control/domain.md` | Registry lists all six domains, map shows contract edges, and each domain has Purpose, Concepts, Contracts, Boundary Owns, Boundary Excludes. | Satisfies G7 domain setup; future domains are documented as consumed-later, not implemented. |
| [x] | T002 | Establish minimal Python project and engineering harness | cli-runtime | `/Users/vaughanknight/GitHub/bluos-knob/.gitignore`, `/Users/vaughanknight/GitHub/bluos-knob/pyproject.toml`, `/Users/vaughanknight/GitHub/bluos-knob/justfile`, `/Users/vaughanknight/GitHub/bluos-knob/docs/project-rules/engineering-harness.md` | `just test`, `just lint`, `just smoke`, and `python -m bluos_knob.cli.main --help` are defined; engineering harness documents Boot → Interact → Observe at L2 target. | Implements Phase 0 backpressure substrate. |
| [x] | T003 | Create package boundaries for planned domains | platform-adapter | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/__init__.py`, `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/__init__.py`, `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/configuration/__init__.py`, `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/cli/__init__.py` | Imports resolve and package boundaries match the domain map without pulling platform HID code into device-input contracts. | Keep source layout domain-aligned from the first code files. |
| [x] | T004 | Define event, report, fixture, and capability contracts with schema tests first | device-input | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/contracts.py`, `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fixture_schema.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_fixture_schema.py`, `/Users/vaughanknight/GitHub/bluos-knob/docs/how/fixture-format.md` | Tests prove valid JSONL fixture rows round-trip, invalid rows fail clearly, and fixture docs mark device identifiers/paths as redactable before sharing. | Per finding 05; tests precede implementation. |
| [x] | T005 | Build fixture-backed fake HID reader and replay tests | device-input | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/fake_hid.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_fake_hid_reader.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/sample_reports.jsonl` | Fake HID reader replays reports deterministically from JSONL and never requires hardware. | Fakes only; no mocks. |
| [x] | T006 | Add normalizer tests and best-effort event mapping | device-input | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/device_input/normalizer.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_normalizer.py` | Known synthetic consumer-control report examples map to `volume_up`, `volume_down`, `mute_toggle`, `brightness_up`, `brightness_down`, or `unknown`. | Unknown reports remain observable, not discarded. |
| [x] | T007 | Add configuration profiles and selection tests | configuration | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/configuration/profiles.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/unit/test_profiles.py` | Anticater candidate profile can filter by optional VID/PID/product/usage fields without hardcoding a user-specific device path. | Keeps device identity configurable. |
| [x] | T008 | Implement real HIDAPI platform reader | platform-adapter | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/platform_adapter/hidapi_reader.py` | Reader can enumerate HID devices with metadata, open a selected path, read reports, and expose capability metadata without importing CLI runtime. | Hardware-dependent behavior stays behind adapter. |
| [x] | T009 | Implement CLI commands for list, capture, and replay | cli-runtime | `/Users/vaughanknight/GitHub/bluos-knob/src/bluos_knob/cli/main.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/integration/test_cli_replay.py`, `/Users/vaughanknight/GitHub/bluos-knob/tests/integration/test_cli_errors.py` | `list` prints candidate metadata; `capture` writes JSONL reports; `replay` emits normalized events from fixture input; no-device/open-failure paths exit clearly. | Real capture uses platform adapter; tests use fake reader/fixtures. |
| [x] | T010 | Add boundary and documentation checks | configuration | `/Users/vaughanknight/GitHub/bluos-knob/scripts/check_boundaries.py`, `/Users/vaughanknight/GitHub/bluos-knob/justfile` | `just lint` or a dedicated check verifies forbidden imports between device-input, cli-runtime, platform-adapter, configuration, and future domains. | Implements Phase 0 dependency-boundary sensor. |
| [x] | T011 | Document user workflow and Anticater limitations | cli-runtime | `/Users/vaughanknight/GitHub/bluos-knob/README.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/how/anticater-capture.md`, `/Users/vaughanknight/GitHub/bluos-knob/docs/how/hid-capabilities.md` | Docs explain OS pairing, list/capture/replay commands, fixture format, hardware smoke steps, fixture redaction guidance, and LED/RGB deferral. | Satisfies AC10 and sets expectations around LED/RGB and local-device privacy. |
| [x] | T012 | Capture and record real-device smoke evidence | device-input | `/Users/vaughanknight/GitHub/bluos-knob/tests/fixtures/anticater/README.md` | Smoke note records the evidence template and explicitly states no real Anticater hardware capture has been recorded yet. | Human/device step remains pending for real evidence; no automated hardware requirement and no evidence fabricated. |

### Acceptance Criteria

- [x] CLI lists candidate HID interfaces for the Anticater VK-01 with useful metadata.
- [x] CLI opens a selected interface and prints timestamped raw reports.
- [ ] Real-device evidence for rotate left/right, click/mute, and long-press+rotate actions is still pending physical Anticater smoke capture.
- [x] CLI saves JSONL fixtures that can be replayed without hardware.
- [x] Replayed fixtures emit the same best-effort normalized events as capture.
- [x] Obvious consumer-control reports map to volume, mute, brightness, no-op release, or unknown events.
- [x] Vendor-defined reports are reported as capabilities and not used for baseline LED/RGB control.
- [x] No-device and failed-open paths exit with clear diagnostics.
- [x] Fixture-backed fake HID readers prove parser, replay, and normalized event behavior without hardware.
- [x] README and `docs/how/` explain pairing, capture, replay, fixture format, and LED/RGB limitations.

### Implementation Evidence

| Check | Result | Evidence |
|---|---|---|
| Install | PASS | `python3 -m pip install -e ".[dev]"` |
| Tests | PASS | `python3 -m pytest` → 23 passed |
| Lint | PASS | `python3 -m ruff check .` |
| Boundaries | PASS | `python3 scripts/check_boundaries.py` |
| Smoke | PASS | CLI help and fixture replay produce six normalized events |
| Review fixes | PASS | `just smoke` and `git diff --check` pass after fixing HID byte preservation, `python3` recipes, and schema errors |
| HID setup/list | PASS | `just setup-hid`, `just doctor-hid`, and `just list-devices` run through the engineering harness; Anticater candidates found |

## Discoveries & Learnings

| ID | Kind | Discovery | Impact | Action |
|---|---|---|---|---|
| D001 | Design | CLI `list` and `capture` need an injectable platform-adapter fake, not only a fixture fake reader. | Keeps AC1/AC9 deterministic without hardware or mocks. | Added `HidPlatform` protocol seam and `FakeHidPlatform`. |
| D002 | Contract | Fixture and normalized event contracts need schema version, sequence, transport, connection, and raw correlation fields. | Preserves future ESP32/BluOS reuse and avoids overfitting to desktop HIDAPI. | Added fields to `CaptureFixtureRow`, `RawHidReport`, and `NormalizedKnobEvent`. |
| D003 | Safety | Real Anticater operation evidence cannot be recorded without physical knob interaction. | Prevents fabricated hardware evidence. | Added smoke-evidence template; live mixed capture was recorded on 2026-06-05. |
| D004 | Correctness | HIDAPI byte streams may be unnumbered, so dropping byte 0 can corrupt raw evidence. | Protects the capture baseline's core evidence fidelity. | Preserve full HID byte stream with `report_id=0` until descriptor evidence proves a split. |
| D005 | Harness | HIDAPI setup should be repeatable through project commands, not terminal lore. | Makes real-device listing reproducible for future agents and humans. | Added `just setup-hid`, `just doctor-hid`, and `just list-devices`; updated harness docs. |
| D006 | Capture | The real knob emits `030000` release/no-op reports between action reports. | Avoids treating releases as unknown actions. | Updated normalizer and tests to classify report-ID-prefixed zero payloads as `no_op`. |

### Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| macOS/PC HID stack filters or consumes reports | Medium | High | Keep OS pairing explicit; provide clear diagnostics and smoke evidence for actual behavior. |
| Anticater modes expose different report IDs/usages | Medium | High | Capture descriptor/report metadata per mode and keep fixtures mode-labeled. |
| LED/RGB control is not exposed | High | Medium | Treat LED/RGB as non-goal; report vendor-defined capabilities only if discovered. |
| Fixture schema under-captures future ESP32/BluOS needs | Medium | High | Include source/session/sequence/raw/capability metadata and document schema. |
| Python/HIDAPI dependency setup differs across platforms | Medium | Medium | Start macOS-first; document native dependency requirements and keep platform adapter isolated. |
| Captures leak local device identifiers, paths, or serials | Medium | Medium | Mark fixture identity fields as local/private by default and document redaction before sharing. |

## Agent Harness Strategy

- **Current Maturity**: L0 — no runnable engineering harness or agent harness exists.
- **Target Maturity**: L2 by completion of T002.
- **Boot Command**: `python -m bluos_knob.cli.main --help`
- **Health Check**: `just smoke`
- **Interaction Model**: Terminal CLI
- **Evidence Capture**: terminal output, test results, JSONL fixture files, and hardware smoke notes
- **Pre-Phase Validation**: Boot → Interact → Observe through the commands documented in `docs/project-rules/engineering-harness.md`

---

## Validation Record (2026-06-05)

### Validation Thesis

**Raison d'être**: The plan exists to turn a research-backed Anticater VK-01 Bluetooth input baseline spec into a concrete, implementation-ready sequence that starts with deterministic backpressure because the repo has no source/tooling substrate yet.

**Value claim**: Implementation should become safer, clearer, and more repeatable because future agents get explicit files, boundaries, fake-backed tests, smoke checks, and domain setup rather than guessing how to start.

**Artifact promise**: Downstream planning/implementation agents can rely on this plan for the file manifest, domain boundaries, task order, success criteria, and proof sensors needed to build the first Python/HIDAPI CLI baseline.

**Intended beneficiaries**: Implementation agents, reviewers, future maintainers, and the user who needs real Anticater capture evidence.

**Proof target**: Implementation

**Evidence standard**: Concrete task ordering, file paths, domain manifest, acceptance criteria coverage, fakes-only testing alignment, and backpressure task coverage.

**Thesis source**: `bluetooth-input-baseline-spec.md`, `research-dossier.md`, and `backpressure-coverage.md`.

**Thesis verdict**: Advanced

**Main thesis risk**: Future work could still drift if the fake-backed boundary checks and hardware smoke evidence are not treated as mandatory gates.

---

| Agent | Lenses Covered | Thesis Axes Covered | Issues | Verdict |
|---|---|---|---|---|
| Plan Coherence Validator | Thesis Alignment, Evidence Sufficiency, Proof-Level Fit, System Behavior, Hidden Assumptions, Edge Cases & Failures, Domain Boundaries, Deployment & Ops | Implementation Readiness, Agent Readiness, Cross-Domain Coordination | 0 | PASS |
| Risk & Completeness Validator | Evidence Sufficiency, Technical Constraints, Edge Cases & Failures, Deployment & Ops, Security & Privacy, User Experience | Safety to Change, Operational Reliability, Evidence Sufficiency | 1 MEDIUM fixed | PASS after fix |
| Thesis Alignment Validator | Thesis Alignment, Evidence Sufficiency, Proof-Level Fit, User/Product Value Preservation, Review Compression, Agent Readiness | Thesis Alignment, Implementation Readiness, Review Compression | 0 | PASS |
| Forward-Compatibility Validator | Forward-Compatibility, Integration & Ripple, Domain Boundaries, Contract Integrity, Test Boundary, Deployment & Ops | Downstream Usefulness, Contract Integrity, Cross-Domain Coordination | 0 | PASS |

### Forward-Compatibility Matrix

| Consumer | Requirement | Failure Mode | Verdict | Evidence |
|---|---|---|---|---|
| `plan-5-v2-phase-tasks-and-brief` | Concrete tasks, file paths, criteria, domain manifest | none | ✅ | Domain Manifest and Tasks table in this plan |
| `plan-6-v2-implement-phase` | Ordered work, validation commands, fakes-only boundaries | none | ✅ | T001-T012 and Agent Harness Strategy |
| `plan-7-v2-code-review` | Acceptance criteria, non-goals, risks, evidence standard | none | ✅ | Acceptance Criteria and Risks sections |
| Future ESP32/BluOS consumers | Transport-neutral event contract; no HID/LED coupling | none | ✅ | Spec and plan keep future domains consume-later only |

**Thesis alignment**: Value claim advanced = Yes; Proof level Target = Implementation and Actual = Implementation; Main thesis risk is that future work could drift if fake-backed boundary checks and hardware smoke evidence are not treated as mandatory gates.

**Outcome alignment**: The plan is forward-compatible for implementation, review, and future ESP32/BluOS reuse because it exposes the needed public contract while deferring private or future-domain decisions.

**Standalone?**: No — downstream consumers are `plan-5`, `plan-6`, `plan-7`, and future ESP32/BluOS work.

Overall: VALIDATED WITH FIXES
