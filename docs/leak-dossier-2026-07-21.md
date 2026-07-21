# BluOS Knob — memory-leak dossier (2026-07-21)

**Verdict:** The `bluos_daemon.py` process leaks native (HID-layer) memory every time it
re-opens the knob's HID handle. Because it re-opens roughly **every 5–8 seconds while the knob
is idle** (which is almost always), it had grown to **~18.3 GB RSS after 14 days** and was the
`largestProcess` when a system **jetsam / out-of-memory kill fired on 2026-07-16**. It is the
biggest single contributor to the Mac Studio's intermittent whole-system hangs.

This dossier is self-contained — you can open a fresh window on this repo and start from here.

---

## 1. How it was found

Investigating machine-wide input hangs (WindowServer starving under memory + CPU contention).
Process inventory surfaced one Python process holding ~18.3 GB:

```
PID 3802  RSS 18,718,912 KB (~18.3 GB)  etime 14 days
/…/.venv/bin/python scripts/bluos_daemon.py --anticater-path auto
  --bluos-host 192.168.1.67 --max-db -24 --i-understand-this-controls-the-amplifier
```

Launched by a **launchd agent** (`KeepAlive`, so it respawns if killed):
`~/Library/LaunchAgents/com.vaughanknight.bluos-knob.plist` → logs to
`~/Library/Logs/bluos-knob/daemon.log`.

The Jul-16 jetsam report (`/Library/Logs/DiagnosticReports/JetsamEvent-2026-07-16-*.ips`)
names `"largestProcess": "Python"`.

## 2. The measurement (this is the smoking gun)

The daemon's own 37 MB log — `~/Library/Logs/bluos-knob/daemon.log`, 466,288 lines — event tally:

| Event | Count | Meaning |
|---|---:|---|
| `hid_idle_reopen` | **158,065** | idle timeout tripped → close + full reopen |
| `hid_opened` | 158,067 | a reopen succeeded |
| `hid_open_error` | **148,990** | an `open_path()` attempt threw (≈1 failure per success) |
| `started` | 2 | one KeepAlive respawn in 14 days |
| `hid_read_error` | 1 | — |

158k reopen cycles ÷ 14 days ≈ **one reopen every ~7.6 s**, all day, whether or not anyone
touches the knob. Each cycle performs at least one `hid.enumerate()` and one-or-more
`hid.device()/open_path()` (≈2 native open attempts per cycle counting the failures).

**~18.3 GB ÷ ~307k native enumerate/open operations ≈ ~60 KB leaked per operation** — exactly the
signature of an unfreed HID enumeration list / IOKit matching allocation on macOS.

## 3. Root-cause chain

1. **Idle reopen is on by default and mis-triggered.** `--idle-reopen-timeouts` defaults to `5`
   and `--timeout-ms` to `1000`. A *read timeout* on an idle HID handle is **normal**, not a
   sign the handle is wedged — but the daemon treats 5 consecutive timeouts (~5 s) as "reopen".
   The launchd plist passes **no override**, so the default is live in production.
   - `scripts/bluos_daemon.py:181` — `if config.idle_reopen_timeouts and idle_reads >= config.idle_reopen_timeouts: … reopen`
   - `scripts/bluos_daemon.py:294` (`--timeout-ms` default 1000), `:298` (`--idle-reopen-timeouts` default 5)

2. **Every reopen re-enumerates.** `reopen_idle_reader` → `open_reader_with_retry` →
   `discover_anticater_paths` → `platform.enumerate_devices()` → `hid.enumerate()`.
   - `scripts/bluos_daemon.py:206-247`
   - `src/bluos_knob/platform_adapter/hidapi_reader.py:23-29` (`enumerate`)

3. **Failed opens are half the cycles.** The knob is Bluetooth; its path churns / it's often not
   openable, so `open_path()` throws (`PlatformHidError`) 148,990 times. A failed open allocates a
   `hid.device()` then raises — the partial native handle is **not explicitly closed** on the error
   path. Prime leak site.
   - `src/bluos_knob/platform_adapter/hidapi_reader.py:31-38` (`open` — no cleanup in the `except`)

4. **The native `hidapi` layer isn't freeing per-cycle allocations** (enumeration list and/or the
   partially-opened device) on this repeated enumerate→open→close cadence. 158k cycles × 14 days
   compounds to 18 GB.

5. **Result:** slow unbounded growth → periodic system memory pressure → the Jul-16 jetsam and the
   ongoing hangs.

## 4. Fixes, ranked

**A — Immediate mitigation (stops ~99% of the churn, no code change):**
Add `--idle-reopen-timeouts 0` to the plist `ProgramArguments` (0 disables idle reopen — see
guard at `bluos_daemon.py:181`), then reload the agent. Idle read timeouts are expected; there's
no reason to reopen a healthy idle handle.

**B — Correct the wedge-detection (small code change):**
Only reopen on repeated **read *errors*** (`PlatformHidError` from `read_report`), never on plain
timeouts (`report is None`). The timeout branch should just keep polling.

**C — Stop re-enumerating on every reopen (kills the dominant leak site):**
Cache the resolved device path after the first successful `open`; on reopen, try that path
directly and only fall back to `enumerate()` if it fails. Enumeration is the expensive/leaky call.

**D — Free native handles on the failed-open path:**
In `HidapiPlatform.open`, wrap so that if `open_path` throws, the just-created `device` is closed
before re-raising. Prevents the partial-handle leak behind the 148,990 `hid_open_error`s.

**E — Handle "device absent" as a slow-poll state:**
When the knob isn't present, back off to a long sleep (e.g. 30 s) instead of the tight
reopen/enumerate loop, so a disconnected knob can't spin the CPU or leak.

**F — Housekeeping:** the log has no rotation (37 MB and growing under `KeepAlive`). Add size-based
rotation or trim on start. Consider a `MemoryLimit`/watchdog so a future leak self-recovers.

## 5. Safe stop / restart right now

The daemon only sends volume/mute/source commands to the amp — killing it changes nothing on the
amp, it just stops the knob working until relaunched. Because launchd `KeepAlive` is set, a bare
`kill` will respawn it; unload the agent to actually stop it:

```bash
# Stop and prevent respawn (frees ~18 GB immediately):
launchctl bootout gui/$(id -u)/com.vaughanknight.bluos-knob

# After applying fix A/B/C, reload:
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.vaughanknight.bluos-knob.plist
```

## 6. Open questions for the reviewer

- Which `hidapi` build is installed in `.venv` (`pip show hidapi`)? A known-good pin may already
  fix the native free behaviour — worth checking release notes for enumerate/open leak fixes.
- Is the idle-reopen feature earning its keep at all? If real BT wedges are rare, option A alone
  may be the whole fix.
- The planned ESP32 port (README) sidesteps this by not running on the Mac — but the leak should
  still be fixed for anyone running the daemon build.

---

*Investigation artifact. Code references are `file:line` at the state of this repo on 2026-07-21
(note: working tree has uncommitted edits to `scripts/bluos_daemon.py`, `run.sh`,
`tests/unit/test_bluos_daemon.py` — confirm line numbers against your buffer).*
