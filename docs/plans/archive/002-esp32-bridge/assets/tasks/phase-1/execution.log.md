# Execution log — Phase 1: Knob to screen over BLE

## 2026-11-17 16:25 — Phase start
- Harness: repo not adopted (no `.harness/engineering-harness.md`); boot chore resolved noop → standard testing.
- **Noteworthy**: tk-2101 (BLE spike) runs after the no-hardware tasks (tk-2102..2106), because it requires unpairing the knob from the Mac, which stops the live Mac daemon. Asked Vaughan on tmwk885d1c.

## tk-2102 — scaffold `firmware/` + justfile recipes — BLOCKED
- Wrote `firmware/platformio.ini` (envs `m5stick`: espressif32 / m5stick-c / Arduino + M5Unified + NimBLE-Arduino; `native`: unity), `firmware/src/main.cpp` boot stub, `firmware/.gitignore`, justfile `fw-build`/`fw-test`/`fw-flash`/`fw-monitor` via `uvx --from platformio pio`.
- `just fw-build` → exit 2: uv could not fetch `platformio-6.2.0-py3-none-any.whl.metadata` from files.pythonhosted.org ("Socket is not connected", os error 57).
- Probe: `curl https://files.pythonhosted.org/` → curl 35 Recv failure (×4, 3 s apart); `pypi.org/simple/platformio/` → 200; github.com → 200; dl.registry.platformio.org → 301. DNS resolves to Fastly 151.101.x.223.
- `harness observe` DL-001 recorded.

## Discoveries & Learnings
| # | Tag | Discovery |
|---|-----|-----------|
| 1 | Noteworthy | Spike reordered after no-hardware tasks (knob unpair stops Mac daemon). |
| 2 | Deferred | Toolchain blocked by files.pythonhosted.org reachability; options: `brew install platformio` (global) or wait. |

## 16:35 — unblocked: Vaughan "use brew install"; also "you can disconnect it from the mac (the knob)"
- `brew install platformio` → PlatformIO Core 6.2.0 (/opt/homebrew/bin/pio); justfile `pio := "pio"`. **Noteworthy**: machine-wide install, approved by Vaughan.

## tk-2103 — RED decoder tests — done
- `firmware/test/test_decoder/test_decoder.cpp`: 5 Given-When-Then tests with Test Doc blocks (real capture vectors `03e900…037000`, report-id-less BLE form, release, unmapped/truncated, action names).
- `just fw-test` against a stub: 4 FAILED on assertions, 1 passed (unknown-path) → RED.

## tk-2104 — GREEN decoder — done
- `firmware/lib/knob_core/src/decoder.{h,cpp}`: 16-bit LE usage scan + release rule, table identical to `normalizer.py:7-13`.
- `just fw-test` → 5 test cases: 5 succeeded.

## tk-2102 — scaffold — done
- First `just fw-build` downloaded espressif32@8.0.0 + xtensa toolchain (≈7.5 min, one-off); rebuild → SUCCESS, RAM 12.9% (42 KB static), Flash 61.0% (800 KB of 1.31 MB) with NimBLE-Arduino + M5Unified.
- `git status` shows no `.pio/` (firmware/.gitignore).

## Flash + first boot (tk-2106 partial)
- `FW_PORT=/dev/cu.usbserial-89521F7539 just fw-flash` → SUCCESS.
- Serial: `{"boot":"bluos-knob","board":"StickC+","free_heap":207512}`; `status` → knob `scanning`, free_heap 186924, min_free_heap 186880. **Board resolved: M5StickC Plus (135×240)** (open question 1).

## Knob bonded to the ESP32 (tk-2101 superseded, tk-2105, tk-2106, tk-2107 part 1)
- Vaughan forgot ANTICATER_MINI on the Mac (06:08Z): "Done, forgot it, and i can see it's working on the device now!"
- Serial `17:07:31 {"knob":"connected","name":"ANTICATER_MINI","addr":"ee:aa:b8:d6:82:5e","reports":6}`: bond + 6 Report notifications on the first attempt.
- 42 gestures logged, every action seen: `volume_up e900`, `volume_down ea00`, `mute_toggle e200`, `brightness_up 6f00`, `brightness_down 7000`. dropped 0; free heap 184164, min 183956.
- **Discovery**: BLE payloads are 2 bytes with no report id (dossier F-02 prediction confirmed); covered by `given_ble_payload_without_report_id…`.
- **Noteworthy**: tk-2101 throwaway spike marked `na`; the real adapter proved feasibility directly. Its coexistence assertion is moved to tk-2207.
- **Discovery**: one knob detent often yields 2–3 events within ~100 ms (seq 35–37). Phase 3 policy must treat each as one step (Mac daemon does the same), so no change now; note for screen/feel.

