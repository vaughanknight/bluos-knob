import io

from bluos_knob.cli.main import (
    EXIT_BAD_OUTPUT,
    EXIT_INTERRUPTED,
    EXIT_NO_DEVICE,
    EXIT_OPEN_FAILED,
    main,
)
from bluos_knob.device_input.contracts import DeviceIdentity, HidDeviceInfo
from bluos_knob.device_input.fake_hid import FakeHidPlatform


def test_given_no_candidate_when_listing_then_clear_no_device_exit():
    """
    Test Doc:
    - Why: Missing hardware should produce an actionable diagnostic, not a crash.
    - Contract: No Anticater candidates returns EXIT_NO_DEVICE.
    - Usage Notes: Use `--all` for non-candidate device visibility.
    - Quality Contribution: Guards the most common first-run failure path.
    - Worked Example: empty fake platform returns 10.
    """
    errors = io.StringIO()

    exit_code = main(
        ["list"],
        platform_factory=lambda: FakeHidPlatform(devices=[], reports_by_path={}),
        stderr=errors,
    )

    assert exit_code == EXIT_NO_DEVICE
    assert "No Anticater candidate" in errors.getvalue()


def test_given_open_failure_when_capturing_then_open_failed_exit(tmp_path):
    """
    Test Doc:
    - Why: HID permissions or stale paths can make open fail after enumeration succeeds.
    - Contract: Failed open returns EXIT_OPEN_FAILED with the path in diagnostics.
    - Usage Notes: The fake raises at the platform boundary.
    - Quality Contribution: Keeps adapter failures visible and non-crashing.
    - Worked Example: fail-open path returns 11.
    """
    fixture = tmp_path / "capture.jsonl"
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="fake-hid-path"))
    platform = FakeHidPlatform(
        devices=[device],
        reports_by_path={"fake-hid-path": []},
        fail_open_paths={"fake-hid-path"},
    )
    errors = io.StringIO()

    exit_code = main(
        ["capture", "--path", "fake-hid-path", "--output", str(fixture), "--limit", "1"],
        platform_factory=lambda: platform,
        stderr=errors,
    )

    assert exit_code == EXIT_OPEN_FAILED
    assert "fake-hid-path" in errors.getvalue()


def test_given_invalid_output_path_when_capturing_then_bad_output_exit(tmp_path):
    """
    Test Doc:
    - Why: Users need clear feedback when the fixture path cannot be written.
    - Contract: Output path errors return EXIT_BAD_OUTPUT before opening hardware.
    - Usage Notes: Passing a directory as output is invalid.
    - Quality Contribution: Prevents partial hardware sessions with no fixture output.
    - Worked Example: output directory returns 12.
    """
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="fake-hid-path"))
    platform = FakeHidPlatform(devices=[device], reports_by_path={"fake-hid-path": []})
    errors = io.StringIO()

    exit_code = main(
        ["capture", "--path", "fake-hid-path", "--output", str(tmp_path), "--limit", "1"],
        platform_factory=lambda: platform,
        stderr=errors,
    )

    assert exit_code == EXIT_BAD_OUTPUT
    assert "Cannot write" in errors.getvalue()


def test_given_keyboard_interrupt_when_capturing_then_partial_fixture_is_closed(tmp_path):
    """
    Test Doc:
    - Why: Users may stop capture after exercising the knob operations.
    - Contract: KeyboardInterrupt returns EXIT_INTERRUPTED and leaves a valid partial file.
    - Usage Notes: The fake raises KeyboardInterrupt from the read boundary.
    - Quality Contribution: Prevents interrupted capture from losing evidence.
    - Worked Example: interrupted fake reader returns 13.
    """
    fixture = tmp_path / "capture.jsonl"
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="fake-hid-path"))
    platform = FakeHidPlatform(
        devices=[device],
        reports_by_path={"fake-hid-path": [KeyboardInterrupt()]},
    )
    errors = io.StringIO()

    exit_code = main(
        ["capture", "--path", "fake-hid-path", "--output", str(fixture), "--limit", "1"],
        platform_factory=lambda: platform,
        stderr=errors,
    )

    assert exit_code == EXIT_INTERRUPTED
    assert fixture.read_text(encoding="utf-8") == ""

