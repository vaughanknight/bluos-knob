<!--
Sync Impact Report
Mode: CREATE
Version bump: none -> 1.0.0
Rationale: Establish initial doctrine for a local daemon that maps a Bluetooth
volume knob to safe NAD M33 BluOS amplifier volume control.
Sections preserved: 0
Sections updated: 0
Custom content retained: 0
New sections added: Guiding Principles; Quality & Verification Strategy;
Delivery Practices; Governance; Complexity-First Estimation; Domain Governance;
User Customization Notes
Outstanding TODOs: TODO(TOOLING), TODO(BLUETOOTH_STACK), TODO(BLUOS_API),
TODO(SERVICE_MANAGER), TODO(CI)
Supporting docs/templates update status: No templates or command references found.
-->

# Exotic Knob Project Constitution

**Version**: 1.0.0  
**Ratified**: 2026-06-05  
**Last amended**: 2026-06-05

## Guiding Principles

### I. Safety-First Volume Control

The daemon MUST prevent surprising or unsafe volume changes. Volume commands MUST
be bounded, monotonic with user input, and resilient to duplicate or noisy knob
events. The system SHOULD fail silent on amplifier-control errors rather than
issuing speculative corrections.

Rationale: The primary side effect is audible output from a real amplifier. A
software fault must not create a loud jump or oscillating volume behavior.

### II. Deterministic Input-to-Output Mapping

Bluetooth knob events MUST be normalized into an explicit internal command model
before reaching the BluOS amplifier adapter. The mapping from event type,
rotation direction, and step magnitude to volume intent MUST be testable without
Bluetooth hardware or the amplifier.

Rationale: Hardware integrations are easier to trust when the risky decisions
are isolated from transport code.

### III. Resilient Local Daemon Operation

The application MUST be designed as a long-running local daemon that can recover
from Bluetooth disconnects, amplifier reachability changes, process restarts,
and duplicate startup attempts. It SHOULD expose enough local observability to
explain current connection state and the last accepted volume command.

Rationale: A daemon that controls a daily-use physical device must remain
predictable across ordinary network and hardware disruptions.

### IV. Explicit Configuration Boundaries

Bluetooth device identity, amplifier target, volume step size, maximum allowed
volume, startup behavior, and logging level MUST be configuration, not hidden
constants. Configuration MUST NOT contain secrets committed to source control.

Rationale: The project depends on user-specific devices and local network
topology; behavior changes must be auditable and reversible.

### V. Adapter Boundaries Around External Systems

Bluetooth and BluOS/NAD integration code MUST sit behind adapter interfaces.
Core volume policy MUST NOT import hardware, network, or service-manager
implementation details directly.

Rationale: Clean adapter boundaries let tests cover safety logic deterministically
and let the project change Bluetooth or BluOS libraries without rewriting policy.

## Quality & Verification Strategy

Changes are safe when they are backed by deterministic checks for the core
volume policy and appropriate manual or hardware-backed validation for external
integration behavior.

The project MUST maintain tests for:

1. Knob-event normalization and de-duplication.
2. Volume step computation, clamping, and maximum-volume enforcement.
3. Error handling when Bluetooth or BluOS connections are unavailable.
4. Configuration parsing and validation.
5. Daemon lifecycle behavior that can be simulated without physical hardware.

External Bluetooth and BluOS behavior SHOULD be tested through adapter-level
fakes or fixtures first. Hardware validation MAY be documented as a manual smoke
check when physical devices are required.

TODO(TOOLING): Select the implementation language and canonical local commands
for format, lint, test, build, and daemon smoke checks.

TODO(BLUETOOTH_STACK): Choose the Bluetooth library or system API and document
supported operating systems.

TODO(BLUOS_API): Document the NAD M33 BluOS control API, including volume range,
error responses, authentication needs if any, and discovery strategy.

## Delivery Practices

Work MUST proceed in small, reviewable phases that keep safety behavior
demonstrable. Each phase SHOULD identify:

1. The behavior changed.
2. The deterministic checks that prove the behavior.
3. The hardware or manual smoke check, if physical devices are involved.
4. Configuration changes and their rollback path.

The definition of done for daemon behavior includes updated documentation,
updated tests, and evidence that the daemon cannot issue unsafe volume jumps for
the covered scenario.

TODO(SERVICE_MANAGER): Decide how the daemon is installed and supervised on the
target machine.

TODO(CI): Establish continuous checks once the language/tooling is selected.

## Complexity-First Estimation

All planning and reporting MUST use complexity, risk, scope, and uncertainty
instead of duration or ETA language.

Prohibited estimate forms include hours, minutes, days, deadlines, "quick",
"fast", "soon", or equivalent duration implications. Required effort
quantification uses Complexity Score (CS 1-5).

Compute points from six factors, each scored 0-2:

| Factor | 0 | 1 | 2 |
|---|---|---|---|
| Surface Area | One file | Multiple files | Many files or cross-cutting |
| Integration Breadth | Internal only | One external system | Multiple or unstable external systems |
| Data & State | None | Minor state/config changes | Non-trivial persistence, migration, or concurrency |
| Novelty & Ambiguity | Well specified | Some ambiguity | Significant discovery required |
| Non-Functional Constraints | Standard checks | Moderate constraints | Strict safety, security, or reliability constraints |
| Testing & Rollout | Unit checks | Integration or smoke checks | Staged rollout, compatibility, or rollback design |

Point totals map to:

| Points | Score | Label |
|---:|---|---|
| 0-2 | CS-1 | Trivial |
| 3-4 | CS-2 | Small |
| 5-7 | CS-3 | Medium |
| 8-9 | CS-4 | Large |
| 10-12 | CS-5 | Epic |

Planning outputs MUST include complexity score, factor breakdown, assumptions,
dependencies, risks, and phases. For CS-4 or CS-5 work, plans MUST include a
staged rollout, feature flag or configuration gate where applicable, and rollback
plan.

## Governance

This constitution is authoritative for project rules, idioms, architecture, and
planning. Changes to doctrine MUST update all affected files under
`docs/project-rules/` in the same change.

Doctrine amendments MUST include:

1. Version bump rationale.
2. Description of changed principles or rules.
3. Impact on tests, architecture, and delivery practices.
4. Outstanding TODOs with named fields.

Versioning follows semantic doctrine changes:

| Bump | Use when |
|---|---|
| MAJOR | Principles or governance change incompatibly |
| MINOR | New principles, sections, or materially expanded guidance |
| PATCH | Clarifications, formatting, or non-material wording changes |

## Domain Governance

Domain infrastructure is not yet initialized for this project. Domain governance
applies once domains are established through `/plan-v2-extract-domain`.

When active:

1. Every source file belongs to a domain or is explicitly uncategorized legacy.
2. Cross-domain imports MUST use public contracts only.
3. Business domains MAY depend on infrastructure domains.
4. Infrastructure domains MUST NOT depend on business domains.
5. Business-to-business dependencies MUST use contracts.
6. `docs/domains/registry.md` is the authoritative domain index.
7. `docs/domains/domain-map.md` MUST stay current with contract edges.

## User Customization Notes

<!-- USER CONTENT START -->
Project-specific doctrine additions may be placed in this block. Future
constitution updates must preserve this content.
<!-- USER CONTENT END -->
