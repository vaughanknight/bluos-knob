#include <unity.h>

#include "config.h"

using knob::Config;

static Config valid() {
    Config c;
    c.wifi_ssid = "home";
    c.wifi_pass = "secret";
    c.host = "192.168.1.67";
    return c;
}

void setUp() {}
void tearDown() {}

void given_complete_safe_config_when_validated_then_ok() {
    /*
    Test Doc:
    - Why: A complete, safe configuration must be accepted or the device never joins Wi-Fi.
    - Contract: validate_config returns "" for defaults plus SSID, password and host.
    - Usage Notes: The NVS adapter validates on boot; invalid config keeps the device offline and says why.
    - Quality Contribution: Catches over-strict validation.
    - Worked Example: ssid=home, pass=secret, host=192.168.1.67, defaults -> "".
    */
    TEST_ASSERT_EQUAL_STRING("", knob::validate_config(valid()).c_str());
}

void given_missing_network_or_host_when_validated_then_plain_error() {
    /*
    Test Doc:
    - Why: The screen shows the first config problem in plain words.
    - Contract: Empty SSID, empty host, or an out-of-range port is rejected with a non-empty message.
    - Usage Notes: Messages are shown verbatim on screen and serial.
    - Quality Contribution: Catches a device that tries to connect with nothing configured.
    - Worked Example: ssid="" -> "Wi-Fi not set up (run just fw-wifi)".
    */
    Config c = valid();
    c.wifi_ssid = "";
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c = valid();
    c.host = "";
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c = valid();
    c.port = 0;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c.port = 70000;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
}

void given_unsafe_volume_limits_when_validated_then_rejected() {
    /*
    Test Doc:
    - Why: Safety-first (constitution I): limits above -24 dB or oversized steps could make loud jumps.
    - Contract: max_db and source_safe_db must be <= -24; step_db must be in (0, 3].
    - Usage Notes: Mirrors scripts/bluos_source.py safe_abs_db_value (-24 cap); max_db and step bounds are firmware tightenings.
    - Quality Contribution: Catches unsafe config reaching the amp policy.
    - Worked Example: max_db=-20 -> error; source_safe_db=-10 -> error; step_db=5 -> error; step_db=0 -> error.
    */
    Config c = valid();
    c.max_db = -20;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c = valid();
    c.source_safe_db = -10;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c = valid();
    c.step_db = 5;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
    c.step_db = 0;
    TEST_ASSERT_TRUE(knob::validate_config(c).size() > 0);
}

void given_serial_set_commands_when_applied_then_valid_values_stick_and_invalid_are_refused() {
    /*
    Test Doc:
    - Why: `just fw-wifi` and the serial console configure the device one key at a time.
    - Contract: apply_setting updates the key on success; on an invalid value or unknown key it returns an error and changes nothing.
    - Usage Notes: Values may contain spaces (SSIDs, passwords); numbers must parse fully.
    - Quality Contribution: Catches partial writes and lenient number parsing ("-20dB").
    - Worked Example: set max_db -30 -> ok; set max_db -10 -> refused (still -30); set port 12x -> refused.
    */
    Config c = valid();
    TEST_ASSERT_EQUAL_STRING("", knob::apply_setting(c, "wifi_ssid", "My Home Net").c_str());
    TEST_ASSERT_EQUAL_STRING("My Home Net", c.wifi_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", knob::apply_setting(c, "max_db", "-30").c_str());
    TEST_ASSERT_EQUAL_DOUBLE(-30, c.max_db);
    TEST_ASSERT_TRUE(knob::apply_setting(c, "max_db", "-10").size() > 0);
    TEST_ASSERT_EQUAL_DOUBLE(-30, c.max_db);
    TEST_ASSERT_TRUE(knob::apply_setting(c, "max_db", "-20dB").size() > 0);
    TEST_ASSERT_TRUE(knob::apply_setting(c, "port", "12x").size() > 0);
    TEST_ASSERT_EQUAL_INT(11000, c.port);
    TEST_ASSERT_TRUE(knob::apply_setting(c, "colour", "blue").size() > 0);
}

void given_keys_when_checked_then_only_password_is_secret() {
    /*
    Test Doc:
    - Why: The Wi-Fi password must never be echoed to serial or screen.
    - Contract: is_secret_key is true for wifi_pass only.
    - Usage Notes: The console masks secret values in `show`.
    - Quality Contribution: Catches a password leaking through `show`.
    - Worked Example: wifi_pass -> true; wifi_ssid -> false.
    */
    TEST_ASSERT_TRUE(knob::is_secret_key("wifi_pass"));
    TEST_ASSERT_FALSE(knob::is_secret_key("wifi_ssid"));
    TEST_ASSERT_FALSE(knob::is_secret_key("host"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(given_complete_safe_config_when_validated_then_ok);
    RUN_TEST(given_missing_network_or_host_when_validated_then_plain_error);
    RUN_TEST(given_unsafe_volume_limits_when_validated_then_rejected);
    RUN_TEST(given_serial_set_commands_when_applied_then_valid_values_stick_and_invalid_are_refused);
    RUN_TEST(given_keys_when_checked_then_only_password_is_secret);
    return UNITY_END();
}
