#pragma once

#include <cstddef>
#include <cstdint>

// Raw knob report handed from the BLE host task to the app task.
struct KnobEvent {
    uint32_t at_ms;
    uint8_t len;
    uint8_t data[8];
};
