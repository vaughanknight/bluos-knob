from bluos_knob.device_input.contracts import (
    CaptureFixtureRow,
    ConnectionState,
    DeviceIdentity,
    RawHidReport,
    TransportMode,
)
from bluos_knob.device_input.fixture_schema import (
    FixtureSchemaError,
    redact_local_identity,
    row_from_json_line,
    row_to_dict,
    row_to_json_line,
)


def test_given_valid_fixture_row_when_round_tripped_then_contract_fields_survive():
    """
    Test Doc:
    - Why: Fixture replay is the backpressure for hardware-free Anticater learning.
    - Contract: JSONL rows preserve schema version, sequence, transport,
      raw bytes, and local identity fields.
    - Usage Notes: Raw bytes are hex strings for stable diffs.
    - Quality Contribution: Catches fixture schema drift before real captures accumulate.
    - Worked Example: e900 remains the volume-up raw payload after roundtrip.
    """
    row = CaptureFixtureRow(
        raw_report=RawHidReport(
            timestamp=1.25,
            report_id=1,
            data=(0xE9, 0x00),
            sequence=7,
            device=DeviceIdentity(product="VK-01", path="local-path", serial_number="secret"),
            transport=TransportMode.BLUETOOTH,
            connection_state=ConnectionState.CONNECTED,
        ),
        operation_label="rotate-right",
    )

    decoded = row_from_json_line(row_to_json_line(row))

    assert decoded.raw_report.raw_data_hex == "e900"
    assert decoded.raw_report.sequence == 7
    assert decoded.raw_report.transport == TransportMode.BLUETOOTH
    assert decoded.operation_label == "rotate-right"


def test_given_invalid_hex_when_loaded_then_schema_error_is_actionable():
    """
    Test Doc:
    - Why: Bad fixture rows should fail clearly instead of producing misleading replay output.
    - Contract: `data` must be a valid even-length hex string.
    - Usage Notes: Catch `FixtureSchemaError` for diagnostics.
    - Quality Contribution: Guards replay against malformed local edits.
    - Worked Example: `not-hex` is rejected.
    """
    line = (
        '{"schema_version":1,"timestamp":1,"sequence":1,"report_id":1,'
        '"data":"not-hex","transport":"unknown","connection_state":"unknown","device":{}}'
    )

    try:
        row_from_json_line(line)
    except FixtureSchemaError as exc:
        assert "data must be" in str(exc)
    else:
        raise AssertionError("invalid hex was accepted")


def test_given_missing_required_field_when_loaded_then_schema_error_names_field():
    """
    Test Doc:
    - Why: Fixture replay diagnostics must stay actionable for local capture edits.
    - Contract: Missing required fields raise `FixtureSchemaError`, not raw KeyError.
    - Usage Notes: The message identifies the missing field.
    - Quality Contribution: Prevents confusing tracebacks from invalid fixtures.
    - Worked Example: missing timestamp reports `timestamp is required`.
    """
    line = (
        '{"schema_version":1,"sequence":1,"report_id":1,"data":"e900",'
        '"transport":"unknown","connection_state":"unknown","device":{}}'
    )

    try:
        row_from_json_line(line)
    except FixtureSchemaError as exc:
        assert "timestamp is required" in str(exc)
    else:
        raise AssertionError("missing timestamp was accepted")


def test_given_bad_enum_when_loaded_then_schema_error_names_field():
    """
    Test Doc:
    - Why: Transport and connection fields are part of the replay contract.
    - Contract: Unsupported enum values raise `FixtureSchemaError`.
    - Usage Notes: Keep enum values in docs/how/fixture-format.md.
    - Quality Contribution: Catches typoed fixture metadata before replay.
    - Worked Example: `space-laser` is rejected as transport.
    """
    line = (
        '{"schema_version":1,"timestamp":1,"sequence":1,"report_id":1,'
        '"data":"e900","transport":"space-laser",'
        '"connection_state":"unknown","device":{}}'
    )

    try:
        row_from_json_line(line)
    except FixtureSchemaError as exc:
        assert "transport has unsupported value" in str(exc)
    else:
        raise AssertionError("bad transport was accepted")


def test_given_missing_transport_when_loaded_then_schema_error_names_field():
    """
    Test Doc:
    - Why: Transport mode is required replay evidence, not an optional default.
    - Contract: Missing transport raises `FixtureSchemaError`.
    - Usage Notes: Use `unknown` explicitly when the platform cannot determine transport.
    - Quality Contribution: Prevents under-specified fixtures from silently passing.
    - Worked Example: missing transport reports `transport is required`.
    """
    line = (
        '{"schema_version":1,"timestamp":1,"sequence":1,"report_id":1,'
        '"data":"e900","connection_state":"unknown","device":{}}'
    )

    try:
        row_from_json_line(line)
    except FixtureSchemaError as exc:
        assert "transport is required" in str(exc)
    else:
        raise AssertionError("missing transport was accepted")


def test_given_local_identity_when_redacted_then_path_and_serial_are_removed():
    """
    Test Doc:
    - Why: Captures can contain private local HID paths and serial numbers.
    - Contract: Redaction replaces local identity fields while preserving useful metadata.
    - Usage Notes: Call on `row_to_dict(row)` before sharing fixtures.
    - Quality Contribution: Makes fixture privacy guidance executable.
    - Worked Example: path and serial_number become <redacted>.
    """
    row = CaptureFixtureRow(
        raw_report=RawHidReport(
            timestamp=1,
            report_id=1,
            data=(0xE9, 0x00),
            sequence=1,
            device=DeviceIdentity(path="local", serial_number="private", product="VK-01"),
        )
    )

    redacted = redact_local_identity(row_to_dict(row))

    assert redacted["device"]["path"] == "<redacted>"
    assert redacted["device"]["serial_number"] == "<redacted>"
    assert redacted["device"]["product"] == "VK-01"
