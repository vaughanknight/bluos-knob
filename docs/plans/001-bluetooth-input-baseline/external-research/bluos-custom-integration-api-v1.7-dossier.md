# BluOS Custom Integration API v1.7 Dossier

**Source**: <https://bluos.io/wp-content/uploads/2025/06/BluOS-Custom-Integration-API_v1.7.pdf>  
**Extracted**: 2026-06-05  
**Purpose**: Capture the BluOS/NAD integration facts that matter for the next
BluOS Knob phase: safely mapping Anticater knob events to NAD M33 volume
control.

The PDF itself was downloaded to session scratch for text extraction and is not
committed to the repository.

## Executive Summary

BluOS exposes a simple local HTTP/XML custom-integration API. For BluOS Knob,
the next implementation should use the local player endpoint on port `11000`,
query `/Status` or `/SyncStatus` for current volume/mute/group state, and issue
bounded relative dB changes through `/Volume?db=<delta>`. Volume control should
be implemented behind a fake-backed `amplifier-control` adapter before any live
NAD M33 calls.

Key implications:

1. **Use dB deltas for knob turns**: `/Volume?db=2` and `/Volume?db=-2` are the
   natural mapping for rotate up/down.
2. **Preserve safety clamps in our policy**: BluOS constrains commands to the
   configured player volume range, but BluOS Knob should still enforce its own
   max-volume and step policy before sending requests.
3. **Poll carefully**: status supports long polling; non-long-poll clients should
   avoid frequent polling.
4. **Grouped players matter**: `tell_slaves` controls whether volume changes
   apply only to the selected player or to grouped secondaries.
5. **XML parsing is required**: responses are UTF-8 XML, not JSON.

## Transport and Discovery

| Fact | Evidence | Design Consequence |
|---|---|---|
| Requests are HTTP GET with URL-encoded parameters and XML responses. | PDF text lines 298-300. | Implement a small HTTP/XML client; do not assume JSON. |
| Standard request form is `http://<player_ip>:<port>/<request>`. | Lines 302-309. | Config should include host and port, with discovery later. |
| Standard BluOS port is `11000`; CI580 has node-specific ports. | Lines 304-308. | Default NAD M33 port can be `11000`, but keep port configurable. |
| mDNS services and LSDP can discover ports; LSDP uses UDP port `11430`. | Lines 307-308 and 2614-2627. | Phase 1 can allow manual host config; later phase can add discovery. |

## Status Model

### `/Status`

`/Status` reports playback and volume state and supports long polling:

```text
/Status?timeout=seconds&etag=etag-value
```

Useful fields for this project:

| Field | Meaning | Evidence |
|---|---|---|
| `etag` | Opaque change token for long polling. | Lines 436-440. |
| `volume` | 0..100 volume level; example response includes `<volume>4</volume>`. | Lines 425 and 795. |
| `db` | Volume level in dB. | Lines 468 and 787. |
| `mute` | Mute state; `1` if muted. | Lines 481 and 789. |
| `muteDb`, `muteVolume` | Previous unmuted level while muted. | Lines 483-487 and 791-793. |
| `syncStat` | Indicates `/SyncStatus` changed. | Lines 331-333 and 564-565. |
| playback fields | `state`, track metadata, seek capability, queue ids. | Lines 344-345 and 365-429. |

Polling rules matter: the API supports regular polling and long polling; clients
using regular polling should restrict frequency, while long-poll clients should
not hammer the same resource faster than the documented interval rules (lines
320-329).

### `/SyncStatus`

`/SyncStatus` is the better fit when only player name, volume, mute, and grouping
state are needed. The PDF explicitly says `/SyncStatus` should be polled if only
name, volume, and grouping status are of interest, and `/Status` if playback
status is needed (lines 331-334).

Useful fields:

| Field | Meaning | Evidence |
|---|---|---|
| `volume` | Player volume 0..100; `-1` means fixed volume. | Lines 710-711. |
| `db` | Volume level in dB. | Line 658. |
| `mute` | Mute state. | Line 685. |
| `master` / `slave` | Group relationships and ports. | Lines 632-637 and 674-704. |
| `syncStat` | Changes when any `/SyncStatus` item changes; matches `/Status` syncStat. | Lines 706-708. |

Grouped-player behavior is important: secondary `/Status` responses copy the
primary player, so `/SyncStatus` long polling is needed to track secondary
volume (lines 336-338).

## Volume and Mute Control

### Endpoint

```text
/Volume
/Volume?level=<0..100>&tell_slaves=<0|1>
/Volume?mute=<0|1>&tell_slaves=<0|1>
/Volume?abs_db=<db>&tell_slaves=<0|1>
/Volume?db=<delta-db>&tell_slaves=<0|1>
```

Important semantics:

