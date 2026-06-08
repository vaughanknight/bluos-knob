"""Best-effort mapping from raw HID reports to normalized knob events."""

from __future__ import annotations

from bluos_knob.device_input.contracts import NormalizedAction, NormalizedKnobEvent, RawHidReport

CONSUMER_USAGE_TO_ACTION = {
    0x00E9: NormalizedAction.VOLUME_UP,
    0x00EA: NormalizedAction.VOLUME_DOWN,
    0x00E2: NormalizedAction.MUTE_TOGGLE,
    0x006F: NormalizedAction.BRIGHTNESS_UP,
    0x0070: NormalizedAction.BRIGHTNESS_DOWN,
}


def normalize_report(report: RawHidReport) -> NormalizedKnobEvent:
    usage_code = first_consumer_usage(report.data)
    if usage_code is None:
        action = (
            NormalizedAction.NO_OP
            if _is_release_report(report.data)
            else NormalizedAction.UNKNOWN
        )
        return _event(report, action=action, magnitude=0, usage_code=None)

    return _event(
        report,
        action=CONSUMER_USAGE_TO_ACTION.get(usage_code, NormalizedAction.UNKNOWN),
        magnitude=1,
        usage_code=usage_code,
    )


def first_consumer_usage(data: tuple[int, ...]) -> int | None:
    if _is_release_report(data):
        return None

    for index in range(0, max(len(data) - 1, 0)):
        candidate = data[index] | (data[index + 1] << 8)
        if candidate in CONSUMER_USAGE_TO_ACTION:
            return candidate
    return None


def _event(
    report: RawHidReport,
    *,
    action: NormalizedAction,
    magnitude: int,
    usage_code: int | None,
) -> NormalizedKnobEvent:
    return NormalizedKnobEvent(
        action=action,
        magnitude=magnitude,
        source_device=report.device,
        sequence=report.sequence,
        raw_report_id=report.report_id,
        raw_data_hex=report.raw_data_hex,
        transport=report.transport,
        connection_state=report.connection_state,
        usage_code=usage_code,
    )


def _is_release_report(data: tuple[int, ...]) -> bool:
    return not data or all(byte == 0 for byte in data) or (
        len(data) > 2 and data[0] != 0 and all(byte == 0 for byte in data[1:])
    )
