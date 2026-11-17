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

## tk-2306 supervised cut-over (Vaughan on the couch, 19:41–19:46)
- Mac daemon already disabled (Phase 1). Vaughan 08:41:02Z: "You run live now I'm on my phone and can test from couch".
- Temporary test cap: `set max_db -36` + reboot (so the refusal could be tested at −38 without getting loud).
- **Dry-run**: 4 turns → `control:"dry_run"`, writes 0.
- 19:41:43 `live on <phrase>` → `{"live":true,"remembered":true}`.
- **Live steps**: one write per click (−38→−39→−40→−39→−38→−37→−36).
- **Cap**: 19:41:50 `{"control":"refused","before_db":-36.0,"cap":1,"writes":0,"message":"max -36 dB"}`; decreases continued straight after. Vaughan: "It works!"
- **Mute**: 19:42:45 mute (−38, 1 write), 19:42:46 unmute (1 write). Vaughan: "Mute worked".
- Source switches (Optical/Spotify) **not exercised live** (Vaughan watching TV). **Deferred**: covered by 4 policy tests incl. lower-first/never-raise; confirm on hardware later.
- Cap restored: `set max_db -24` + reboot → `show` max_db −24.0, still LIVE (remembered).

## Safety bug found live + fixed (TDD)
- The unmute log line showed `before_db:-100.0`: **a muted BluOS player reports db="-100"**, with the real level only in `muteDb` (present only while muted; confirmed in the BluOS Custom Integration API docs + homebridge-bluos PROTOCOL.md). A volume step while muted would check the cap against −100, but BluOS applies `db=+1` relative to the real level, so it could pass max_db. **The Python daemon has the same hole** (Deferred: Mac daemon is disabled; fix there before re-enabling).
- RED: new tests (`given_muted_amp_…`, `given_muted_volume_response_…`) failed: the muted step at the real cap was accepted.
- Fix: `parse_volume` exposes `has_mute_db/mute_db`; the policy basis while muted is `muteDb`; muted without `muteDb` → Failed, no write. Mute toggle uses only the mute state, so **unmute always works**. `pio test -e native` 29/29 green; reflashed; came back LIVE and in use (19:46).

## Phase 3 review fixes (TDD) + reflash
- Increases → `abs_db=<checked target>` (race RED −21 → GREEN −24); post-switch re-read + lower (RED −20 → GREEN −40); `fmt_db` fixed-point; `live` save retried + console warns when not saved. `pio test -e native` **31/31**. Reflashed: boots LIVE (remembered), knob connected, amp ok −38 dB.

## Phase 3 complete
- Deferred for a human before relying on them: **Optical/Spotify buttons not yet exercised on the real amp** (tests only); **Python daemon has the muted −100 dB and read→relative-write gaps**, so fix it before re-enabling it.
