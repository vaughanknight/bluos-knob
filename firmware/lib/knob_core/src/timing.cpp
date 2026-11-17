#include "timing.h"

namespace knob {

uint32_t Backoff::on_failure() {
    const uint32_t wait = next_;
    next_ = (next_ > max_ / 2) ? max_ : next_ * 2;
    return wait;
}

void Backoff::on_success() { next_ = min_; }

void Dimmer::touch(uint32_t now_ms) { last_ = now_ms; }

bool Dimmer::dimmed(uint32_t now_ms) const { return static_cast<uint32_t>(now_ms - last_) >= after_; }

}  // namespace knob
