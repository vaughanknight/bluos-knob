# Run the knob bridge on an M5StickC Plus (ESP32)

The firmware in `firmware/` does the same job as the Mac daemon, but on a small
USB-powered M5StickC Plus. It connects to the Anticater knob over BLE, joins
Wi-Fi, reads the amp over the BluOS HTTP API and sends guarded volume, mute and
source commands. It shows what's happening on its screen.

## What you need

- An M5StickC Plus (ESP32-PICO-D4). The original StickC also works; the layout scales down.
- The Anticater knob (`ANTICATER_MINI`, a BLE HID device).
- A **2.4 GHz** Wi-Fi network on the same LAN as the amp. The ESP32 can't use
  5 GHz: a 5 GHz-only SSID shows *"Wi-Fi network not found (2.4 GHz only)"*.
- PlatformIO on the Mac: `brew install platformio`.

## Build, test, flash

```bash
just fw-test      # host-native unit tests (no hardware)
just fw-build     # firmware image
just fw-flash     # flash over USB (FW_PORT=/dev/cu.usbserial-... to pin the port)
just fw-monitor   # serial console (Ctrl-C to exit)
```

> Only one program can use the USB serial port at a time. Close `fw-monitor`
> before `fw-flash` or `fw-wifi`. **Opening the port restarts the device.**

## Pair the knob

1. On the Mac: **System Settings → Bluetooth → ANTICATER_MINI → ⓘ → Forget This
   Device** (and *Ignore* if offered). The knob remembers the Mac and goes back to
   it whenever the Mac will take it.
2. Turn the knob. The device scans for `ANTICATER` by name, bonds, and the
   **Knob** dot turns green.
3. After that it reconnects on its own: after a restart, when the knob wakes, or
   when the knob comes back into range.

If the dot stays yellow, check `system_profiler SPBluetoothDataType | grep -A3
ANTICATER`. If the Mac lists it as *Connected*, the Mac has it again. Forget it
there again. On the device's serial console, `scan` lists nearby BLE advertisers and
`bonds clear` forgets the knob so it can be paired fresh.

## Set up Wi-Fi

```bash
just fw-wifi
```

It asks for the Wi-Fi name, the password (hidden as you type) and the amp IP,
saves them on the device (NVS), restarts it, and reports whether it joined and
can reach the amp. The password is never printed (`show` displays `****`) and
never stored in the repo. `wifi scan` on the serial console lists the 2.4 GHz
networks the device can see.

## The screen

| Area | Meaning |
|---|---|
| **Wi-Fi** dot (top left) | green connected · yellow connecting · red not set up |
| **DRY-RUN / LIVE** badge | yellow: the amp is never changed · red: knob controls the amp |
| **Knob** dot (top right) | green connected · yellow looking for it · red disconnected |
| Big number | the amp's current volume in dB (read every 2 s) |
| Line under it | the current source (e.g. "Optical 1") |
| Bottom line | plain-words faults: *Wi-Fi network not found*, *can't reach amp …*, *knob not connected* |

After a knob turn it shows the target dB with a bar up to the cap. At the cap it
shows a red **MAX −24 dB** and does not change the amp. Mute and source buttons
show MUTED/UNMUTED and Optical/Spotify. The screen dims after 30 s and wakes on
a knob turn or when the volume changes elsewhere.

## Going live

A freshly flashed device is in **DRY-RUN**: it reads the amp and runs every
safety check, but sends nothing. To let the knob control the amp:

```bash
just fw-live      # asks you to type: I understand this controls the amplifier
just fw-dry-run   # stop controlling the amp
just fw-status    # current state as JSON
```

(Or on the serial console: `live on I understand this controls the amplifier` /
`live off`.) The mode is **remembered across restarts** until you switch it back.
The badge always shows which mode it's in.

## Safety rules (same as the Mac daemon, tightened)

- Every volume step and mute toggle starts with a fresh read of the amp. If the
  volume can't be read, nothing is sent.
- A step that would go above `max_db` (−24 dB, can't be set higher) is refused.
  Turning down always works.
- A source switch never raises the volume. It lowers to `source_safe_db` (−40 dB,
  ≤ −24 enforced) first if louder, then switches.
- A command whose reply is lost is **not retried** (it may have applied).
- Knob gestures older than 1 s, or queued before a live/dry-run or knob-link
  change, are dropped.

## Serial console

| Command | Does |
|---|---|
| `status` | one JSON line: knob, Wi-Fi, amp, live, faults, heap |
| `show` | settings (password masked) |
| `set <key> <value>` | change a setting (`wifi_ssid`, `wifi_pass`, `host`, `port`, `max_db`, `source_safe_db`, `step_db`, `optical_input`, `spotify_source`); unsafe values are refused; takes effect after `reboot` |
| `reboot` | restart (keeps the remembered LIVE/DRY-RUN mode) |
| `live on <phrase>` / `live off` | see *Going live* |
| `scan` · `bonds clear` | BLE diagnostics / forget the knob |
| `wifi scan` | list visible 2.4 GHz networks |

## Cut over from the Mac daemon

1. Stop the Mac daemon and keep it off after reboots:
   ```bash
   launchctl bootout gui/$(id -u)/com.vaughanknight.bluos-knob
   launchctl disable gui/$(id -u)/com.vaughanknight.bluos-knob
   ```
2. Forget the knob on the Mac (above) and pair it to the device.
3. Check DRY-RUN behaviour: turn, mute and both source buttons show the expected
   result, and the amp doesn't move.
4. `just fw-live`. Turn down, then up towards the cap and confirm **MAX**.

## Roll back to the Mac daemon

1. `just fw-dry-run` (or unplug the device).
2. Re-pair the knob with the Mac (Bluetooth settings; un-ignore it if needed).
3. Re-enable the daemon:
   ```bash
   launchctl enable gui/$(id -u)/com.vaughanknight.bluos-knob
   launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.vaughanknight.bluos-knob.plist
   ```
