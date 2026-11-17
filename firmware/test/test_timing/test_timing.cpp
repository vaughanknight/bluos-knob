#include <unity.h>

#include "timing.h"

void setUp() {}
void tearDown() {}

void given_repeated_failures_when_backing_off_then_doubles_to_cap_and_resets_on_success() {
    /*
    Test Doc:
    - Why: rules.md: the daemon MUST handle amplifier unreachability without retry storms.
    - Contract: on_failure returns min, 2*min, 4*min ... capped at max; on_success resets to min.
    - Usage Notes: Use the returned value as the wait before the next poll.
    - Quality Contribution: Catches tight retry loops when the amp is off.
    - Worked Example: min 1000, max 8000 -> 1000, 2000, 4000, 8000, 8000; success -> 1000.
    */
    knob::Backoff b(1000, 8000);
    TEST_ASSERT_EQUAL_UINT32(1000, b.on_failure());
    TEST_ASSERT_EQUAL_UINT32(2000, b.on_failure());
    TEST_ASSERT_EQUAL_UINT32(4000, b.on_failure());
    TEST_ASSERT_EQUAL_UINT32(8000, b.on_failure());
    TEST_ASSERT_EQUAL_UINT32(8000, b.on_failure());
    b.on_success();
    TEST_ASSERT_EQUAL_UINT32(1000, b.on_failure());
}

void given_inactivity_when_time_passes_then_dims_and_wakes_on_touch() {
    /*
    Test Doc:
    - Why: AC-09: the screen dims after inactivity and wakes on the next knob event or status change.
    - Contract: dimmed(now) is true once now - last touch >= dim_after; touch() wakes it; millis() wrap is handled.
    - Usage Notes: Call touch() on knob events and on amp volume/source changes.
    - Quality Contribution: Catches a screen that never dims or never wakes, and the 49-day millis() wrap.
    - Worked Example: dim_after 30000; touch at 0 -> not dimmed at 29999, dimmed at 30000; touch at 31000 -> awake.
    */
    knob::Dimmer d(30000);
    d.touch(0);
    TEST_ASSERT_FALSE(d.dimmed(29999));
    TEST_ASSERT_TRUE(d.dimmed(30000));
    d.touch(31000);
    TEST_ASSERT_FALSE(d.dimmed(31000));
    knob::Dimmer w(30000);
    w.touch(0xFFFFF000u);
    TEST_ASSERT_FALSE(w.dimmed(0x00000100u));
    TEST_ASSERT_TRUE(w.dimmed(0xFFFFF000u + 30000u));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(given_repeated_failures_when_backing_off_then_doubles_to_cap_and_resets_on_success);
    RUN_TEST(given_inactivity_when_time_passes_then_dims_and_wakes_on_touch);
    return UNITY_END();
}
