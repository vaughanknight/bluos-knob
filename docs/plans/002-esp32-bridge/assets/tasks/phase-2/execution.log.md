# Execution log — Phase 2: Wi-Fi and read-only BluOS status

## 17:33 — start
- Boot chore noop (repo not adopted). Mac daemon disabled at Vaughan's request (Phase 1 log).

## tk-2201 config validation — done (TDD)
- `lib/knob_core/src/config.{h,cpp}` + `test/test_config` (5 tests, Test Doc blocks). RED: 4 failed on stub → GREEN.
- **Noteworthy**: firmware tightens Python's rules: `max_db` ≤ −24 and `0 < step_db ≤ 3` (Python only capped `source_safe_db`).

## tk-2204/2205 BluOS XML + read-only adapter — done (TDD)
- Fixtures `tests/fixtures/bluos/volume.xml`, `status.xml` captured read-only (`curl http://192.168.1.67:11000/Volume|Status`); no secrets.
- `bluos_xml.{h,cpp}`: root-checked attribute/element extraction, exact tag match (`<title1>` ≠ `<twoline_title1>`), entity decode, fail-closed on missing/non-finite db. RED 3 failed → GREEN 4/4.
- `timing.{h,cpp}`: Backoff (doubling to cap, reset on success) and wrap-safe Dimmer; `test_timing` 2/2. Unity needed `-D UNITY_INCLUDE_DOUBLE`.
- `src/bluos_client.cpp`: separate FreeRTOS task polls `/Volume` + `/Status` every 2 s, Backoff 2→30 s on failure, mutex-guarded snapshot.
- `just fw-test` → 16/16.

## tk-2202 NVS store + console — implemented, awaiting reboot-persistence check
- `config_store.cpp` (Preferences "bluos-knob"); console `set <key> <value>` (value never echoed), `show` (password ****), `reboot`, plus existing `status`/`scan`/`bonds clear`.
- Smoke on device: boot → `{"config":"invalid","error":"Wi-Fi not set up (run just fw-wifi)"}`; `set max_db -10` → refused; `set colour blue` → unknown setting.
- **Discovery**: firmware hit 113.5% of the 1.31 MB OTA app slot once Wi-Fi + HTTPClient were added → `board_build.partitions = huge_app.csv` (3 MB, no OTA, which is a plan non-goal); NVS offset unchanged so the knob bond survives. Flash now 47.3%, RAM 19.9%; free heap at idle ≈157 KB.

## tk-2203 `just fw-wifi` — implemented, Vaughan to run
- `scripts/fw_wifi.py` (pyserial from the PlatformIO venv): auto-detects the port, `getpass` password, `set` each key, waits for `{"wifi":"connected"}` then reports amp reachability. ruff clean.

## Vaughan feedback + first fw-wifi run (17:45–17:50)
- "the font on the device is too small for status bits": switched to M5GFX proportional fonts (Font2 16 px status/fault, Font7 48 px 7-segment dB, Font4 26 px source); dropped the redundant title.
- First `just fw-wifi` crashed with `SerialException … multiple access on port`: **my concurrent `pio … -t upload` collided with it** (agent error). Settings had saved (`show`: ssid set, wifi_pass `****`, host 192.168.1.67, valid true). `fw_wifi.py` now exits with a plain message on SerialException.
- Wi-Fi never joined: added STA disconnect-reason logging + plain faults. Device logs reason **201 (NO_AP_FOUND)** repeatedly. Mac is on the same SSID at **5 GHz ch 161**; the ESP32 is 2.4 GHz-only. Asked Vaughan for a 2.4 GHz network. **Noteworthy**: a 2.4 GHz requirement goes in the runbook/README.
- Lesson: never flash while a human is running a serial tool; the logger/flash must check the port first.
- Added serial `wifi scan` (pauses the pending connect: a scan returns -2/WIFI_SCAN_FAILED while a connect is in progress; needed `WiFi.disconnect(true)` + `WiFi.mode(WIFI_STA)`). Device scan: 29 networks on 2.4 GHz; Vaughan's UDM SSIDs on ch 11 at −60 dBm ("{open-2.4ghz-ssid}" open, "{home-2.4ghz-ssid}"/"{other-2.4ghz-ssid}" WPA2, one hidden); **"{home-5ghz-ssid}" absent → confirmed 5 GHz-only**. Offered: use an existing 2.4 GHz SSID on the amp's LAN, or enable 2.4 GHz on {home-5ghz-ssid} in UniFi.

## Wi-Fi joined + read-only status live (tk-2203, tk-2207 part 1)
- Vaughan chose existing 2.4 GHz SSID "{home-2.4ghz-ssid}" and ran `just fw-wifi`: joined as 192.168.1.69 (same /24 as the amp). The script reported "didn't find it" because it checked status before the first amp poll (**bug, fixed**: retries status for up to ~10 s).
- Serial `status`: `wifi connected, amp ok, db -39.0, source "Optical 1"`; knob + Wi-Fi together: gestures seq 83–86 flowing, dropped 0.
- Mac re-grabbed the knob despite Forget + Ignore (Vaughan 08:02Z). **Deferred** to the Phase 3 cut-over: investigate macOS re-pair, document in the runbook.
- **Unreachable-amp fault** (read-only): `set host 192.168.1.250` + reboot → screen/serial fault "can't reach amp 192.168.1.250", backoff retry 2000→4000→8000→16000 ms; knob auto-reconnected after reboot (bond). Restored `host 192.168.1.67` + reboot → amp ok, fault cleared. **Config persisted across both reboots** (tk-2202).
- Heap with BLE + Wi-Fi + HTTP: free ≈88–92 KB, min_free 81300 B.
