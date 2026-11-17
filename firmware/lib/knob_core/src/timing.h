// Clock-free timing helpers; callers pass millis() so tests can drive time.
#pragma once

#include <cstdint>

namespace knob {

class Backoff {
   public:
    Backoff(uint32_t min_ms, uint32_t max_ms) : min_(min_ms), max_(max_ms), next_(min_ms) {}
    uint32_t on_failure();  // returns the delay to wait before the next attempt
    void on_success();
    uint32_t next() const { return next_; }

   private:
    uint32_t min_, max_, next_;
};

class Dimmer {
   public:
    explicit Dimmer(uint32_t dim_after_ms) : after_(dim_after_ms) {}
    void touch(uint32_t now_ms);
    bool dimmed(uint32_t now_ms) const;

   private:
    uint32_t after_;
    uint32_t last_ = 0;
};

}  // namespace knob
