# Code Review: plan 003 — Coalesced control + idle screen

## A) Verdict
**APPROVE (after fixes)**. Reviewer: REQUEST_CHANGES (1 HIGH, 1 MEDIUM), both fixed and reflashed.

## B) Summary
Safety-focused review of `71b8fa1..33e655c -- firmware`. Confirmed: coalesced/clamped increases never exceed max_db (fresh read, muteDb basis, abs write); mute/source barriers keep order; read-retry never matches a write path; HTTPClient keep-alive has no internal resend, discards leftovers on connect, and honours `Connection: close`.

## D) Findings
| # | Sev | Where | Finding | Resolution |
|---|-----|-------|---------|------------|
| 1 | HIGH | amp_control.cpp control_task | A drained burst kept running after `live off` or a knob reconnect: generation was checked only at drain, `live` read once per burst | Generation re-checked and `g_live` re-read before **each** command; a change abandons the rest of the burst (counted as dropped) |
| 2 | MEDIUM | amp_control.cpp control_task | 1 s staleness only checked at drain; later burst commands could run seconds late behind slow HTTP | Burst split into segments (volume runs / barriers) with their newest event time; staleness re-checked before each segment |

## G) Commands
`pio test -e native` 33/33 · `pio run -e m5stick` · flash + status (LIVE, knob + Wi-Fi connected, amp ok).
