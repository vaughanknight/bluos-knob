#!/usr/bin/env python3
"""Send one command to the BluOS Knob ESP32 over USB serial and print its replies.

Opening the port restarts the device; the script waits for it to boot first.
`live` asks you to type the confirmation phrase yourself (the device checks it).
"""

from __future__ import annotations

import argparse
import glob
import json
import sys
import time

import serial  # pyserial (ships with PlatformIO)

PHRASE = "I understand this controls the amplifier"


def pick_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    ports = sorted(glob.glob("/dev/cu.usbserial-*")) + sorted(glob.glob("/dev/ttyUSB*"))
    if len(ports) != 1:
        sys.exit("Pass --port (found: " + (", ".join(ports) or "none") + ").")
    return ports[0]


def lines(link: serial.Serial, seconds: float):
    end = time.time() + seconds
    while time.time() < end:
        raw = link.readline().decode(errors="replace").strip()
        if raw.startswith("{"):
            try:
                yield json.loads(raw)
            except json.JSONDecodeError:
                continue


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["status", "live", "dry-run"])
    parser.add_argument("--port")
    args = parser.parse_args()

    command = {"status": "status", "dry-run": "live off"}.get(args.action)
    if args.action == "live":
        print("This lets the knob change the amp's volume, mute and source,")
        print("and it stays LIVE across restarts until `just fw-dry-run`.")
        typed = input(f"Type exactly: {PHRASE}\n> ")
        command = f"live on {typed}"

    try:
        link = serial.Serial(pick_port(args.port), 115200, timeout=0.3)
    except serial.SerialException as exc:
        sys.exit(f"Can't open the port ({exc}). Close any serial monitor and retry.")
    link.dtr = False
    link.rts = False
    for message in lines(link, 8):
        if "live" in message and "note" in message:
            break
    if args.action == "status":
        for _ in lines(link, 6):  # let Wi-Fi join and the first amp poll land after the restart
            pass
    link.write((command + "\n").encode())
    for message in lines(link, 3):
        if args.action == "status" and "board" in message:
            print(json.dumps(message, indent=2))
            return 0
        if args.action != "status" and "live" in message and "note" not in message:
            if message.get("error"):
                print(f"Refused: {message['error']}")
                return 1
            state = "LIVE" if message.get("live") else "DRY-RUN"
            if message.get("remembered") is False:
                print(f"WARNING: device is {state} now but the setting was NOT saved.")
                print("It will come back in its previous mode after a restart. Run this again.")
                return 1
            print(f"Device is now {state} (remembered across restarts).")
            return 0
    print("No reply from the device.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
