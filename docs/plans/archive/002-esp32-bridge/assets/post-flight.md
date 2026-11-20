# Post-flight — 002-esp32-bridge
**Closed out**: 2026-11-20T13:45+11:00
**Archived to**: docs/plans/archive/002-esp32-bridge/
**Shipped**: pushed to origin/main with the close-out (Vaughan: "push it", tmwk885d1c 02:42Z)

## Completion
| Check | Result |
|-------|--------|
| Phases / tasks | all 3 complete (tk-2101 superseded; tk-2207 dw-2073 + tk-2306 dw-3061 marked na with notes) |
| Acceptance criteria | ac-0201..0209 checked; `harness plan validate --complete` 0 err / 0 warn |
| Latest review | Phase 3: APPROVE after fixes (review.phase-3.md) |
| Soak | Vaughan used it 2 days: "it's all working fine from all my testing" (2026-11-20) |

## Open / deferred items
_Carried into the archive on Vaughan's explicit go-ahead._
| Kind | Item | Where | Note |
|------|------|-------|------|
| Deferred | Optical/Spotify buttons not confirmed live on the amp | phase-3 log | Covered by policy tests (lower-first, post-switch re-check) |
| Deferred | Python Mac daemon has the muted −100 dB basis and read→relative-write gaps | scripts/bluos_daemon.py, bluos_volume_step.py | Daemon is disabled (`launchctl disable`); fix before re-enabling |
| Noteworthy | macOS re-grabs the knob despite Forget + Ignore | phase-1/3 logs, runbook | Workaround documented in docs/how/esp32-bridge.md |
| Noteworthy | Owner decision: LIVE remembered across restarts | plan clarifications | Supersedes plan-review session-only live |