| Command | Meaning | Evidence |
|---|---|---|
| `/Volume?db=2` | Increase volume by 2 dB. | Lines 811-848. |
| `/Volume?db=-2` | Decrease volume by 2 dB. | Lines 850-882. |
| `/Volume?mute=1` | Mute. | Lines 884-923. |
| `/Volume?mute=0` | Unmute. | Lines 924-955. |
| `tell_slaves=1` | Apply grouped volume change to all players in group. | Lines 762-766 and example lines 803-805. |
| `level=0..100` | Absolute percentage-like level. | Lines 752 and 762. |
| `abs_db=<db>` | Absolute dB level. | Lines 756 and 770. |

The document states command variants are constrained to values within the
configured available volume range, typically `-80..0` dB, and that range is
adjustable in the BluOS Controller app (lines 742-744). BluOS Knob should still
apply its own safety policy before sending a command.

### Recommended BluOS Knob Policy

| Knob event | First BluOS intent | Notes |
|---|---|---|
| `volume_up` | `/Volume?db=<safe_step_db>` | Start with config default `2.0 dB`, clamp by local max. |
| `volume_down` | `/Volume?db=-<safe_step_db>` | Always allowed unless adapter unreachable. |
| `mute_toggle` | Query current mute, then send `/Volume?mute=0|1`. | Avoid blind toggles because API has explicit on/off, not toggle. |
| `brightness_up/down` | No BluOS command in first amplifier phase. | These are Anticater LED/device gestures, not amplifier controls. |
| `no_op` | No command. | Release report from real Anticater capture. |

## Other Capabilities We Could Use Later

| Capability | Endpoint(s) | Why it might matter |
|---|---|---|
| Playback control | `/Play`, `/Pause`, `/Stop`, skip/back, shuffle/repeat | Could map extra knob gestures later, but keep out of first volume phase. |
| Presets | `/Presets`, `/Preset?id=<id|-1|+1>` | Could make long-click or double-click cycle presets after volume is safe. |
| Direct input selection | `/RadioBrowse?service=Capture`, `/Play?inputTypeIndex=<type-index>` | NAD M33 input selection could be useful later. |
| Bluetooth mode | `/audiomodes?bluetoothAutoplay=<0..3>` | Could configure player Bluetooth mode, but this is not needed for volume control. |
| Doorbell chime | `/Doorbell?play=1` | Out of scope for knob volume; avoid. |
| Reboot | player reboot endpoint | Dangerous side effect; exclude unless explicit maintenance mode exists. |

Input selection deserves a later dedicated design: firmware newer than v4.2.0
uses `/Play?inputTypeIndex=$typeIndex`, with types including `spdif`, `analog`,
`coax`, `bluetooth`, `arc`, `earc`, `phono`, `computer`, `aesebu`, and
`balanced` (lines 2526-2572).

Live NAD M33 discovery showed Spotify appears as a CloudService source with
`URL="Spotify%3Aplay"`, and status after invoking it reports `service=Spotify`
with `inputTypeIndex=null`. Therefore Spotify should be modeled as "resume
Spotify", not as passive source selection.

## Test and Harness Implications

The next phase should add fake-backed BluOS tests before live calls:

1. **Fake BluOS server** returning XML for `/Status`, `/SyncStatus`, and
   `/Volume`.
2. **Adapter contract tests** for parsing `<volume db="..." mute="...">`.
3. **Policy tests** proving max-volume clamp, step size, mute behavior, and
   no-op release handling.
4. **Harness commands** such as:
   - `just bluos-status`
   - `just bluos-volume-up`
   - `just bluos-volume-down`
   - `just bluos-doctor`
5. **Manual live smoke** using configured host/port only after fake-backed checks
   pass.

## Suggested Next Plan

**Plan title**: BluOS volume adapter and safe volume policy baseline

**Scope**:

- Add configuration for BluOS host, port, step dB, max dB or max level, and
  group behavior (`tell_slaves`).
- Add `amplifier-control` fake BluOS adapter and real HTTP/XML adapter.
- Add pure `volume-policy` mapping from normalized knob events to safe BluOS
  intents.
- Add CLI smoke commands to query status and send a bounded relative dB step.
- Do not yet implement presets, input switching, Bluetooth mode changes, or
  daemon service installation.

**First acceptance criteria**:

1. Given fake `/Status` XML, adapter reads volume dB, 0..100 level, mute state,
   and etag.
2. Given `volume_up`, policy emits a bounded `db=+step` command only if it does
   not exceed configured max.
3. Given `volume_down`, policy emits `db=-step`.
4. Given `mute_toggle`, adapter queries current mute and sends explicit
   `/Volume?mute=1` or `/Volume?mute=0`.
5. Given `no_op` or brightness events, no amplifier command is emitted.
6. Given live config is absent, live BluOS commands fail with clear diagnostics
   and do not guess a host.
