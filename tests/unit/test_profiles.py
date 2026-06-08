from bluos_knob.configuration.profiles import AnticaterProfile, default_anticater_profile
from bluos_knob.device_input.contracts import DeviceIdentity, HidDeviceInfo


def test_given_product_hint_when_matching_default_profile_then_candidate_is_selected():
    """
    Test Doc:
    - Why: Users should not need a hardcoded local HID path to find the knob.
    - Contract: Default profile matches broad Anticater/VK product hints.
    - Usage Notes: Exact VID/PID filters can be added later without changing CLI code.
    - Quality Contribution: Prevents accidental coupling to one user's device identifier.
    - Worked Example: "VK-01 Volume Knob" matches.
    """
    device = HidDeviceInfo(DeviceIdentity(product="VK-01 Volume Knob", path="local"))

    assert default_anticater_profile().matches(device)


def test_given_exact_vid_pid_filter_when_values_differ_then_candidate_is_rejected():
    """
    Test Doc:
    - Why: Future users may need precise profile filters when multiple HID devices exist.
    - Contract: Exact hardware filters must be honored when configured.
    - Usage Notes: Exact filters are optional; no local path is required.
    - Quality Contribution: Catches over-broad device selection regressions.
    - Worked Example: vendor_id 123 rejects vendor_id 999.
    """
    profile = AnticaterProfile(vendor_id=123, product_contains=(), manufacturer_contains=())
    device = HidDeviceInfo(DeviceIdentity(vendor_id=999, product="VK-01"))

    assert not profile.matches(device)

