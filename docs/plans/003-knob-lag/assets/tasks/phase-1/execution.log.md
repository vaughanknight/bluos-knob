# Execution log — plan 003 knob lag

## 09:08 — start (Vaughan approved on tmwk885d1c 22:00Z)
- Harness chores noop (repo not adopted).

## tk-3101/3102 coalescer + clamped net step (TDD)
- RED: `given_click_bursts…` and `given_net_burst_past_cap…` failed on stubs; existing near-cap test updated (−24.5 +1 now stops at −24 instead of refusing; **Noteworthy** semantic change, safer-or-equal: never above max_db).
- GREEN: `coalesce()` sums runs of up/down into one VolumeStep (zero net → nothing), mute/source are ordered barriers; `execute` clamps an increase to max_db (refused only when already at/over it). 33/33.
- **Kept** the fresh /Volume read per command (plan 002 safety basis); coalescing makes it one read + one write per burst.

## tk-3103 control task + transport
- Control task drains the whole queue, drops stale/superseded clicks, coalesces, executes in order; serial line adds `clicks`.
- Transport: one `HTTPClient` + `WiFiClient` with `setReuse(true)` (keep-alive); a failed **read** is retried once on a fresh connection (stale idle socket); writes are never retried. Queue depth 8→32 (memory bound only).
- `control_busy()` = executing or queued.

## tk-3104 idle-only screen
- `ui_render` runs only when the knob event queue is empty and `!control_busy()`.
