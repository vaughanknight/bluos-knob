<!--
Sync Impact Report
Mode: CREATE
Version: 1.0.0
Constitution: docs/project-rules/constitution.md
Outstanding TODOs: TODO(TOOLING), TODO(CI), TODO(MOCK_POLICY)
-->

# Project Rules

**Version**: 1.0.0  
**Ratified**: 2026-06-05  
**Last amended**: 2026-06-05

These rules implement the project constitution. "MUST" is mandatory, "SHOULD"
is expected unless a documented tradeoff is approved, and "MAY" is optional.

## Source Control and Change Hygiene

1. Changes MUST be scoped to a single coherent behavior or doctrine update.
2. Generated files, local configuration, credentials, and device-specific secrets
   MUST NOT be committed.
3. Documentation MUST change with behavior when configuration, daemon operation,
   hardware assumptions, or safety boundaries change.
4. Destructive operations against user state, local daemon state, or amplifier
   settings MUST require explicit review in the relevant plan.

## Coding Standards

1. Core volume policy MUST be separated from Bluetooth, BluOS, networking, and
   service-manager adapters.
2. Names SHOULD describe daemon behavior in domain terms: knob event, volume
   intent, amplifier command, clamp, reconnect, and config.
3. Error handling MUST surface actionable diagnostics through the project's
   logging or status mechanism once tooling is selected.
4. Broad catch-and-ignore behavior MUST NOT be used around volume commands.
5. Configuration parsing MUST validate ranges and reject unsafe values before the
   daemon accepts hardware input.

TODO(TOOLING): Define language-specific formatter, linter, static-analysis, and
build commands.

## Bluetooth and BluOS Integration Rules

1. Bluetooth event ingestion MUST normalize raw device events before core policy
   receives them.
2. Duplicate, partial, or noisy input events MUST NOT produce repeated unsafe
   amplifier commands.
3. BluOS volume commands MUST be clamped to configured safe limits.
4. The daemon MUST handle amplifier unreachability without retry storms.
5. Integration adapters SHOULD include fakes or fixtures that allow policy tests
   without physical hardware.

## Testing Philosophy

Tests are executable documentation. They MUST make the system easier to
understand, not merely raise a coverage number.

Tests SHOULD be written when they protect critical behavior, explain opaque
logic, guard a regression, or document edge-case contracts. Tests MAY be skipped
for trivial wiring only when the behavior is already covered by a higher-value
test or deterministic check.

Test-driven development SHOULD be used for complex policy, event normalization,
configuration validation, safety clamps, and API contracts. It MAY be skipped for
simple wrappers, mechanical refactors, or documentation-only changes. When TDD is
used, follow red-green-refactor cycles and document expected behavior clearly.

## Test Quality Standards

Every durable test MUST explain:

1. Why it exists: business, safety, bug, or regression reason.
2. Contract asserted: plain-English invariant.
3. Usage notes: how to call the API and notable gotchas.
4. Quality contribution: what failures it catches.
5. Worked example: representative inputs and outputs, when useful.

Tests MUST use behavior-focused names such as Given-When-Then or an equivalent
format. Promoted tests MUST be deterministic and comprehensible without external
hardware unless explicitly marked as hardware smoke checks.

## Scratch to Promote Workflow

1. Probe tests MAY be written in `tests/scratch/` for exploration.
2. `tests/scratch/` MUST be excluded from CI once CI exists.
3. Scratch tests MUST be promoted only when they add durable value.
4. Promotion criteria are Critical path, Opaque behavior, Regression-prone, or
   Edge case.
5. Promoted tests MUST move to `tests/unit/` or `tests/integration/`.
6. Promoted tests MUST include complete Test Doc blocks.
7. Scratch tests without durable value MUST be deleted.

## Test Reliability and External Dependencies

1. Tests in the main suite MUST NOT call the real amplifier or Bluetooth stack
   unless explicitly categorized as hardware smoke checks.
2. Tests MUST NOT rely on sleeps or wall-clock timing; use controllable clocks or
   event simulation.
3. Tests MUST be deterministic; flaky behavior is a defect.
4. Tests SHOULD use realistic fixtures for Bluetooth events and BluOS responses.
5. Mocks MUST document why the real dependency is not used.

TODO(MOCK_POLICY): Choose project mock policy: Avoid, Targeted, or Liberal.
Until then, use targeted mocks only at external hardware/network boundaries.

## Test Organization

The expected test layout is:

| Directory | Purpose |
|---|---|
| `tests/scratch/` | Temporary probes, excluded from CI |
| `tests/unit/` | Isolated core policy and config tests |
| `tests/integration/` | Adapter and daemon lifecycle tests with fakes |
| `tests/e2e/` or `tests/acceptance/` | Full-system checks when applicable |
| `tests/fixtures/` | Shared Bluetooth/BluOS examples |

## Test Documentation Format

Language-specific test syntax may vary, but durable tests MUST include the same
five fields.

```typescript
test('given_knob_turn_when_volume_near_limit_then_clamps_command', () => {
  /*
  Test Doc:
  - Why: Prevent unsafe loud jumps when the knob is turned near the configured limit.
  - Contract: Volume policy never emits a command above maxVolume.
  - Usage Notes: Pass normalized knob events; raw Bluetooth events belong in adapter tests.
  - Quality Contribution: Catches clamp regressions and documents safe-volume behavior.
  - Worked Example: current=38, max=40, step=5 -> command volume=40.
  */
  // Arrange-Act-Assert with clear phases.
});
```

```python
def test_given_knob_turn_when_volume_near_limit_then_clamps_command():
    """
    Test Doc:
    - Why: Prevent unsafe loud jumps when the knob is turned near the configured limit.
    - Contract: Volume policy never emits a command above max_volume.
    - Usage Notes: Pass normalized knob events; raw Bluetooth events belong in adapter tests.
    - Quality Contribution: Catches clamp regressions and documents safe-volume behavior.
    - Worked Example: current=38, max=40, step=5 -> command volume=40.
    """
    # Arrange-Act-Assert with clear phases.
```

## Complexity-First Planning Rules

Planning and reporting MUST NOT use duration or ETA language. Use Complexity
Score (CS 1-5), factor breakdown, confidence, assumptions, dependencies, risks,
and phases.

Required shape:

```json
{
  "complexity": {
    "score": 3,
    "label": "medium",
    "breakdown": {
      "surface": 1,
      "integration": 1,
      "data_state": 1,
      "novelty": 1,
      "nfr": 0,
      "testing_rollout": 1
    },
    "confidence": 0.75
  },
  "assumptions": ["Bluetooth device emits relative rotation events"],
  "dependencies": ["BluOS volume API behavior documented"],
  "risks": ["Unexpected duplicate knob events"],
  "phases": ["Design notes", "Implementation", "Tests", "Configured rollout"]
}
```

For CS-4 or CS-5 work, the plan MUST include staged rollout, configuration gate
or feature flag where applicable, and rollback plan.

## Review Checklist

Reviewers MUST check:

1. Does the change preserve safety-first volume control?
2. Is core policy independent from Bluetooth and BluOS adapter details?
3. Are configuration defaults explicit and safe?
4. Are tests valuable executable documentation?
5. Are daemon failure modes observable and recoverable?
6. Does planning use complexity instead of duration?

## User Customization Notes

<!-- USER CONTENT START -->
Project-specific rules may be placed in this block. Future updates must preserve
this content.
<!-- USER CONTENT END -->
