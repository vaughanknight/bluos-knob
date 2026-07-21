import pytest

from bluos_knob.platform_adapter.hidapi_reader import (
    HidapiPlatform,
    HidapiReportReader,
    PlatformHidError,
)


class _Device:
    def __init__(self, reports):
        self.reports = list(reports)
        self.closed = False

    def read(self, size, timeout_ms):
        del size, timeout_ms
        return self.reports.pop(0)

    def close(self):
        self.closed = True


def test_given_hidapi_bytes_when_read_then_full_report_is_preserved():
    """
    Test Doc:
    - Why: The baseline exists to preserve raw HID evidence from the real adapter.
    - Contract: Bytes returned by HIDAPI are captured intact unless a descriptor
      proves a report ID should be split later.
    - Usage Notes: Report ID is 0 for the first baseline; payload contains all bytes.
    - Quality Contribution: Prevents capture corruption for unnumbered reports.
    - Worked Example: e900 remains e900, not 00.
    """
    reader = HidapiReportReader(_Device([[0xE9, 0x00]]), path="fake-path")

    report = reader.read_report(timeout_ms=1)

    assert report is not None
    assert report.report_id == 0
    assert report.data == (0xE9, 0x00)
    assert report.raw_data_hex == "e900"


def test_given_text_path_when_opening_then_hidapi_receives_bytes(monkeypatch):
    """
    Test Doc:
    - Why: macOS HIDAPI can expose paths as text in JSON but require bytes for open_path.
    - Contract: The platform adapter encodes stored text paths before opening HIDAPI.
    - Usage Notes: CLI users still pass the printable path from `list`.
    - Quality Contribution: Catches real-device open regressions.
    - Worked Example: DevSrvsID:123 becomes b"DevSrvsID:123".
    """
    device = _OpenDevice()
    fake_hid = type("FakeHid", (), {"device": lambda self: device})()
    monkeypatch.setattr("bluos_knob.platform_adapter.hidapi_reader._load_hid", lambda: fake_hid)

    HidapiPlatform().open("DevSrvsID:123")

    assert device.opened_path == b"DevSrvsID:123"


def test_given_failed_open_when_open_path_raises_then_native_handle_is_closed(monkeypatch):
    """
    Test Doc:
    - Why: The daemon retries opens constantly while the Bluetooth knob is absent; a
      partially-opened handle left to GC leaks IOKit memory on every failure and was the
      per-failed-open contributor to the ~18 GB OOM growth.
    - Contract: When open_path raises, the just-created device is closed before the
      PlatformHidError propagates.
    - Usage Notes: The error is still surfaced so callers can retry/enumerate.
    - Quality Contribution: Removes the failed-open native leak.
    - Worked Example: open_path raises -> device.close() called -> PlatformHidError raised.
    """
    device = _FailingOpenDevice()
    fake_hid = type("FakeHid", (), {"device": lambda self: device})()
    monkeypatch.setattr("bluos_knob.platform_adapter.hidapi_reader._load_hid", lambda: fake_hid)

    with pytest.raises(PlatformHidError):
        HidapiPlatform().open("DevSrvsID:123")

    assert device.closed is True


class _OpenDevice:
    def __init__(self):
        self.opened_path = None

    def open_path(self, path):
        self.opened_path = path


class _FailingOpenDevice:
    def __init__(self):
        self.closed = False

    def open_path(self, path):
        del path
        raise OSError("device busy")

    def close(self):
        self.closed = True
