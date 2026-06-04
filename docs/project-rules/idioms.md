<!--
Sync Impact Report
Mode: CREATE
Version: 1.0.0
Constitution: docs/project-rules/constitution.md
-->

# Project Idioms

**Version**: 1.0.0  
**Ratified**: 2026-06-05  
**Last amended**: 2026-06-05

Idioms are preferred patterns that make the rules concrete. They are examples,
not substitutes for the constitution or rules.

## Adapter-Core-Adapter Flow

Prefer this flow for behavior that starts at the knob and ends at the amplifier:

```text
Bluetooth adapter -> normalized knob event -> volume policy -> amplifier intent -> BluOS adapter
```

Core policy should be deterministic and testable with plain data. Adapters own
transport details, retries, device discovery, and protocol-specific errors.

## Safe Volume Policy Pattern

Volume behavior should be expressed as policy before it is expressed as I/O:

```text
current volume + normalized event + config -> bounded volume intent
```

The policy should handle:

1. Minimum and maximum volume.
2. Step size.
3. Duplicate events.
4. Startup state when current amplifier volume is unknown.
5. Error behavior when the amplifier cannot confirm state.

## Configuration Idiom

Configuration should make unsafe assumptions visible:

```text
bluetooth_device_id: user-specific knob identity
amplifier_host: NAD M33 or BluOS endpoint
volume_step: bounded relative step
max_volume: hard safety ceiling
startup_mode: connect-only, sync-state, or disabled output
log_level: local diagnostics level
```

Defaults should favor no unexpected amplifier output.

## Observability Idiom

Daemon status should explain:

1. Bluetooth connection state.
2. Amplifier reachability.
3. Last normalized knob event.
4. Last accepted volume intent.
5. Last rejected command and reason.

Avoid logs that reveal private network details beyond what is needed for local
diagnosis.

## Test Doc Idiom

Durable tests should read like a small usage guide:

```text
Test Doc:
- Why: Safety or regression reason.
- Contract: Invariant in plain English.
- Usage Notes: Correct API call pattern and gotchas.
- Quality Contribution: What defect class this catches.
- Worked Example: Representative input -> output.
```

Good daemon tests favor simulated events and fake adapters over physical
hardware. Hardware smoke checks should be explicit and separate from deterministic
tests.

## Scratch Promotion Idiom

Use scratch tests to learn, then promote only durable knowledge:

| Keep when | Example |
|---|---|
| Critical path | Knob turn maps to bounded BluOS volume command |
| Opaque behavior | Bluetooth event stream emits duplicate messages |
| Regression-prone | Reconnect emits stale event after device wake |
| Edge case | Current volume is unknown at daemon startup |

Delete scratch tests that only captured temporary exploration.

## Complexity Calibration Examples

| Work item | Breakdown | Result |
|---|---|---|
| Rename a constant used in one file | S=0, I=0, D=0, N=0, F=0, T=0 | CS-1 (trivial) |
| Add config validation for max volume | S=1, I=0, D=1, N=0, F=1, T=1 | CS-3 (medium) |
| Add BluOS adapter using a documented API | S=1, I=1, D=0, N=1, F=1, T=1 | CS-3 (medium) |
| Introduce daemon supervision and reconnect orchestration | S=2, I=2, D=1, N=1, F=2, T=2 | CS-5 (epic) |

## Naming Idioms

Prefer names that reveal domain intent:

| Prefer | Avoid |
|---|---|
| `NormalizedKnobEvent` | `RawData` |
| `VolumeIntent` | `Payload` |
| `AmplifierAdapter` | `Client` without context |
| `maxVolume` or `max_volume` | `limit` without units |
| `rejectUnsafeVolumeJump` | `handleError` |

## User Customization Notes

<!-- USER CONTENT START -->
Project-specific idioms may be placed in this block. Future updates must
preserve this content.
<!-- USER CONTENT END -->
