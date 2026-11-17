#include "decoder.h"

namespace knob {
namespace {

struct UsageMapping {
    uint16_t usage;
    Action action;
};

// Same table as normalizer.py CONSUMER_USAGE_TO_ACTION.
constexpr UsageMapping kUsageTable[] = {
    {0x00E9, Action::VolumeUp},     {0x00EA, Action::VolumeDown},
    {0x00E2, Action::MuteToggle},   {0x006F, Action::BrightnessUp},
    {0x0070, Action::BrightnessDown},
};

bool is_release(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return true;
    bool rest_zero = true;
    for (size_t i = 1; i < len; ++i) rest_zero = rest_zero && data[i] == 0;
    if (data[0] == 0 && rest_zero) return true;
    return len > 2 && data[0] != 0 && rest_zero;
}

bool lookup(uint16_t usage, Action* out) {
    for (const auto& m : kUsageTable) {
        if (m.usage == usage) {
            *out = m.action;
            return true;
        }
    }
    return false;
}

}  // namespace

Decoded decode_report(const uint8_t* data, size_t len) {
    if (is_release(data, len)) return {Action::NoOp, 0};
    for (size_t i = 0; i + 1 < len; ++i) {
        const uint16_t candidate = static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
        Action action;
        if (lookup(candidate, &action)) return {action, candidate};
    }
    return {Action::Unknown, 0};
}

const char* action_name(Action action) {
    switch (action) {
        case Action::VolumeUp: return "volume_up";
        case Action::VolumeDown: return "volume_down";
        case Action::MuteToggle: return "mute_toggle";
        case Action::BrightnessUp: return "brightness_up";
        case Action::BrightnessDown: return "brightness_down";
        case Action::NoOp: return "no_op";
        case Action::Unknown: break;
    }
    return "unknown";
}

}  // namespace knob
