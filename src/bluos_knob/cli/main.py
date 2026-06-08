"""CLI entry point for listing, capturing, and replaying Anticater HID evidence."""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections.abc import Callable, Sequence
from pathlib import Path
from typing import Protocol, TextIO

from bluos_knob.configuration.profiles import default_anticater_profile
from bluos_knob.device_input.contracts import CaptureFixtureRow, HidDeviceInfo, RawHidReport
from bluos_knob.device_input.fixture_schema import (
    FixtureSchemaError,
    load_jsonl,
    row_to_json_line,
)
from bluos_knob.device_input.normalizer import normalize_report
from bluos_knob.platform_adapter.hidapi_reader import HidapiPlatform, PlatformHidError

EXIT_OK = 0
EXIT_NO_DEVICE = 10
EXIT_OPEN_FAILED = 11
EXIT_BAD_OUTPUT = 12
EXIT_INTERRUPTED = 13
EXIT_INVALID_FIXTURE = 14
EXIT_HID_UNAVAILABLE = 15


class ReportReader(Protocol):
    def read_report(self, timeout_ms: int | None = None) -> RawHidReport | None: ...

    def close(self) -> None: ...


class HidPlatform(Protocol):
    def enumerate_devices(self) -> list[HidDeviceInfo]: ...

    def open(self, path: str) -> ReportReader: ...


PlatformFactory = Callable[[], HidPlatform]
Clock = Callable[[], float]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="bluos-knob")
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", help="List candidate HID interfaces.")
    list_parser.add_argument("--all", action="store_true", help="Show all HID devices.")

    capture_parser = subparsers.add_parser("capture", help="Capture raw HID reports to JSONL.")
    capture_parser.add_argument("--path", help="HID path to open. Defaults to first candidate.")
    capture_parser.add_argument("--output", required=True, help="JSONL fixture output path.")
    capture_parser.add_argument(
        "--limit", type=int, default=0, help="Number of reports to capture."
    )
    capture_parser.add_argument("--timeout-ms", type=int, default=1000)
    capture_parser.add_argument(
        "--operation-label", help="Optional label for the captured operation."
    )

    replay_parser = subparsers.add_parser("replay", help="Replay a JSONL fixture without hardware.")
    replay_parser.add_argument("--fixture", required=True, help="JSONL fixture path.")

    return parser


def main(
    argv: Sequence[str] | None = None,
    *,
    platform_factory: PlatformFactory | None = None,
    stdout: TextIO | None = None,
    stderr: TextIO | None = None,
    clock: Clock | None = None,
) -> int:
    output = stdout or sys.stdout
    errors = stderr or sys.stderr
    platform_factory = platform_factory or HidapiPlatform
    clock = clock or time.time
    args = build_parser().parse_args(argv)

    if args.command == "list":
        return _list_devices(args, platform_factory, output, errors)
    if args.command == "capture":
        return _capture(args, platform_factory, output, errors, clock)
    if args.command == "replay":
        return _replay(args, output, errors)
    raise AssertionError(f"unhandled command: {args.command}")


def entrypoint() -> int:
    return main()


def _list_devices(
    args: argparse.Namespace,
    platform_factory: PlatformFactory,
    stdout: TextIO,
    stderr: TextIO,
) -> int:
    try:
        platform = platform_factory()
        devices = platform.enumerate_devices()
    except PlatformHidError as exc:
        print(str(exc), file=stderr)
        return EXIT_HID_UNAVAILABLE

    profile = default_anticater_profile()
    shown = [device for device in devices if args.all or profile.matches(device)]
    if not shown:
        print("No Anticater candidate HID devices found.", file=stderr)
        return EXIT_NO_DEVICE

    for device in shown:
        print(json.dumps(_device_output(device), sort_keys=True), file=stdout)
    return EXIT_OK


def _capture(
    args: argparse.Namespace,
    platform_factory: PlatformFactory,
    stdout: TextIO,
    stderr: TextIO,
    clock: Clock,
) -> int:
    try:
        platform = platform_factory()
        path = args.path or _first_candidate_path(platform.enumerate_devices())
    except PlatformHidError as exc:
        print(str(exc), file=stderr)
        return EXIT_HID_UNAVAILABLE

    if not path:
        print("No Anticater candidate HID devices found.", file=stderr)
        return EXIT_NO_DEVICE

    output_path = Path(args.output)
    try:
        handle = output_path.open("w", encoding="utf-8")
    except OSError as exc:
        print(f"Cannot write fixture output {output_path}: {exc}", file=stderr)
        return EXIT_BAD_OUTPUT

    reader: ReportReader | None = None
    captured = 0
    exit_code = EXIT_OK
    try:
        try:
            reader = platform.open(path)
        except (OSError, PlatformHidError) as exc:
            print(f"Cannot open HID device {path!r}: {exc}", file=stderr)
            return EXIT_OPEN_FAILED

        while args.limit == 0 or captured < args.limit:
            report = reader.read_report(args.timeout_ms)
            if report is None:
                if args.limit:
                    break
                continue
            if report.timestamp == 0:
                report = RawHidReport(
                    timestamp=clock(),
                    report_id=report.report_id,
                    data=report.data,
                    sequence=report.sequence,
                    device=report.device,
                    transport=report.transport,
                    connection_state=report.connection_state,
                )
            row = CaptureFixtureRow(raw_report=report, operation_label=args.operation_label)
            handle.write(row_to_json_line(row))
            handle.write("\n")
            print(json.dumps(_capture_output(report), sort_keys=True), file=stdout)
            captured += 1
    except KeyboardInterrupt:
        print("Capture interrupted; partial fixture was left on disk.", file=stderr)
        exit_code = EXIT_INTERRUPTED
    finally:
        if reader is not None:
            reader.close()
        handle.close()
    return exit_code


def _replay(args: argparse.Namespace, stdout: TextIO, stderr: TextIO) -> int:
    try:
        rows = load_jsonl(args.fixture)
    except (OSError, FixtureSchemaError) as exc:
        print(f"Cannot replay fixture {args.fixture}: {exc}", file=stderr)
        return EXIT_INVALID_FIXTURE

    for row in rows:
        print(json.dumps(_event_output(row.raw_report), sort_keys=True), file=stdout)
    return EXIT_OK


def _first_candidate_path(devices: list[HidDeviceInfo]) -> str | None:
    profile = default_anticater_profile()
    for device in devices:
        if profile.matches(device) and device.identity.path:
            return device.identity.path
    return None


def _device_output(device: HidDeviceInfo) -> dict[str, object]:
    return {
        "candidate": default_anticater_profile().matches(device),
        **device.to_dict(),
    }


def _capture_output(report: RawHidReport) -> dict[str, object]:
    return {
        "raw_report": {
            "timestamp": report.timestamp,
            "sequence": report.sequence,
            "report_id": report.report_id,
            "data": report.raw_data_hex,
            "device": report.device.to_dict(),
            "transport": report.transport.value,
            "connection_state": report.connection_state.value,
        },
        "normalized_event": _event_output(report),
    }


def _event_output(report: RawHidReport) -> dict[str, object]:
    return normalize_report(report).to_dict()


if __name__ == "__main__":
    raise SystemExit(entrypoint())
