"""Real HIDAPI adapter for local HID enumeration and report capture."""

from __future__ import annotations

import time
from typing import Any

from bluos_knob.device_input.contracts import (
    ConnectionState,
    DeviceCapability,
    DeviceIdentity,
    HidDeviceInfo,
    RawHidReport,
    TransportMode,
)


class PlatformHidError(RuntimeError):
    """Raised when the optional HIDAPI runtime cannot enumerate or open devices."""


class HidapiPlatform:
    def enumerate_devices(self) -> list[HidDeviceInfo]:
        hid = _load_hid()
        try:
            devices = hid.enumerate()
        except Exception as exc:  # pragma: no cover - depends on local HID stack
            raise PlatformHidError(f"failed to enumerate HID devices: {exc}") from exc
        return [_device_from_hidapi(item) for item in devices]

    def open(self, path: str) -> HidapiReportReader:
        hid = _load_hid()
        device = hid.device()
        try:
            device.open_path(_path_for_hidapi(path))
        except Exception as exc:  # pragma: no cover - depends on local HID stack
            raise PlatformHidError(f"failed to open HID path {path!r}: {exc}") from exc
        return HidapiReportReader(device=device, path=path)


class HidapiReportReader:
    def __init__(self, device: Any, path: str) -> None:
        self._device = device
        self._path = path
        self._sequence = 0

    def read_report(self, timeout_ms: int | None = None) -> RawHidReport | None:
        try:
            data = self._device.read(64, timeout_ms or 1000)
        except Exception as exc:  # pragma: no cover - depends on local HID stack
            raise PlatformHidError(f"failed to read HID report from {self._path!r}: {exc}") from exc
        if not data:
            return None
        self._sequence += 1
        report_id = 0
        payload = tuple(int(byte) for byte in data)
        return RawHidReport(
            timestamp=time.time(),
            report_id=report_id,
            data=payload,
            sequence=self._sequence,
            device=DeviceIdentity(path=self._path),
            transport=TransportMode.UNKNOWN,
            connection_state=ConnectionState.CONNECTED,
        )

    def close(self) -> None:
        close = getattr(self._device, "close", None)
        if callable(close):
            close()


def _load_hid() -> Any:
    try:
        import hid  # type: ignore[import-not-found]
    except ImportError as exc:
        raise PlatformHidError(
            "HIDAPI is not installed. Run `just setup-hid` or install with "
            "`brew install hidapi && python3 -m pip install -e '.[hid]'`."
        ) from exc
    return hid


def _device_from_hidapi(item: dict[str, Any]) -> HidDeviceInfo:
    usage_page = _optional_int(item.get("usage_page"))
    usage = _optional_int(item.get("usage"))
    identity = DeviceIdentity(
        manufacturer=_optional_text(item.get("manufacturer_string")),
        product=_optional_text(item.get("product_string")),
        vendor_id=_optional_int(item.get("vendor_id")),
        product_id=_optional_int(item.get("product_id")),
        serial_number=_optional_text(item.get("serial_number")),
        path=_optional_text(item.get("path")),
        usage_page=usage_page,
        usage=usage,
        interface_number=_optional_int(item.get("interface_number")),
        transport=_transport_from_bus_type(item.get("bus_type")),
    )
    capability = DeviceCapability(
        usage_page=usage_page,
        usage=usage,
        vendor_defined=usage_page is not None and usage_page >= 0xFF00,
    )
    return HidDeviceInfo(identity=identity, capabilities=(capability,))


def _path_for_hidapi(path: str) -> str | bytes:
    return path.encode("utf-8")


def _optional_text(value: Any) -> str | None:
    if value is None:
        return None
    if isinstance(value, bytes):
        return value.decode(errors="replace")
    return str(value)


def _optional_int(value: Any) -> int | None:
    if value is None or value == "":
        return None
    return int(value)


def _transport_from_bus_type(value: Any) -> TransportMode:
    if value is None:
        return TransportMode.UNKNOWN
    normalized = str(value).lower()
    if "bluetooth" in normalized:
        return TransportMode.BLUETOOTH
    if "usb" in normalized:
        return TransportMode.USB
    return TransportMode.UNKNOWN
