#include "config.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace knob {
namespace {

bool parse_number(const std::string& text, double* out) {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end != text.c_str() + text.size() || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

}  // namespace

std::string validate_config(const Config& c) {
    if (c.wifi_ssid.empty()) return "Wi-Fi not set up (run just fw-wifi)";
    if (c.wifi_ssid.size() > 32) return "Wi-Fi name longer than 32 characters";
    if (c.wifi_pass.size() > 64) return "Wi-Fi password longer than 64 characters";
    if (c.host.empty()) return "amp address not set (run just fw-wifi)";
    if (c.port < 1 || c.port > 65535) return "amp port must be 1-65535";
    if (c.max_db > -24) return "max_db must be -24 dB or lower";
    if (c.source_safe_db > -24) return "source_safe_db must be -24 dB or lower";
    if (!(c.step_db > 0 && c.step_db <= 3)) return "step_db must be above 0 and at most 3 dB";
    if (c.optical_input.empty()) return "optical_input not set";
    if (c.spotify_source.empty()) return "spotify_source not set";
    return "";
}

std::string apply_setting(Config& config, const std::string& key, const std::string& value) {
    Config next = config;
    double number = 0;
    if (key == "wifi_ssid") {
        next.wifi_ssid = value;
        if (value.empty() || value.size() > 32) return "wifi_ssid must be 1-32 characters";
    } else if (key == "wifi_pass") {
        next.wifi_pass = value;
        if (value.size() > 64) return "wifi_pass must be at most 64 characters";
    } else if (key == "host") {
        next.host = value;
        if (value.empty()) return "host must not be empty";
    } else if (key == "port") {
        if (!parse_number(value, &number) || number != std::floor(number) || number < 1 || number > 65535)
            return "port must be a whole number 1-65535";
        next.port = static_cast<int>(number);
    } else if (key == "max_db" || key == "source_safe_db" || key == "step_db") {
        if (!parse_number(value, &number)) return key + " must be a number";
        if (key == "max_db") next.max_db = number;
        if (key == "source_safe_db") next.source_safe_db = number;
        if (key == "step_db") next.step_db = number;
        if (key == "max_db" && number > -24) return "max_db must be -24 dB or lower";
        if (key == "source_safe_db" && number > -24) return "source_safe_db must be -24 dB or lower";
        if (key == "step_db" && !(number > 0 && number <= 3)) return "step_db must be above 0 and at most 3 dB";
    } else if (key == "optical_input") {
        if (value.empty()) return "optical_input must not be empty";
        next.optical_input = value;
    } else if (key == "spotify_source") {
        if (value.empty()) return "spotify_source must not be empty";
        next.spotify_source = value;
    } else {
        return "unknown setting: " + key;
    }
    config = next;
    return "";
}

bool is_secret_key(const std::string& key) { return key == "wifi_pass"; }

}  // namespace knob
