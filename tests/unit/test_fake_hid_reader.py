from bluos_knob.device_input.contracts import DeviceIdentity, HidDeviceInfo, RawHidReport
from bluos_knob.device_input.fake_hid import FakeHidPlatform


def test_given_fixture_backed_fake_when_opened_then_reports_replay_in_order():
    """
    Test Doc:
    - Why: Tests must use fakes instead of physical Anticater hardware.
    - Contract: Fake HID readers replay raw reports deterministically and then exhaust.
    - Usage Notes: The fake implements the same enumerate/open/read shape the CLI consumes.
    - Quality Contribution: Protects CLI capture/replay tests from hardware dependency.
    - Worked Example: two reports are returned, then None.
    """
    device = HidDeviceInfo(DeviceIdentity(product="VK-01", path="fake-hid-path"))
    reports = [
        RawHidReport(1, 1, (0xE9, 0x00), 1, device.identity),
        RawHidReport(2, 1, (0xEA, 0x00), 2, device.identity),
    ]
    platform = FakeHidPlatform(devices=[device], reports_by_path={"fake-hid-path": reports})

    reader = platform.open("fake-hid-path")

    assert platform.enumerate_devices() == [device]
    assert reader.read_report().sequence == 1
    assert reader.read_report().sequence == 2
    assert reader.read_report() is None

