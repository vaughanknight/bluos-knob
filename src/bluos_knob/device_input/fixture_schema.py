"""JSONL fixture schema for raw HID report capture and replay."""

from __future__ import annotations

import json
from collections.abc import Iterable
from pathlib import Path
from typing import Any

from bluos_knob.device_input.contracts import (
    SCHEMA_VERSION,
    CaptureFixtureRow,
    ConnectionState,
    DeviceIdentity,
    RawHidReport,
    TransportMode,
)

LOCAL_IDENTITY_FIELDS = {"path", "serial_number"}


class FixtureSchemaError(ValueError):
    """Raised when a JSONL fixture row does not match the baseline schema."""


def row_to_dict(row: CaptureFixtureRow) -> dict[str, Any]:
    return {
        "schema_version": row.schema_version,
        "timestamp": row.raw_report.timestamp,
        "sequence": row.raw_report.sequence,
        "report_id": row.raw_report.report_id,
        "data": row.raw_report.raw_data_hex,
        "transport": row.raw_report.transport.value,
        "connection_state": row.raw_report.connection_state.value,
        "device": row.raw_report.device.to_dict(),
        "operation_label": row.operation_label,
        "notes": row.notes,
    }


def row_from_dict(value: dict[str, Any]) -> CaptureFixtureRow:
    if value.get("schema_version") != SCHEMA_VERSION:
        raise FixtureSchemaError(f"unsupported schema_version: {value.get('schema_version')!r}")

    data_hex = _require_str(value, "data")
    try:
        data = tuple(bytes.fromhex(data_hex))
    except ValueError as exc:
        raise FixtureSchemaError("data must be an even-length hex string") from exc

    device_value = value.get("device")
    if not isinstance(device_value, dict):
        raise FixtureSchemaError("device must be an object")

    raw_report = RawHidReport(
        timestamp=_require_float(value, "timestamp"),
        report_id=_require_int(value, "report_id"),
        data=data,
        sequence=_optional_int(value.get("sequence")),
        device=DeviceIdentity.from_dict(device_value),
        transport=_enum_value(value, "transport", TransportMode),
        connection_state=_enum_value(value, "connection_state", ConnectionState),
    )
    return CaptureFixtureRow(
        raw_report=raw_report,
        schema_version=SCHEMA_VERSION,
        operation_label=_optional_str(value.get("operation_label")),
        notes=_optional_str(value.get("notes")),
    )


def row_to_json_line(row: CaptureFixtureRow) -> str:
    return json.dumps(row_to_dict(row), sort_keys=True)


def row_from_json_line(line: str) -> CaptureFixtureRow:
    try:
        decoded = json.loads(line)
    except json.JSONDecodeError as exc:
        raise FixtureSchemaError(f"invalid JSONL row: {exc}") from exc
    if not isinstance(decoded, dict):
        raise FixtureSchemaError("fixture row must be a JSON object")
    return row_from_dict(decoded)


def load_jsonl(path: str | Path) -> list[CaptureFixtureRow]:
    rows: list[CaptureFixtureRow] = []
    with Path(path).open(encoding="utf-8") as handle:
        for index, line in enumerate(handle, start=1):
            stripped = line.strip()
            if not stripped:
                continue
            try:
                rows.append(row_from_json_line(stripped))
            except FixtureSchemaError as exc:
                raise FixtureSchemaError(f"{path}:{index}: {exc}") from exc
    return rows


def write_jsonl(path: str | Path, rows: Iterable[CaptureFixtureRow]) -> None:
    with Path(path).open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(row_to_json_line(row))
            handle.write("\n")


def redact_local_identity(value: dict[str, Any]) -> dict[str, Any]:
    redacted = json.loads(json.dumps(value))
    device = redacted.get("device")
    if isinstance(device, dict):
        for field in LOCAL_IDENTITY_FIELDS:
            if device.get(field):
                device[field] = "<redacted>"
    return redacted


def _require_str(value: dict[str, Any], key: str) -> str:
    field = value.get(key)
    if not isinstance(field, str):
        raise FixtureSchemaError(f"{key} must be a string")
    return field


def _optional_str(value: Any) -> str | None:
    if value is None:
        return None
    return str(value)


def _require_int(value: dict[str, Any], key: str) -> int:
    try:
        return int(value[key])
    except KeyError as exc:
        raise FixtureSchemaError(f"{key} is required") from exc
    except (TypeError, ValueError) as exc:
        raise FixtureSchemaError(f"{key} must be an integer") from exc


def _require_float(value: dict[str, Any], key: str) -> float:
    try:
        return float(value[key])
    except KeyError as exc:
        raise FixtureSchemaError(f"{key} is required") from exc
    except (TypeError, ValueError) as exc:
        raise FixtureSchemaError(f"{key} must be a number") from exc


def _optional_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError) as exc:
        raise FixtureSchemaError("sequence must be an integer when present") from exc


def _enum_value(
    value: dict[str, Any],
    key: str,
    enum_type: type[TransportMode] | type[ConnectionState],
) -> TransportMode | ConnectionState:
    if key not in value:
        raise FixtureSchemaError(f"{key} is required")
    raw = value[key]
    try:
        return enum_type(raw)
    except ValueError as exc:
        raise FixtureSchemaError(f"{key} has unsupported value: {raw!r}") from exc
