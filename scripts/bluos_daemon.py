#!/usr/bin/env python3
"""Live Anticater-to-BluOS daemon loop."""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from bluos_knob.device_input.contracts import NormalizedAction, NormalizedKnobEvent
from bluos_knob.device_input.normalizer import normalize_report
from bluos_knob.platform_adapter.hidapi_reader import HidapiPlatform, PlatformHidError
from scripts.bluos_mute import send_mute
from scripts.bluos_readonly import fetch_endpoint, summarize
from scripts.bluos_source import (
    fetch_sources,
    find_source,
    send_input_type_index_switch,
    send_safe_abs_db,
    send_source_switch,
)
from scripts.bluos_volume_step import assert_safe_step, parse_decimal, send_volume_step


@dataclass(frozen=True)
class DaemonConfig:
    anticater_path: str
    bluos_host: str
    bluos_port: int
    step_db: Decimal
    max_db: Decimal
    source_safe_db: str
    optical_input_type_index: str
    spotify_source_name: str
    timeout_ms: int
    request_timeout: float
    reconnect_delay: float
    dry_run: bool


def planned_command(event: NormalizedKnobEvent, config: DaemonConfig) -> dict[str, Any]:
    if event.action == NormalizedAction.VOLUME_UP:
        return {"kind": "volume_step", "step_db": str(config.step_db)}
    if event.action == NormalizedAction.VOLUME_DOWN:
        return {"kind": "volume_step", "step_db": str(-config.step_db)}
    if event.action == NormalizedAction.MUTE_TOGGLE:
        return {"kind": "mute_toggle"}
    if event.action == NormalizedAction.BRIGHTNESS_DOWN:
        return {
            "kind": "source_optical",
            "input_type_index": config.optical_input_type_index,
            "safe_db": config.source_safe_db,
        }
    if event.action == NormalizedAction.BRIGHTNESS_UP:
        return {
            "kind": "source_spotify",
            "source": config.spotify_source_name,
            "safe_db": config.source_safe_db,
        }
    return {"kind": "ignore"}


def execute_command(command: dict[str, Any], config: DaemonConfig) -> dict[str, Any]:
    if config.dry_run or command["kind"] == "ignore":
        return {"executed": False, "reason": "dry_run" if config.dry_run else "ignored"}

    if command["kind"] == "volume_step":
        step_db = Decimal(command["step_db"])
        before = _fetch_summary(config, "volume")
        current_db = parse_decimal(str(before["db"]), "current db")
        planned_after = assert_safe_step(current_db, step_db, config.max_db)
        response = send_volume_step(
            config.bluos_host, config.bluos_port, step_db, config.request_timeout
        )
        after = _fetch_summary(config, "volume")
        return {
            "executed": True,
            "before": before,
            "planned_after_db": str(planned_after),
            "response": response,
            "after": after,
        }

    if command["kind"] == "mute_toggle":
        before = _fetch_summary(config, "volume")
        state = "off" if before.get("mute") == "1" else "on"
        response = send_mute(config.bluos_host, config.bluos_port, state, config.request_timeout)
        after = _fetch_summary(config, "volume")
        return {
            "executed": True,
            "before": before,
            "requested_state": state,
            "response": response,
            "after": after,
        }

    if command["kind"] == "source_optical":
        source_response = send_input_type_index_switch(
            config.bluos_host,
            config.bluos_port,
            config.optical_input_type_index,
            config.request_timeout,
        )
        volume_response = send_safe_abs_db(
            config.bluos_host, config.bluos_port, config.source_safe_db, config.request_timeout
        )
        after = _fetch_summary(config, "status")
        return _source_result(source_response, volume_response, after)

    if command["kind"] == "source_spotify":
        sources = fetch_sources(config.bluos_host, config.bluos_port, config.request_timeout)
        source = find_source(sources, config.spotify_source_name)
        source_response = send_source_switch(
            config.bluos_host, config.bluos_port, source, config.request_timeout
        )
        volume_response = send_safe_abs_db(
            config.bluos_host, config.bluos_port, config.source_safe_db, config.request_timeout
        )
        after = _fetch_summary(config, "status")
        return _source_result(source_response, volume_response, after)

    raise SystemExit(f"Unsupported daemon command: {command['kind']}")


def execute_command_for_daemon(command: dict[str, Any], config: DaemonConfig) -> dict[str, Any]:
    try:
        return execute_command(command, config)
    except SystemExit as exc:
        return {
            "executed": False,
            "reason": "command_refused",
            "message": str(exc),
        }


def _fetch_summary(config: DaemonConfig, command: str) -> dict[str, Any]:
    return summarize(
        fetch_endpoint(config.bluos_host, config.bluos_port, command, config.request_timeout)
    )


def _source_result(
    source_response: dict[str, Any],
    volume_response: dict[str, Any],
    after: dict[str, Any],
) -> dict[str, Any]:
    return {
        "executed": True,
        "source_response": source_response,
        "safe_volume_response": volume_response,
        "after": after,
    }


