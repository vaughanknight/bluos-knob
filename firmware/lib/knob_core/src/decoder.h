// Portable knob report decoder: raw HID consumer-control bytes -> knob action.
// Mirrors src/bluos_knob/device_input/normalizer.py. No Arduino dependencies.
#pragma once

#include <cstddef>
#include <cstdint>

namespace knob {

enum class Action {
    Unknown,
    NoOp,  // key release
    VolumeUp,
    VolumeDown,
    MuteToggle,
    BrightnessUp,
    BrightnessDown,
};

struct Decoded {
    Action action;
    uint16_t usage;  // matched consumer usage, 0 when none
};

Decoded decode_report(const uint8_t* data, size_t len);
const char* action_name(Action action);

}  // namespace knob
