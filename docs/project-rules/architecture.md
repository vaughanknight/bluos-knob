<!--
Sync Impact Report
Mode: CREATE
Version: 1.0.0
Constitution: docs/project-rules/constitution.md
Outstanding TODOs: TODO(RUNTIME), TODO(BLUETOOTH_STACK), TODO(BLUOS_API),
TODO(SERVICE_MANAGER)
-->

# Project Architecture

**Version**: 1.0.0  
**Ratified**: 2026-06-05  
**Last amended**: 2026-06-05

## System Purpose

Exotic Knob is a local daemon application. A Bluetooth volume knob connects to
the daemon, and the daemon controls volume on a NAD M33 BluOS amplifier.

The architecture prioritizes safe physical side effects, deterministic policy,
recoverable long-running operation, and explicit configuration.

## High-Level Structure

```text
+--------------------+      +----------------------+      +-------------------+
| Bluetooth Volume   | ---> | Local Daemon         | ---> | NAD M33 BluOS     |
| Knob               |      |                      |      | Amplifier         |
+--------------------+      +----------------------+      +-------------------+
                               |
                               v
                         Local logs/status
```

Inside the daemon:

```text
+-------------------+     +-------------------+     +-------------------+
| Bluetooth Adapter | --> | Volume Policy     | --> | BluOS Adapter     |
+-------------------+     +-------------------+     +-------------------+
          |                         |                         |
          v                         v                         v
 Raw device events        Normalized intents         Amplifier commands
```

## Components

### Daemon Runtime

Responsibilities:

1. Start once and maintain process lifecycle.
2. Load and validate configuration.
3. Wire adapters to core policy.
4. Expose local status and logs.
5. Handle shutdown cleanly.

TODO(RUNTIME): Select the implementation language and runtime.

### Configuration

Responsibilities:

1. Identify the Bluetooth knob.
2. Identify the BluOS amplifier endpoint.
3. Define safe volume settings.
4. Configure startup behavior and diagnostics.

Configuration validation MUST reject unsafe or incomplete values before hardware
input is accepted.

### Bluetooth Adapter

Responsibilities:

1. Connect to the configured knob.
2. Translate raw Bluetooth events into normalized knob events.
3. Report connection state and adapter errors.
4. Avoid leaking raw transport concerns into volume policy.

TODO(BLUETOOTH_STACK): Choose the supported Bluetooth API or library.

### Volume Policy

Responsibilities:

1. Convert normalized knob events into volume intents.
2. Clamp output to configured bounds.
3. De-duplicate unsafe repeated events.
4. Decide behavior when amplifier state is unknown.
5. Remain testable without Bluetooth or BluOS dependencies.

The volume policy is the safety-critical core of the project.

### BluOS Adapter

Responsibilities:

1. Discover or connect to the NAD M33 BluOS endpoint.
2. Read current volume when needed.
3. Send bounded volume commands.
4. Surface protocol and reachability errors.
5. Avoid retry storms and speculative corrections.

TODO(BLUOS_API): Document the exact BluOS API endpoints and volume semantics.

### Service Manager Integration

Responsibilities:

1. Install/start/stop the daemon on the target host.
2. Restart on recoverable process failure.
3. Preserve safe startup behavior.
4. Surface logs through platform-appropriate tools.

TODO(SERVICE_MANAGER): Decide whether the target supervisor is launchd, systemd,
or another mechanism.

## Allowed Dependencies

| From | May depend on | Must not depend on |
|---|---|---|
| Daemon Runtime | Config, adapters, volume policy | Raw hardware details in policy logic |
| Bluetooth Adapter | Bluetooth library/API, config, normalized event contract | BluOS adapter internals |
| Volume Policy | Plain data contracts and config values | Bluetooth library, network client, service manager |
| BluOS Adapter | BluOS API client, config, amplifier command contract | Bluetooth adapter internals |
| Tests | Core policy, fakes, fixtures | Real hardware in default deterministic suite |

## Interaction Contracts

### Normalized Knob Event

A normalized knob event SHOULD capture:

1. Direction or action.
2. Step count or magnitude.
3. Source device identity.
4. Sequence or timestamp-like ordering token if provided by the adapter.
5. Raw-event reference for diagnostics only.

### Volume Intent

A volume intent SHOULD capture:

1. Target volume or relative adjustment.
2. Clamp decision.
3. Safety reason when command is rejected.
4. Correlation to the triggering knob event.

### Amplifier Command Result

An amplifier command result SHOULD capture:

1. Accepted/rejected status.
2. Observed or confirmed volume if available.
3. Recoverable versus terminal error category.
4. Diagnostic message suitable for local logs.

## Failure Modes and Guardrails

| Failure mode | Required guardrail |
|---|---|
| Bluetooth disconnects | Reconnect without emitting stale volume commands |
| Duplicate knob events | De-duplicate or clamp according to policy |
| Amplifier unreachable | Surface status and avoid retry storms |
| Unknown current volume | Use safe startup behavior before relative changes |
| Invalid configuration | Refuse to accept hardware input |
| Daemon restart | Avoid replaying stale commands |

## Architecture Anti-Patterns

1. Raw Bluetooth callbacks issuing BluOS commands directly.
2. Hardcoded amplifier addresses or volume limits in source code.
3. Tests that require physical devices for core policy behavior.
4. Silent error swallowing around volume commands.
5. Retry loops without backoff or state awareness.
6. Service startup that can emit a volume command before configuration validation.

## Domain Integration

Domain infrastructure is not initialized. When domains are introduced, this
architecture should map naturally to domains such as Daemon Runtime, Device
Input, Volume Policy, Amplifier Control, and Configuration.

Until then, source organization SHOULD still preserve those boundaries through
directories, modules, or packages.

## Reviewer Checklist

Reviewers should ask:

1. Can this behavior be tested without real Bluetooth or BluOS hardware?
2. Could this change produce an unexpected loud volume jump?
3. Does configuration make device-specific assumptions visible?
4. Are adapter failures surfaced without corrupting policy state?
5. Does daemon lifecycle behavior preserve safety on startup and restart?

## User Customization Notes

<!-- USER CONTENT START -->
Project-specific architecture notes may be placed in this block. Future updates
must preserve this content.
<!-- USER CONTENT END -->
