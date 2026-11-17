# Research Dossier: Running the bluos-knob daemon on an M5 ESP32

**Generated**: 2026-11-17T16:20+11:00
**Query**: "Run the bluos-knob daemon on the USB-connected M5 ESP32 (ESP32-PICO-D4), with a meaningful on-device screen while running"
**Effort**: Standard
**Evidence**: 9 current sources · 2 historical sources

## The Ask

Vaughan plugged an M5Stack ESP32 into his Mac over USB and wants it to replace the Mac as the bridge between the Anticater knob and
the NAD M33. Today a Python daemon on the Mac does that job. The device has a small screen that should show something useful while it
runs. He has said "go ahead and build it, can always adjust afterwards" (dashboard thread tmwk885d1c). This dossier sets out what the
plan has to decide.

## Answer

1. The Python daemon cannot run on the ESP32. It has to be **reimplemented as firmware**, but it is small: one loop that decodes a
   16-bit consumer usage from each report, maps it to five actions, and makes plain HTTP GETs to BluOS (F-02, F-03, F-04).
2. The knob is a **BLE HID keyboard** (F-01). The ESP32-PICO-D4 has to act as a **BLE HID host (HOGP central)**: scan, bond, subscribe
   to Report notifications. This is the main technical risk (R-1).
3. Safety rules must be carried over exactly: increases are refused past **max −24 dB**, the source-switch volume is capped at
   **≤ −24 dB (default −40)**, and live control needs explicit opt-in, with dry-run by default (F-05, F-06).
4. The board is an M5 ESP32-PICO-D4 with a screen, which means **M5StickC or M5StickC Plus** (80×160 or 135×240 panel; Atom variants
   have no screen). The exact model is unconfirmed; M5Unified detects it at runtime (R-2).
5. The knob is currently bonded to the Mac, and the Mac daemon is **running live under launchd** (F-07). The firmware cut-over must stop
   that daemon and move the knob's bond, or the two bridges will fight.

## Evidence

| ID | Finding | Evidence | Planning implication | Confidence |
|----|---------|----------|----------------------|------------|
| F-01 | Knob advertises as `ANTICATER_MINI`, BLE, minor type Keyboard, Apple-style VID 0x05AC/PID 0x022C | `system_profiler SPBluetoothDataType` (2026-11-17) | ESP32 needs a BLE HID host, not Classic BT; match by name, not VID | High |
| F-02 | Real reports are report ID 3 + little-endian usage: `03e900` up, `03ea00` down, `03e200` mute, `036f00`/`037000` brightness, `030000` release | `capture.local.jsonl` (323 rows, gitignored) | Firmware decoder = 16-bit usage scan; over BLE the payload likely omits the report ID, and pair-scanning handles both | High |
| F-03 | Decoder maps 0xE9/0xEA/0xE2/0x6F/0x70 → up/down/mute/brightness-up/brightness-down; all-zero = release | `src/bluos_knob/device_input/normalizer.py:7-13,62-67` | Port as a pure function and golden-test it against the same fixtures | High |
| F-04 | Action → command: ±step dB, mute toggle, brightness-down → optical (`/Play?inputTypeIndex=optical-2` + safe abs dB), brightness-up → Spotify (`/Play?url=` + safe abs dB) | `scripts/bluos_daemon.py:57-79,82-150` | Same command table in firmware; config holds step, input index, source name | High |
| F-05 | Volume step = read `/Volume`, refuse if `current+step > max_db` (increase only), `GET /Volume?db=<delta>` | `scripts/bluos_volume_step.py:32-50` | Core safety check; needs a host-side unit test in the firmware project | High |
| F-06 | Live mode requires `--i-understand-this-controls-the-amplifier`; `source-safe-db` must be ≤ −24 | `scripts/bluos_daemon.py:452-456`; `scripts/bluos_source.py:126-140` | Firmware ships dry-run by default; live is a stored, explicit setting | High |
| F-07 | Mac daemon running: launchd `com.vaughanknight.bluos-knob` pid 28333 | `launchctl list`; `deploy/com.vaughanknight.bluos-knob.plist` | Cut-over step: unload plist, unpair knob from Mac, bond to ESP32; rollback is the reverse | High |
| F-08 | Board: ESP32-PICO-D4 rev 1.1, 4 MB embedded flash, USB serial "M5stack" at `/dev/cu.usbserial-89521F7539`; `esptool` 5.1 installed, PlatformIO absent | `esptool chip-id` (read-only, 2026-11-17); `which pio` | Toolchain: PlatformIO installed project-local via `uv`/`pipx` (no global installs) | High |
| F-09 | Constitution: adapter boundaries, fakes not mocks, deterministic policy tests, config not constants, no committed secrets | `docs/project-rules/constitution.md` (Principles I–V, Testing Doubles) | Firmware splits into decoder/policy (host-testable C++) vs BLE/WiFi/HTTP/display adapters; Wi-Fi credentials provisioned over serial into NVS | High |

