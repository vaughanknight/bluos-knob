import io
import json

from bluos_knob.cli.main import EXIT_OK, main
from bluos_knob.device_input.contracts import DeviceIdentity, HidDeviceInfo
from bluos_knob.device_input.fake_hid import FakeHidPlatform


def test_given_fake_platform_when_listing_then_candidate_metadata_is_printed():
    """
    Test Doc:
    - Why: Listing behavior must be deterministic without real Bluetooth hardware.
    - Contract: `list` prints useful candidate metadata from the platform adapter seam.
    - Usage Notes: Tests pass a fake platform factory instead of importing HIDAPI.
    - Quality Contribution: Proves AC1 through a fake, not a mock or hardware.
    - Worked Example: product and path appear in JSON output.
    """
    output = io.StringIO()
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="fake-hid-path"))
    platform = FakeHidPlatform(devices=[device], reports_by_path={})

    exit_code = main(["list"], platform_factory=lambda: platform, stdout=output)

    line = json.loads(output.getvalue())
    assert exit_code == EXIT_OK
    assert line["candidate"] is True
    assert line["identity"]["path"] == "fake-hid-path"

