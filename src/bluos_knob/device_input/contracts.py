"""Transport-neutral contracts for Anticater HID input evidence."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any

SCHEMA_VERSION = 1


class TransportMode(str, Enum):
    USB = "usb"
    BLUETOOTH = "bluetooth"
    TWO_POINT_FOUR_GHZ = "2.4ghz"
    UNKNOWN = "unknown"


class ConnectionState(str, Enum):
    CONNECTED = "connected"
    DISCONNECTED = "disconnected"
    UNKNOWN = "unknown"


class NormalizedAction(str, Enum):
    VOLUME_UP = "volume_up"
    VOLUME_DOWN = "volume_down"
    MUTE_TOGGLE = "mute_toggle"
    BRIGHTNESS_UP = "brightness_up"
    BRIGHTNESS_DOWN = "brightness_down"
    NO_OP = "no_op"
    UNKNOWN = "unknown"


@dataclass(frozen=True)
class DeviceIdentity:
    manufacturer: str | None = None
    product: str | None = None
    vendor_id: int | None = None
    product_id: int | None = None
    serial_number: str | None = None
    path: str | None = None
    usage_page: int | None = None
    usage: int | None = None
    interface_number: int | None = None
    transport: TransportMode = TransportMode.UNKNOWN

    def to_dict(self) -> dict[str, Any]:
        return {
            "manufacturer": self.manufacturer,
            "product": self.product,
            "vendor_id": self.vendor_id,
            "product_id": self.product_id,
            "serial_number": self.serial_number,
            "path": self.path,
            "usage_page": self.usage_page,
            "usage": self.usage,
            "interface_number": self.interface_number,
            "transport": self.transport.value,
        }

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> DeviceIdentity:
        return cls(
            manufacturer=_optional_str(value.get("manufacturer")),
            product=_optional_str(value.get("product")),
            vendor_id=_optional_int(value.get("vendor_id")),
            product_id=_optional_int(value.get("product_id")),
            serial_number=_optional_str(value.get("serial_number")),
            path=_optional_str(value.get("path")),
            usage_page=_optional_int(value.get("usage_page")),
            usage=_optional_int(value.get("usage")),
            interface_number=_optional_int(value.get("interface_number")),
            transport=TransportMode(value.get("transport") or TransportMode.UNKNOWN.value),
        )


@dataclass(frozen=True)
class DeviceCapability:
    usage_page: int | None
    usage: int | None
    report_id: int | None = None
    kind: str = "input"
    vendor_defined: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "usage_page": self.usage_page,
            "usage": self.usage,
            "report_id": self.report_id,
            "kind": self.kind,
            "vendor_defined": self.vendor_defined,
        }


@dataclass(frozen=True)
class HidDeviceInfo:
    identity: DeviceIdentity
    capabilities: tuple[DeviceCapability, ...] = field(default_factory=tuple)

    def to_dict(self) -> dict[str, Any]:
        return {
            "identity": self.identity.to_dict(),
            "capabilities": [capability.to_dict() for capability in self.capabilities],
        }


@dataclass(frozen=True)
class RawHidReport:
    timestamp: float
    report_id: int
    data: tuple[int, ...]
    sequence: int | None
    device: DeviceIdentity
    transport: TransportMode = TransportMode.UNKNOWN
    connection_state: ConnectionState = ConnectionState.UNKNOWN

    @property
    def raw_data_hex(self) -> str:
        return bytes(self.data).hex()

    @property
    def correlation_id(self) -> str:
        sequence = "none" if self.sequence is None else str(self.sequence)
        return f"{sequence}:{self.report_id}:{self.raw_data_hex}"


@dataclass(frozen=True)
class CaptureFixtureRow:
    raw_report: RawHidReport
    schema_version: int = SCHEMA_VERSION
    operation_label: str | None = None
    notes: str | None = None


@dataclass(frozen=True)
class NormalizedKnobEvent:
    action: NormalizedAction
    magnitude: int
    source_device: DeviceIdentity
    sequence: int | None
    raw_report_id: int
    raw_data_hex: str
    transport: TransportMode
    connection_state: ConnectionState
    usage_code: int | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "action": self.action.value,
            "magnitude": self.magnitude,
            "source_device": self.source_device.to_dict(),
            "sequence": self.sequence,
            "raw_report_id": self.raw_report_id,
            "raw_data_hex": self.raw_data_hex,
            "transport": self.transport.value,
            "connection_state": self.connection_state.value,
            "usage_code": self.usage_code,
        }


def _optional_str(value: Any) -> str | None:
    if value is None:
        return None
    if isinstance(value, bytes):
        return value.decode(errors="replace")
    return str(value)


def _optional_int(value: Any) -> int | None:
    if value is None or value == "":
        return None
    return int(value)

