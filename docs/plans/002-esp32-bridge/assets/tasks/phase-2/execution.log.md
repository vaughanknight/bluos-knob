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