## tk-2107 reconnect: ESP32 side
- 17:10:52 serial port open → POWERON_RESET; 17:10:53 scanning; **17:10:55 `{"knob":"connected","name":"","addr":"ee:aa:b8:d6:82:5e","reports":6}`**: bonded reconnect in ~2 s via the `isBonded` path (knob advertises without its name once bonded).
- Knob-side drop: the knob has no off switch (Vaughan); testing by walking it out of range.

## tk-2107 reconnect: knob side (bug found + fixed)
- Out-of-range test: 17:12:27 `disconnected reason 520` (supervision timeout); 17:13:22 `connect_failed`; then the device scanned without ever seeing the knob again.
- **Root cause (Discovery)**: the ESP32 controller's duplicate-advertisement filter persists across scans, so after one failed connect the knob was never reported again.
- **Fix** (`firmware/src/ble_knob.cpp` rewrite): when a bond exists, skip scanning and keep an *async* direct connect pending to the bonded address (30 s timeout, retry after 500 ms); blocking secure/discover/subscribe moved out of the NimBLE host task into `loop()`; unbonded scans use `setDuplicateFilter(0)` and `setMaxResults(0)`.
- After flashing: two `connect_failed reason 13` (timeout) while **the knob had re-attached to the Mac** (Vaughan 06:18:09Z: "it connected to the mac again"). Then 17:18:04 `{"knob":"connected","addr":"ee:aa:b8:d6:82:5e","reports":6}`; Vaughan: "it's fixed!".
- **Noteworthy**: the knob keeps its Mac bond; if it drifts back to the Mac, forget it there again (goes in the Phase 3 runbook).
- Added a serial `scan` diagnostic (logs nearby advertisers: address, type, adv type, name, RSSI, connectable, bonded) for future pairing problems.

## Phase 1 complete
- tk-2101 na (superseded; coexistence proof → tk-2207), tk-2102..2107 checked. AC-01, AC-02, AC-03 met.

## Review fixes + re-test (17:2x)
- Fixed the 3 review findings (diag-scan state loss, `scan->stop()` in onResult use-after-free, `g_peer_name` race) plus diag scan starting before the connect cancel had finished. Reflashed.
- `scan` diagnostic now works (15 s of nearby advertisers logged); the knob was not advertising.
- 17:28 `system_profiler`: **ANTICATER_MINI Connected to the Mac again**, which explains repeated `connect_failed reason 13` (timeout). The knob re-attaches to the Mac on wake even after "Forget". **Noteworthy** for the cut-over runbook: the Mac must forget the knob *and* the Mac daemon should be unloaded.
- 06:29Z Vaughan: "Turn the Mac daemon off now". `launchctl bootout` + `launchctl disable gui/$UID/com.vaughanknight.bluos-knob` → not in `launchctl list`. Rollback: `launchctl enable …` then `launchctl bootstrap gui/$UID ~/Library/LaunchAgents/com.vaughanknight.bluos-knob.plist`.
- Vaughan forgot + "ignored" the knob on the Mac, but `system_profiler` still showed it Connected; diag scan showed no `ee:aa:b8:d6:82:5e` adverts. Sent `bonds clear` to re-pair by name; asked Vaughan to turn the Mac's Bluetooth off briefly.
- Final build evidence: **17:29:45 bonded direct reconnect** (`name:""`, reports 6) as soon as the Mac released the knob; 17:30:25 `bonds_cleared`; **17:30:30 fresh pairing by name** (`ANTICATER_MINI`, reports 6, 5 s); gestures seq 43+ flowing. Vaughan 06:30:17Z: "and working!".
- **Final-build out-of-range test**: 17:31:16 `disconnected reason 520` → **17:31:21 `connected` (5 s, automatic)**. Vaughan 06:31:39Z: "Came back green".
- Knob power-cycle is not testable (no off switch); covered by ESP32-reset + out-of-range + Mac-release reconnects.
