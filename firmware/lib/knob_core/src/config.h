// Device configuration and its validation. Portable; persisted by the NVS adapter.
#pragma once

#include <string>

namespace knob {

struct Config {
    std::string wifi_ssid;
    std::string wifi_pass;
    std::string host;
    int port = 11000;
    double max_db = -24;
    double source_safe_db = -40;
    double step_db = 1;
    std::string optical_input = "optical-2";
    std::string spotify_source = "Spotify";
};

// Empty string when valid, else the first problem in plain words.
std::string validate_config(const Config& config);

// Applies one `set <key> <value>`; leaves config untouched and returns an error when the value is invalid.
std::string apply_setting(Config& config, const std::string& key, const std::string& value);

bool is_secret_key(const std::string& key);

}  // namespace knob
