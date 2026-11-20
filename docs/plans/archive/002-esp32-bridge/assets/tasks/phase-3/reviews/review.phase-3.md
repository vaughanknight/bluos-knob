# Code Review: Phase 3 — Guarded amp control and cut-over

## A) Verdict
**APPROVE (after fixes)**. Reviewer: REQUEST_CHANGES (2 MEDIUM, 1 LOW), all fixed test-first and reflashed.

## B) Summary
Safety-focused independent code-review agent over `3e2f303..HEAD -- firmware scripts justfile` (15 files, +1062; `_computed.diff`). Confirmed: no write uses a stale or muted value as its basis; live/dry-run gating, queue generation flush and stale-event logic hold.

## D) Findings
| # | Sev | Where | Finding | Resolution |
|---|-----|-------|---------|------------|
| 1 | MEDIUM | amp_policy.cpp volume step | Read→relative `db=+step` write isn't atomic; a change made elsewhere in between could pass max_db (Python has the same gap) | Increases now write `abs_db=<target checked against max_db>`; decreases stay relative. Test `given_volume_raised_elsewhere…` (RED −21 → GREEN −24) |
| 2 | MEDIUM | amp_policy.cpp switch_source | No re-check after `/Play`; an input with its own louder level stayed loud | Re-read after switching; above source_safe_db → `abs_db=source_safe_db`. Test `given_input_with_its_own_level…` (RED −20 → GREEN −40) |
| 3 | LOW | fw_console.py / amp_control.cpp | `"remembered":false` ignored; console claimed DRY-RUN was saved | Firmware retries the save once; console prints a warning and exits 1 when not saved |
| + | — | amp_policy.cpp fmt_db | (hardening) `%g` could produce exponent notation | Fixed-point `%.1f`, trimmed `.0` |

Found earlier in this phase on the live system: muted amp reports db=−100 → muteDb basis (commit 4240286).

## F) Coverage
| AC | Evidence |
|----|----------|
| ac-0206 | Live refusal at cap (0 writes); 13 policy tests incl. muted, race and source re-check |
| ac-0207 | Fresh boot DRY-RUN; phrase-gated live; remembered (owner decision); badge |
| ac-0208 | docs/how/esp32-bridge.md + README |

## G) Commands
`pio test -e native` 31/31 · `pio run -e m5stick` · flash + serial status (live, amp ok, knob connected).