def run_daemon(config: DaemonConfig, limit: int | None) -> int:
    platform = HidapiPlatform()
    reader = open_reader_with_retry(platform, config)
    seen_actions = 0
    idle_reads = 0
    try:
        _log({"daemon": "started", "dry_run": config.dry_run, "path": config.anticater_path})
        while limit is None or seen_actions < limit:
            try:
                report = reader.read_report(config.timeout_ms)
            except PlatformHidError as exc:
                reader = reopen_reader(platform, config, reader, exc)
                idle_reads = 0
                continue
            if report is None:
                idle_reads += 1
                if idle_reads % 30 == 0:
                    _log({"daemon": "idle", "timeouts": idle_reads})
                continue
            idle_reads = 0
            event = normalize_report(report)
            command = planned_command(event, config)
            result = execute_command_for_daemon(command, config)
            payload = {
                "action": event.action.value,
                "command": command,
                "raw": event.raw_data_hex,
                "result": result,
                "sequence": event.sequence,
            }
            _log(payload)
            if command["kind"] != "ignore":
                seen_actions += 1
    except KeyboardInterrupt:
        _log({"daemon": "stopped"})
    finally:
        reader.close()
    return 0


def open_reader_with_retry(platform: HidapiPlatform, config: DaemonConfig) -> Any:
    while True:
        paths = discover_anticater_paths(platform, config.anticater_path)
        for path in paths:
            try:
                reader = platform.open(path)
            except PlatformHidError as exc:
                _log({"daemon": "hid_open_error", "message": str(exc), "path": path})
                continue
            _log({"daemon": "hid_opened", "path": path})
            return reader
        time.sleep(config.reconnect_delay)


def discover_anticater_paths(platform: HidapiPlatform, configured_path: str) -> list[str]:
    paths: list[str] = []
    if configured_path != "auto":
        paths.append(configured_path)

    try:
        devices = platform.enumerate_devices()
    except PlatformHidError as exc:
        _log({"daemon": "hid_enumerate_error", "message": str(exc)})
        return paths

    candidates = []
    for device in devices:
        identity = device.identity
        product = (identity.product or "").casefold()
        manufacturer = (identity.manufacturer or "").casefold()
        if "anticater" not in product and "vk" not in product and "anticater" not in manufacturer:
            continue
        if not identity.path:
            continue
        candidates.append(device)

    candidates.sort(key=lambda device: device.identity.usage_page != 12)
    for device in candidates:
        path = device.identity.path
        if path and path not in paths:
            paths.append(path)
    return paths


def reopen_reader(
    platform: HidapiPlatform,
    config: DaemonConfig,
    reader: Any,
    exc: PlatformHidError,
) -> Any:
    _log({"daemon": "hid_read_error", "message": str(exc), "path": config.anticater_path})
    try:
        reader.close()
    except Exception as close_exc:
        _log({"daemon": "hid_close_error", "message": str(close_exc)})
    time.sleep(config.reconnect_delay)
    return open_reader_with_retry(platform, config)


def _log(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, sort_keys=True), flush=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the live BluOS Knob daemon loop.")
    parser.add_argument("--anticater-path", required=True)
    parser.add_argument("--bluos-host", required=True)
    parser.add_argument("--bluos-port", type=int, default=11000)
    parser.add_argument("--step-db", default="1")
    parser.add_argument("--max-db", default="-24")
    parser.add_argument("--source-safe-db", default="-40")
    parser.add_argument("--optical-input-type-index", default="optical-2")
    parser.add_argument("--spotify-source-name", default="Spotify")
    parser.add_argument("--timeout-ms", type=int, default=1000)
    parser.add_argument("--request-timeout", type=float, default=5.0)
    parser.add_argument("--reconnect-delay", type=float, default=2.0)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument(
        "--i-understand-this-controls-the-amplifier",
        action="store_true",
        help="Required unless --dry-run is set.",
    )
    return parser


def config_from_args(args: argparse.Namespace) -> DaemonConfig:
    if not args.dry_run and not args.i_understand_this_controls_the_amplifier:
        raise SystemExit("Refusing to run live daemon without explicit amplifier-control flag.")
    source_safe_db = str(parse_decimal(args.source_safe_db, "source-safe-db"))
    if Decimal(source_safe_db) > Decimal("-24"):
        raise SystemExit("source-safe-db must not exceed -24 dB")
    return DaemonConfig(
        anticater_path=args.anticater_path,
        bluos_host=args.bluos_host,
        bluos_port=args.bluos_port,
        step_db=parse_decimal(args.step_db, "step-db"),
        max_db=parse_decimal(args.max_db, "max-db"),
        source_safe_db=source_safe_db,
        optical_input_type_index=args.optical_input_type_index,
        spotify_source_name=args.spotify_source_name,
        timeout_ms=args.timeout_ms,
        request_timeout=args.request_timeout,
        reconnect_delay=args.reconnect_delay,
        dry_run=args.dry_run,
    )


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return run_daemon(config_from_args(args), args.limit)
    except Exception as exc:
        _log({"daemon": "error", "type": type(exc).__name__, "message": str(exc)})
        raise


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