## Historical Evidence

| ID | Prior friction / decision | Source | Applicability now | Implication |
|----|---------------------------|--------|-------------------|-------------|
| H-01 | Keep events transport-neutral (`KnobTurned`, `KnobPressed`, `TransportState`); BLE callbacks only enqueue; bounded queues | `docs/plans/001-bluetooth-input-baseline/external-research/esp32-battery-bluetooth-path.md#Migration Contract` | Direct | BLE notify callback → FreeRTOS queue → app task; never HTTP inside the callback |
| H-02 | Daemon leaked native HID handles on reopen; fixed by cached-path reopen and an RSS watchdog | `scripts/bluos_daemon.py:288-315,355-374` (commit 960d11c) | Partial | Firmware: one long-lived BLE connection, reconnect with backoff, heap watermark on serial |

## Risks and Unknowns

| Item | Evidence | Why it matters | Resolution / next evidence |
|------|----------|----------------|----------------------------|
| R-1 BLE HID host on Arduino/NimBLE with this knob (bonding/encryption, Report Map, notify subscription) | F-01; no on-device evidence yet | No events, no product | Phase 1 spike: bond, log raw notifications to serial and screen before anything else |
| R-2 Exact M5 model (StickC 80×160 vs StickC Plus 135×240) | F-08 | Screen layout depends on it | M5Unified auto-detect; log the board at boot; layout scales to both |
| R-3 Two bridges at once (Mac daemon + ESP32) could double-step the amp | F-07 | Safety: unexpected volume jumps | Cut-over checklist; ESP32 stays dry-run until the Mac daemon is unloaded |
| R-4 Knob bonds to one host at a time | F-01 | Pairing UX / rollback | Document re-pair steps both ways |
| R-5 Wi-Fi credentials | F-09; Vaughan chose a CLI he runs himself (thread tmwk885d1c, 05:13:50Z) | Must never be committed or logged | `just fw-wifi`: hidden terminal prompt → serial command → NVS on device |

## Domain Impact

| Domain / boundary | Relationship | Contract or constraint | Evidence |
|-------------------|--------------|------------------------|----------|
| device-input | Reimplemented in C++ | Same usage → action table and release detection | F-03 |
| volume-policy | Reimplemented in C++ | max-dB increase refusal; safe source dB ≤ −24 | F-05, F-06 |
| amplifier-control | New HTTP adapter | Same BluOS endpoints (`/Volume`, `/Play`, `/Status`) | F-04 |
| platform-adapter | New BLE HID host adapter | Replaces hidapi | F-01 |
| (new) display | New | Read-only view of state; never drives control | Ask |

## Planning Handoff

- **Preserve**: usage table + release rule (F-03); increase-only max-dB refusal (F-05); safe source dB ≤ −24 and dry-run default (F-06);
  fixtures as golden tests; fakes-not-mocks (F-09).
- **Change carefully**: the cut-over from the Mac daemon (F-07, R-3); BLE bonding (R-1).
- **Likely files/symbols**: new `firmware/` PlatformIO project (`platformio.ini`, `src/`, `lib/core` host-testable, `test/`); justfile
  recipes `fw-build`, `fw-test`, `fw-flash`, `fw-monitor`; README section; `docs/how/esp32-bridge.md`.
- **Decisions still required**: Arduino+NimBLE vs ESP-IDF `esp_hidh`; whether the Mac daemon remains as a documented fallback.

## External Research

| Question | Why repo evidence is insufficient | Planning impact | Prompt |
|----------|-----------------------------------|-----------------|--------|
| Most reliable BLE HID host (HOGP) stack for ESP32 Arduino in 2026: NimBLE-Arduino client vs ESP-IDF `esp_hidh` under arduino-esp32 3.x | Repo has no firmware; library behaviour is external | Picks the Phase 1 stack | "For an ESP32-PICO-D4 (M5StickC) acting as BLE HID host for a BLE consumer-control knob, compare NimBLE-Arduino 2.x client (security/bonding, subscribing to 0x2A4D reports) with ESP-IDF esp_hidh via arduino-esp32 3.x in PlatformIO; RAM use alongside Wi-Fi + M5Unified; known pairing pitfalls." |
