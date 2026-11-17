# Code Review: Phase 2 — Wi-Fi and read-only BluOS status

## A) Verdict
**APPROVE** (reviewer). All 4 findings (1 MEDIUM, 3 LOW) fixed anyway, because Phase 3 builds on this code; re-verified on the device.

## B) Summary
Diff `1fed55e..HEAD -- firmware scripts justfile` (19 files, +1148/−45; `_computed.diff`). Independent code-review agent with all lenses combined (domains off). Confirmed clean: no password path to screen, serial, logs or fw_wifi output; NVS keys ≤15 chars; XML fails closed on db; mutex copy of AmpState correct; the render-key cache correctly excludes dim state.

## D) Findings
| # | Sev | Where | Finding | Resolution |
|---|-----|-------|---------|------------|
| 1 | MEDIUM | bluos_client.cpp | Amp state stayed `reachable` with stale dB while Wi-Fi was down; a failed /Status kept the old source with a fresh timestamp | Wi-Fi down → `reachable=false, error="Wi-Fi down"`; /Status failure fails the poll; `updated_ms` documented as the freshness basis for Phase 3 |
| 2 | LOW | main.cpp poll_serial | `trim()` dropped trailing spaces from `set wifi_pass` | Only leading whitespace dropped; non-`set` commands right-trimmed |
| 3 | LOW | main.cpp wifi_begin | `WiFi.onEvent` re-registered on every `wifi scan` → duplicate handlers | Registered once in setup() |
| 4 | LOW | main.cpp printf | SSIDs, titles, host and errors were not JSON-escaped | `json_escape()` on every string field; amp error sanitised |

## F) Coverage
| AC | Evidence |
|----|----------|
| ac-0204 | fw-wifi via getpass → NVS; `show` masks `****`; check-staged clean |
| ac-0205 | Live −39/−38 dB "Optical 1"; plain faults (Wi-Fi not found, can't reach amp, knob not connected) |
| ac-0209 | Dimmer test (fake clock, wrap); Vaughan "Both work" |

## G) Commands
`pio test -e native` 16/16 · `pio run -e m5stick` · flash + serial verification (67 JSON lines all parse; one disconnect line per event after 2 scans).
