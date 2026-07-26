# Deploy

`com.vaughanknight.bluos-knob.plist` is the launchd agent that runs the daemon
under `KeepAlive` (respawns on exit) with structured logging + memory watchdog.

Paths inside are absolute (repo at `~/GitHub/bluos-knob`, logs at
`~/Library/Logs/bluos-knob/`); adjust if yours differ.

## Install / reload

Copy into place, then reload. A bare `kill` won't stick because `KeepAlive`
respawns it — you must `bootout` to actually stop it:

```bash
cp deploy/com.vaughanknight.bluos-knob.plist ~/Library/LaunchAgents/

# Stop (if already loaded)
launchctl bootout gui/$(id -u)/com.vaughanknight.bluos-knob

# Start / reload after editing the plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.vaughanknight.bluos-knob.plist
```

## Notes

- Structured JSON logs go to `~/Library/Logs/bluos-knob/daemon.log` (size-rotated,
  5 MB × 3 backups). launchd's own stdout/stderr go to `daemon.out.log` /
  `daemon.err.log`.
- The daemon exits (for `KeepAlive` to respawn clean) if its RSS reaches
  `--max-rss-mb` (default 512), so any residual native leak self-recovers.
