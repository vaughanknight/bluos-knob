# Execution Log — Simple Implementation

**Plan**: `docs/plans/001-bluetooth-input-baseline/bluetooth-input-baseline-plan.md`  
**Started**: 2026-06-05T03:23:56Z  
**Completed**: 2026-06-05T03:33:51Z

## Agent Harness Validation

This phase establishes the first engineering harness, so pre-phase harness
validation was not available at task start. End-of-phase commands now provide the
Boot → Interact → Observe loop:

| Stage | Status | Evidence |
|---|---|---|
| Boot | PASS | `python3 -m bluos_knob.cli.main --help` |
| Interact | PASS | `python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl` |
| Observe | PASS | `python3 -m pytest`, `python3 -m ruff check .`, `python3 scripts/check_boundaries.py` |

## Task Entries

| Task | Status | Evidence |
|---|---|---|
| T001 | Complete | Created `docs/domains/registry.md`, `docs/domains/domain-map.md`, and six domain docs. |
| T002 | Complete | Created `.gitignore`, `pyproject.toml`, `justfile`, and `docs/project-rules/engineering-harness.md`. |
| T003 | Complete | Created `src/bluos_knob` package and domain package boundaries. |
| T004 | Complete | Added HID contracts, JSONL fixture schema, redaction helper, fixture docs, and schema tests. |
| T005 | Complete | Added fixture-backed fake HID platform/reader, sample JSONL reports, and fake-reader tests. |
| T006 | Complete | Added shared normalizer for volume, mute, brightness, no-op release, and unknown reports with tests. |
| T007 | Complete | Added Anticater profile filtering without local path/serial coupling and tests. |
| T008 | Complete | Added lazy HIDAPI platform adapter with enumerate/open/read boundaries and capability metadata. |
| T009 | Complete | Added `list`, `capture`, and `replay` CLI commands plus fake-backed integration tests. |
| T010 | Complete | Added import-boundary checker and wired it into `just lint`. |
| T011 | Complete | Added README and `docs/how/` workflow, fixture, and HID capability docs. |
| T012 | Complete with pending human evidence | Added smoke evidence template and explicitly recorded that real Anticater capture is not yet present. |

## Validation Evidence

```text
python3 -m pip install -e ".[dev]"  # PASS
python3 -m pytest                   # 23 passed after review fixes
python3 -m ruff check .             # All checks passed
python3 scripts/check_boundaries.py # Domain boundary check passed
python3 -m bluos_knob.cli.main --help
python3 -m bluos_knob.cli.main replay --fixture tests/fixtures/anticater/sample_reports.jsonl
just smoke                          # PASS after switching recipes to python3
git diff --check                    # PASS after doctrine whitespace cleanup
```

## Discoveries & Learnings

| ID | Discovery | Action |
|---|---|---|
| D001 | CLI `list` and `capture` require a fake platform adapter seam, not only a fake report reader. | Added `HidPlatform` protocol and `FakeHidPlatform` for deterministic CLI tests. |
| D002 | Transport-neutral reuse needs schema version, sequence, transport, connection state, and raw correlation. | Added these fields to fixture and normalized event contracts. |
| D003 | Real-device smoke evidence cannot be fabricated without the physical Anticater being exercised. | Added a smoke template and left real evidence pending. |
| D004 | Review caught that treating HIDAPI byte 0 as a report ID could corrupt unnumbered reports. | Changed the baseline adapter to preserve the full HID byte stream with `report_id=0` until descriptor evidence proves otherwise. |
| D005 | Running the real `list` command exposed missing HIDAPI setup in the local environment. | Promoted HID setup into harness commands: `just setup-hid`, `just doctor-hid`, and `just list-devices`. |
| D006 | Live Anticater capture showed `030000` is the release/no-op report after actions. | Updated the normalizer and tests so report-ID-prefixed zero payloads map to `no_op`, not `unknown`. |
| D007 | BluOS Custom Integration API v1.7 is enough to design the next amplifier-control phase. | Added an implementation-focused dossier covering HTTP/XML, port 11000, status, dB volume, mute, grouping, input selection, discovery, and testing implications. |
| D008 | NAD M33 is reachable at `192.168.1.67:11000`, but live volume mutation must wait for safety policy. | Added read-only BluOS harness commands only: doctor, status, syncstatus, and current volume. |
| D009 | Guarded live BluOS mutation works with a `-24 dB` maximum and explicit acknowledgement. | Added fake-tested `bluos-step-down` and `bluos-step-up` harness commands; verified -1 dB then +1 dB returned the NAD to `-48 dB`. |
| D010 | BluOS mute is explicit on/off, and mute-on reports `mute=1`, `volume=0`, `db=-100`. | Added fake-tested `bluos-mute-off` and `bluos-mute-on` harness commands; verified off then on against the NAD. |
| D011 | BluOS exposes Optical 1 as a direct input and Spotify as a service/play context. | Added guarded source commands and mapped `brightness_down` to Optical 1, `brightness_up` to resume Spotify. |
| D012 | Optical 1 should use `/Play?inputTypeIndex=optical-2`, not the generic browse URL, to restore reliably after Spotify. | Updated `bluos-source-optical` to use `inputTypeIndex=optical-2`; Spotify keeps the discovered `Spotify%3Aplay` URL. |
| D013 | Spotify cannot be selected through `inputTypeIndex`; live status reports `service=Spotify` and `inputTypeIndex=null`. | Treat the Spotify shortcut as "resume Spotify", not passive source selection. |
| D014 | Source switching should set a safe absolute volume because different inputs can be unexpectedly loud. | Updated Optical and Spotify source-switch commands to set `abs_db=-40` after switching and reject targets louder than `-24 dB`. |

