# Code Review: Phase 1 — Knob to screen over BLE

## A) Verdict
**APPROVE (after fixes)**. The first pass was REQUEST_CHANGES (1 HIGH, 2 MEDIUM, 1 LOW); all fixed and re-verified on hardware.

## B) Summary
Diff `b94a3c1..HEAD -- firmware justfile` (12 files, +720; `_computed.diff`). The decoder matches `normalizer.py` exactly (table, 16-bit LE scan, release rule); Test Doc blocks follow rules.md. Reviewer: independent code-review agent, with four lenses run together (quality, anti-reinvention, testing/evidence, doctrine) because the diff is small; domain compliance is N/A (domains off).

## D) Findings
| # | Sev | Where | Finding | Resolution |
|---|-----|-------|---------|------------|
| 1 | HIGH | ble_knob.cpp diag_scan | `scan` while connected forced Disconnected → endless `connect()` refusals (NimBLEClient.cpp:261) | Link re-derived from `client()->isConnected()`; `start_connect` refuses to stack on a live link |
| 2 | MEDIUM | ble_knob.cpp onResult | `scan->stop()` in onResult + `setMaxResults(0)` → use-after-free (NimBLEScan.cpp:343) | onResult only records + flags; loop() stops the scan |
| 3 | MEDIUM | execution log | Reconnect evidence predated the rewrite; sleep/power-cycle not shown | Re-run on the final build: bonded reconnect 17:29:45, fresh pair 17:30:30, out-of-range 17:31:16→17:31:21; power-cycle N/A (no switch) |
| 4 | LOW | ble_knob.cpp g_peer_name | String written on host task, read on loop task | Callback writes a fixed buffer once, before g_found; loop() takes ownership |
| + | — | diag_scan | (found during re-test) scan started before the connect cancel finished | Wait for onConnectFail (≤2 s) before scanning |

## F) Coverage
| AC | Evidence |
|----|----------|
| ac-0201 | `just fw-build` SUCCESS; `just fw-test` 5/5 |
| ac-0202 | test_decoder.cpp: capture vectors + report-id-less + release + unknown, all pass |
| ac-0203 | 164+ gestures on screen/serial, 0 dropped; auto-reconnect after ESP32 reset, after the Mac released the knob, and after out-of-range |

## G) Commands
`just fw-test` · `just fw-build` · `just fw-flash` · serial `status`/`scan`/`bonds clear` via /tmp/bk_serlog.py · `system_profiler SPBluetoothDataType`
