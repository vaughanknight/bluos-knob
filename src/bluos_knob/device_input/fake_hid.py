"""Fixture-backed fakes for deterministic HID boundary tests."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass

from bluos_knob.device_input.contracts import CaptureFixtureRow, HidDeviceInfo, RawHidReport


class FakeHidExhausted(Exception):
    """Raised by tests that want exhaustion to be explicit."""


@dataclass
class FakeHidReportReader:
    reports: list[RawHidReport | BaseException]
    index: int = 0
    closed: bool = False

    def read_report(self, timeout_ms: int | None = None) -> RawHidReport | None:
        del timeout_ms
        if self.index >= len(self.reports):
            return None
        item = self.reports[self.index]
        self.index += 1
        if isinstance(item, BaseException):
            raise item
        return item

    def close(self) -> None:
        self.closed = True


@dataclass
class FakeHidPlatform:
    devices: list[HidDeviceInfo]
    reports_by_path: dict[str, list[RawHidReport | BaseException]]
    fail_open_paths: set[str] | None = None

    @classmethod
    def from_fixture_rows(
        cls,
        device: HidDeviceInfo,
        rows: Iterable[CaptureFixtureRow],
        path: str = "fake-hid-path",
    ) -> FakeHidPlatform:
        reports = [row.raw_report for row in rows]
        return cls(devices=[device], reports_by_path={path: reports})

    def enumerate_devices(self) -> list[HidDeviceInfo]:
        return list(self.devices)

    def open(self, path: str) -> FakeHidReportReader:
        if self.fail_open_paths and path in self.fail_open_paths:
            raise OSError(f"failed to open fake HID path: {path}")
        if path not in self.reports_by_path:
            raise OSError(f"unknown fake HID path: {path}")
        return FakeHidReportReader(list(self.reports_by_path[path]))

