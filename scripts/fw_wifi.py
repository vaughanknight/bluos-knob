#!/usr/bin/env python3
"""Provision Wi-Fi and amp settings onto the BluOS Knob ESP32 over USB serial.

The password is read with getpass (not echoed), sent once over the local USB
serial link, and never printed: the firmware stores it in NVS and only ever
reports it masked.
"""

from __future__ import annotations

import argparse
import getpass
import glob
import json
import sys
import time

import serial  # pyserial (ships with PlatformIO)

BAUD = 115200


def pick_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    ports = sorted(glob.glob("/dev/cu.usbserial-*")) + sorted(glob.glob("/dev/ttyUSB*"))
    if not ports:
        sys.exit("No ESP32 serial port found. Plug the device in or pass --port.")
    if len(ports) > 1:
        sys.exit(f"Several serial ports found ({', '.join(ports)}); pass --port.")
    return ports[0]


def read_json_lines(link: serial.Serial, seconds: float):
    end = time.time() + seconds
    while time.time() < end:
        raw = link.readline()
        if not raw:
            continue
        text = raw.decode(errors="replace").strip()
        if text.startswith("{"):
            try:
                yield json.loads(text)
            except json.JSONDecodeError:
                continue


def wait_for(link: serial.Serial, predicate, seconds: float) -> dict | None:
    for message in read_json_lines(link, seconds):
        if predicate(message):
            return message
    return None


def send(link: serial.Serial, line: str) -> None:
    link.write((line + "\n").encode())
    link.flush()


def set_value(link: serial.Serial, key: str, value: str) -> None:
    send(link, f"set {key} {value}")
    reply = wait_for(link, lambda m: m.get("set") == key, 5)
    if reply is None:
        sys.exit(f"No reply from the device when setting {key}.")
    if not reply.get("ok"):
        sys.exit(f"Device refused {key}: {reply.get('error')}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port (default: auto-detect)")
    parser.add_argument("--host", help="BluOS amp IP (default: prompt)")
    args = parser.parse_args()

    port = pick_port(args.port)
    try:
        link = serial.Serial(port, BAUD, timeout=0.3)
    except serial.SerialException as exc:
        sys.exit(f"Can't open {port} ({exc}). Close any serial monitor/logger and retry.")
    link.dtr = False
    link.rts = False

    print(f"Connected to {port}; waiting for the device to boot...")
    if wait_for(link, lambda m: "boot" in m, 8) is None:
        send(link, "show")
    send(link, "show")
    current = wait_for(link, lambda m: "wifi_ssid" in m, 5) or {}

    stored_ssid = current.get("wifi_ssid", "")
    ssid = input(f"Wi-Fi name [{stored_ssid}]: ").strip() or stored_ssid
    if not ssid:
        sys.exit("A Wi-Fi name is required.")
    password = getpass.getpass("Wi-Fi password (hidden; Enter keeps the stored one): ")
    default_host = args.host or current.get("host") or "192.168.1.67"
    host = input(f"Amp IP address [{default_host}]: ").strip() or default_host

    set_value(link, "wifi_ssid", ssid)
    if password:
        set_value(link, "wifi_pass", password)
    set_value(link, "host", host)
    del password

    print("Saved. Restarting the device and joining Wi-Fi...")
    send(link, "reboot")
    connected = wait_for(
        link, lambda m: m.get("wifi") == "connected" or m.get("config") == "invalid", 30
    )
    if connected is None:
        print("Didn't see a Wi-Fi connection within 30 s. Check the name/password and run again.")
        return 1
    if connected.get("config") == "invalid":
        print(f"Device says the settings are incomplete: {connected.get('error')}")
        return 1
    print(f"On Wi-Fi as {connected.get('ip')} (signal {connected.get('rssi')} dBm).")
    status = None
    for _ in range(5):  # the first amp poll can take a few seconds after Wi-Fi joins
        send(link, "status")
        status = wait_for(link, lambda m: "board" in m and "amp" in m, 2.5)
        if status and status.get("amp") == "ok":
            break
    if status and status.get("amp") == "ok":
        print(f"Amp {host} reachable: {status.get('db')} dB, source '{status.get('source')}'.")
    else:
        fault = status.get("fault") if status else "no status from the device"
        print(f"Couldn't confirm the amp at {host} yet: {fault}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except serial.SerialException as exc:
        sys.exit(
            f"Lost the USB serial link ({exc}). Is something else using the port? Re-run fw-wifi."
        )
