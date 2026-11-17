#include <unity.h>

#include <initializer_list>
#include <vector>

#include "decoder.h"

using knob::Action;
using knob::decode_report;

static knob::Decoded decode(std::initializer_list<uint8_t> bytes) {
    std::vector<uint8_t> v(bytes);
    return decode_report(v.data(), v.size());
}

void setUp() {}
void tearDown() {}

void given_real_capture_reports_with_report_id_when_decoded_then_match_python_normalizer() {
    /*
    Test Doc:
    - Why: The ESP32 must interpret the knob exactly like the Mac daemon, or a turn could mean the wrong command.
    - Contract: Report-id-3 payloads map to the same actions as normalizer.py CONSUMER_USAGE_TO_ACTION.
    - Usage Notes: Bytes are the raw report as captured on macOS (report id first, little-endian usage).
    - Quality Contribution: Catches table drift between the Python and firmware decoders.
    - Worked Example: 03 e9 00 -> VolumeUp (usage 0x00E9); 03 70 00 -> BrightnessDown.
    */
    TEST_ASSERT_EQUAL(Action::VolumeUp, decode({0x03, 0xE9, 0x00}).action);
    TEST_ASSERT_EQUAL_HEX16(0x00E9, decode({0x03, 0xE9, 0x00}).usage);
    TEST_ASSERT_EQUAL(Action::VolumeDown, decode({0x03, 0xEA, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::MuteToggle, decode({0x03, 0xE2, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::BrightnessUp, decode({0x03, 0x6F, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::BrightnessDown, decode({0x03, 0x70, 0x00}).action);
}

void given_ble_payload_without_report_id_when_decoded_then_same_actions() {
    /*
    Test Doc:
    - Why: BLE HID notifications carry the report id out of band, so payloads arrive without it.
    - Contract: The decoder finds the usage whether or not a leading report id is present.
    - Usage Notes: Pass the notification value bytes unchanged.
    - Quality Contribution: Catches a decoder that assumes a fixed offset.
    - Worked Example: e9 00 -> VolumeUp; ea 00 -> VolumeDown.
    */
    TEST_ASSERT_EQUAL(Action::VolumeUp, decode({0xE9, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::VolumeDown, decode({0xEA, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::MuteToggle, decode({0xE2, 0x00}).action);
}

void given_release_reports_when_decoded_then_noop() {
    /*
    Test Doc:
    - Why: Every press is followed by a release; treating it as a command would double-fire.
    - Contract: Empty, all-zero, and report-id-then-zeros payloads are NoOp with no usage.
    - Usage Notes: Matches normalizer.py _is_release_report.
    - Quality Contribution: Catches duplicate volume steps from key-up reports.
    - Worked Example: 03 00 00 -> NoOp; 00 00 -> NoOp; empty -> NoOp.
    */
    TEST_ASSERT_EQUAL(Action::NoOp, decode({0x03, 0x00, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::NoOp, decode({0x00, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::NoOp, decode_report(nullptr, 0).action);
    TEST_ASSERT_EQUAL_HEX16(0, decode({0x03, 0x00, 0x00}).usage);
}

void given_unmapped_or_truncated_reports_when_decoded_then_unknown() {
    /*
    Test Doc:
    - Why: Unexpected input must never turn into an amplifier command.
    - Contract: Non-release payloads without a mapped usage decode to Unknown.
    - Usage Notes: Unknown is ignored by policy; it is logged for diagnosis.
    - Quality Contribution: Catches a decoder that defaults to a real action.
    - Worked Example: 03 cd 00 (play/pause, unmapped) -> Unknown; single byte e9 -> Unknown.
    */
    TEST_ASSERT_EQUAL(Action::Unknown, decode({0x03, 0xCD, 0x00}).action);
    TEST_ASSERT_EQUAL(Action::Unknown, decode({0xE9}).action);
}

void given_actions_when_named_then_match_python_action_values() {
    /*
    Test Doc:
    - Why: Serial log lines should read the same as the Mac daemon's JSON logs.
    - Contract: action_name returns normalizer.py NormalizedAction values.
    - Usage Notes: Used in the serial action line and on screen.
    - Quality Contribution: Keeps logs comparable across both bridges.
    - Worked Example: VolumeUp -> "volume_up".
    */
    TEST_ASSERT_EQUAL_STRING("volume_up", knob::action_name(Action::VolumeUp));
    TEST_ASSERT_EQUAL_STRING("volume_down", knob::action_name(Action::VolumeDown));
    TEST_ASSERT_EQUAL_STRING("mute_toggle", knob::action_name(Action::MuteToggle));
    TEST_ASSERT_EQUAL_STRING("brightness_up", knob::action_name(Action::BrightnessUp));
    TEST_ASSERT_EQUAL_STRING("brightness_down", knob::action_name(Action::BrightnessDown));
    TEST_ASSERT_EQUAL_STRING("no_op", knob::action_name(Action::NoOp));
    TEST_ASSERT_EQUAL_STRING("unknown", knob::action_name(Action::Unknown));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(given_real_capture_reports_with_report_id_when_decoded_then_match_python_normalizer);
    RUN_TEST(given_ble_payload_without_report_id_when_decoded_then_same_actions);
    RUN_TEST(given_release_reports_when_decoded_then_noop);
    RUN_TEST(given_unmapped_or_truncated_reports_when_decoded_then_unknown);
    RUN_TEST(given_actions_when_named_then_match_python_action_values);
    return UNITY_END();
}
