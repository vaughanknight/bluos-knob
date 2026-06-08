import io
import json

from bluos_knob.cli.main import EXIT_OK, main
from bluos_knob.device_input.contracts import DeviceIdentity, HidDeviceInfo, RawHidReport
from bluos_knob.device_input.fake_hid import FakeHidPlatform


def test_given_capture_fixture_when_replayed_then_normalized_events_match_capture(tmp_path):
    """
    Test Doc:
    - Why: Replay must prove the same behavior as live capture without hardware.
    - Contract: Capture and replay share one normalization code path and produce the same events.
    - Usage Notes: Capture output includes raw report plus normalized_event;
      replay prints normalized events.
    - Quality Contribution: Catches drift between live capture and fixture replay.
    - Worked Example: captured e900 and replayed e900 both emit volume_up.
    """
    fixture = tmp_path / "capture.jsonl"
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="fake-hid-path"))
    reports = [
        RawHidReport(1.0, 1, (0xE9, 0x00), 1, device.identity),
        RawHidReport(2.0, 1, (0xEA, 0x00), 2, device.identity),
    ]
    platform = FakeHidPlatform(devices=[device], reports_by_path={"fake-hid-path": reports})
    capture_output = io.StringIO()
    replay_output = io.StringIO()

    capture_code = main(
        ["capture", "--path", "fake-hid-path", "--output", str(fixture), "--limit", "2"],
        platform_factory=lambda: platform,
        stdout=capture_output,
    )
    replay_code = main(["replay", "--fixture", str(fixture)], stdout=replay_output)

    captured_events = [
        json.loads(line)["normalized_event"] for line in capture_output.getvalue().splitlines()
    ]
    replayed_events = [json.loads(line) for line in replay_output.getvalue().splitlines()]
    assert capture_code == EXIT_OK
    assert replay_code == EXIT_OK
    assert replayed_events == captured_events