## Review Fixes

| Time | Fix | Evidence |
|---|---|---|
| 2026-06-05T03:41:54Z | Preserved full bytes returned by HIDAPI, changed just recipes/harness docs to `python3`, hardened fixture schema errors, and removed trailing whitespace. | `python3 -m pytest` → 22 passed; `python3 -m ruff check .`; `python3 scripts/check_boundaries.py`; `just smoke`; `git diff --check`. |
| 2026-06-05T03:43:48Z | Required explicit `transport` and `connection_state` fixture fields and added regression coverage. | `python3 -m pytest` → 23 passed; `python3 -m ruff check .`; `python3 scripts/check_boundaries.py`; `just smoke`; `git diff --check`. |
| 2026-06-05T03:49:40Z | Promoted HIDAPI installation and real listing into the engineering harness. | `just setup-hid`; `just doctor-hid` → HIDAPI import OK and 70 HID devices visible; `just list-devices` → Anticater candidates found; standard tests/lint/boundaries/smoke/diff-check passed. |
| 2026-06-05T04:02:00Z | Captured 323 live Anticater reports while dialing/clicking the knob. | Raw counts: `030000` x221, `03ea00` x36, `03e900` x30, `036f00` x16, `037000` x12, `03e200` x8. |
| 2026-06-05T04:08:00Z | Researched BluOS Custom Integration API v1.7 PDF for next amplifier-control phase. | Dossier written to `external-research/bluos-custom-integration-api-v1.7-dossier.md`; source PDF retained only in session scratch. |
| 2026-06-05T04:58:16Z | Added and validated safe read-only BluOS harness. | `just bluos-doctor/status/syncstatus/volume 192.168.1.67` all passed; NAD M33 `Lounge Music`, volume `54`, dB `-48`, mute `0`; no volume up/down commands added. |
| 2026-06-05T05:12:28Z | Tested guarded live BluOS volume mutation with max `-24 dB`. | Start `-48 dB`/volume `54`; step down `-1 dB` -> `-49 dB`/volume `53`; step up `+1 dB` -> `-48 dB`/volume `54`. |
| 2026-06-05T05:13:08Z | Re-ran guarded live BluOS volume test with explicit intermediate readback. | Before `-48 dB`/volume `54`; after down readback `-49 dB`/volume `53`; final readback after up `-48 dB`/volume `54`. |
| 2026-06-05T05:16:03Z | Tested guarded live BluOS mute off then mute on. | Before `mute=0`, `-48 dB`, volume `54`; mute-off remained `mute=0`; mute-on readback `mute=1`, `-100 dB`, volume `0`. |
| 2026-06-05T05:18:00Z | Discovered BluOS sources for quick-access mapping. | `/RadioBrowse?service=Capture` includes Optical 1 URL and Spotify URL; source switching harness added for live validation. |
| 2026-06-05T05:18:00Z | Live-tested Spotify and Optical source switching. | Spotify switch changed status to `service=Spotify`, `name=Spotify`, then stream `Wormholes`; Optical restored reliably with direct `/Play?inputTypeIndex=optical-2`. |
