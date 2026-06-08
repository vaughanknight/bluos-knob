"""Anticater candidate selection profiles."""

from __future__ import annotations

from dataclasses import dataclass

from bluos_knob.device_input.contracts import HidDeviceInfo


@dataclass(frozen=True)
class AnticaterProfile:
    product_contains: tuple[str, ...] = ("anticater", "vk-01", "vk01", "volume")
    manufacturer_contains: tuple[str, ...] = ("anticater",)
    vendor_id: int | None = None
    product_id: int | None = None
    usage_page: int | None = None
    usage: int | None = None

    def matches(self, device: HidDeviceInfo) -> bool:
        identity = device.identity
        if self.vendor_id is not None and identity.vendor_id != self.vendor_id:
            return False
        if self.product_id is not None and identity.product_id != self.product_id:
            return False
        if self.usage_page is not None and identity.usage_page != self.usage_page:
            return False
        if self.usage is not None and identity.usage != self.usage:
            return False

        product = (identity.product or "").lower()
        manufacturer = (identity.manufacturer or "").lower()
        has_product_hint = any(fragment in product for fragment in self.product_contains)
        has_manufacturer_hint = any(
            fragment in manufacturer for fragment in self.manufacturer_contains
        )
        return has_product_hint or has_manufacturer_hint or self._has_exact_hardware_filter()

    def _has_exact_hardware_filter(self) -> bool:
        return any(
            value is not None
            for value in (self.vendor_id, self.product_id, self.usage_page, self.usage)
        )


def default_anticater_profile() -> AnticaterProfile:
    return AnticaterProfile()

