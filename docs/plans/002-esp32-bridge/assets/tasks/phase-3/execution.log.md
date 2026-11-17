# Execution log — Phase 3: Guarded amp control and cut-over

## 19:13 — start
- Boot chore noop. Phase 2 review MEDIUM (stale amp state) fixed before starting, since this phase writes to the amp.

## tk-2301 RED → tk-2302 GREEN: policy + executor (TDD)
- `lib/knob_core/src/amp_policy.{h,cpp}`: `plan_command` (same table as bluos_daemon.py) + `execute()` over an `AmpTransport` interface. `test/test_amp_policy`: 11 Given-When-Then tests with a **behaviour fake** of a BluOS player (dB, mute, source, unreachable paths, lost responses; no mocks).
- RED: 11/11 failed on the stub → GREEN: `pio test -e native` **27/27**.
- Rules: fresh `/Volume` read before every step/mute (fail closed on unreadable db); increase refused past max_db (incl. externally changed volume); decreases always allowed; dry-run = reads + checks, zero writes; a lost write response → Failed, never retried; mute toggles the fresh state; stale events (≥1 s) dropped.
- **Noteworthy (deliberate tightening vs Python)**: a source switch **never raises volume**. If current > source_safe_db it sets abs_db first, *then* switches; at or below it, it only switches. Python always set abs_db, which could raise the volume, and set it after the switch.
- Fixture `tests/fixtures/bluos/radiobrowse_capture.xml` (real source list; Spotify `URL="Spotify%3Aplay"`).

## Device side (tk-2302/2303/2304)
- `src/amp_control.{h,cpp}`: HTTP `AmpTransport`; command task (queue 8, drop-newest, counted); events stale ≥1 s dropped at dequeue; **generation counter flushes the queue on live/dry-run change and on any knob-link change**; control config copied at boot (settings apply after reboot).
- Live switch: `live on I understand this controls the amplifier` (exact phrase) / `live off`; session-only. Serial check: boot → `{"live":false,"note":"dry-run at every boot"}`; `live on` and `live on yes please` refused; exact phrase → `{"live":true}`; `reboot` → dry-run again.
- Screen v3: DRY-RUN (yellow) / LIVE (red) badge; volume result = big dB + bar from −80 to max_db, red with "MAX −24 dB" when refused; MUTED/UNMUTED; Optical/Spotify; failures and "dry-run: amp not changed" on the bottom line.
- Heap with control task: free ≈79 KB, min ≈74.6 KB.
- **Discovery**: re-opening the USB serial port resets the ESP32 (macOS toggles DTR on open), so session-only live is lost on any serial connection or power blip, and going live always needs a computer. **Decision asked of Vaughan** (button hold / persist / keep).

## Owner decision: remember LIVE (tk-2304 revised)
- Vaughan 08:23:19Z: "2: remember LIVE". Recorded as a plan clarification; ac-0207 and tk-2304 updated (this supersedes plan-review fix 3, session-only live, by explicit owner decision).
- `config_store`: `live_load/live_save` (NVS key `live`, default false); `control_start` restores it; `control_set_live` persists it.
- Device: fresh boot `live:false`; `live on <phrase>` → `{"live":true,"remembered":true}` → reboot → `live:true`; `live off` → reboot → `live:false`.
- `scripts/fw_console.py` + `just fw-live` (operator types the phrase), `just fw-dry-run`, `just fw-status` (waits ~6 s after the restart for the first poll). `fw-status` → knob connected, wifi connected, amp ok −38 dB, live 0.

## tk-2305 docs: done
- `docs/how/esp32-bridge.md` runbook; README "Run it on an ESP32" section + intro no longer says "next".
