import pytest

from bluos_knob.device_input.contracts import (
    DeviceIdentity,
    NormalizedAction,
    RawHidReport,
)
from bluos_knob.device_input.normalizer import normalize_report


@pytest.mark.parametrize(
    ("payload", "action"),
    [
        ((0xE9, 0x00), NormalizedAction.VOLUME_UP),
        ((0xEA, 0x00), NormalizedAction.VOLUME_DOWN),
        ((0xE2, 0x00), NormalizedAction.MUTE_TOGGLE),
        ((0x6F, 0x00), NormalizedAction.BRIGHTNESS_UP),
        ((0x70, 0x00), NormalizedAction.BRIGHTNESS_DOWN),
    ],
)
def test_given_known_consumer_usage_when_normalized_then_expected_action(payload, action):
    """
    Test Doc:
    - Why: The CLI needs useful best-effort labels for obvious HID consumer reports.
    - Contract: Known consumer usages map to normalized transport-neutral actions.
    - Usage Notes: The raw report remains the source of truth; unknowns stay observable.
    - Quality Contribution: Catches regressions in the shared capture/replay normalization path.
    - Worked Example: e900 maps to volume_up.
    """
    event = normalize_report(_report(payload))

    assert event.action == action
    assert event.magnitude == 1
    assert event.sequence == 42
    assert event.raw_data_hex == bytes(payload).hex()


def test_given_release_report_when_normalized_then_no_op_not_unknown():
    """
    Test Doc:
    - Why: HID consumer controls often emit all-zero release reports after a press.
    - Contract: Release reports are no-op events, not unknown actions.
    - Usage Notes: Non-empty unrecognized reports still become unknown.
    - Quality Contribution: Prevents duplicate/spurious knob actions during capture.
    - Worked Example: 0000 maps to no_op.
    """
    event = normalize_report(_report((0x00, 0x00)))

    assert event.action == NormalizedAction.NO_OP
    assert event.magnitude == 0


def test_given_report_id_prefixed_release_when_normalized_then_no_op():
    """
    Test Doc:
    - Why: The real Anticater capture emitted 030000 as a release/no-op report.
    - Contract: Report-ID-prefixed zero payloads are no-op events, not unknown actions.
    - Usage Notes: Preserve the raw bytes while preventing fake knob actions.
    - Quality Contribution: Converts real capture evidence into regression coverage.
    - Worked Example: 030000 maps to no_op.
    """
    event = normalize_report(_report((0x03, 0x00, 0x00)))

    assert event.action == NormalizedAction.NO_OP
    assert event.magnitude == 0
    assert event.raw_data_hex == "030000"


def test_given_unrecognized_report_when_normalized_then_unknown_preserves_raw_data():
    """
    Test Doc:
    - Why: Vendor-defined or surprising reports must remain visible for future analysis.
    - Contract: Unknown reports preserve raw bytes and correlation metadata.
    - Usage Notes: Do not discard unmapped reports.
    - Quality Contribution: Keeps the baseline from hiding Anticater-specific capabilities.
    - Worked Example: 3412 maps to unknown with raw_data_hex 3412.
    """
    event = normalize_report(_report((0x34, 0x12)))

    assert event.action == NormalizedAction.UNKNOWN
    assert event.raw_data_hex == "3412"


def _report(payload: tuple[int, ...]) -> RawHidReport:
    return RawHidReport(
        timestamp=1.0,
        report_id=1,
        data=payload,
        sequence=42,
        device=DeviceIdentity(product="VK-01", path="fake-hid-path"),
    )
