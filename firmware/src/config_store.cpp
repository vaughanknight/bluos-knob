#include "config_store.h"

#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "bluos-knob";
}

void config_load(knob::Config& c) {
    Preferences p;
    p.begin(kNamespace, true);
    c.wifi_ssid = p.getString("wifi_ssid", c.wifi_ssid.c_str()).c_str();
    c.wifi_pass = p.getString("wifi_pass", c.wifi_pass.c_str()).c_str();
    c.host = p.getString("host", c.host.c_str()).c_str();
    c.port = p.getInt("port", c.port);
    c.max_db = p.getDouble("max_db", c.max_db);
    c.source_safe_db = p.getDouble("src_safe_db", c.source_safe_db);
    c.step_db = p.getDouble("step_db", c.step_db);
    c.optical_input = p.getString("optical_input", c.optical_input.c_str()).c_str();
    c.spotify_source = p.getString("spotify_src", c.spotify_source.c_str()).c_str();
    p.end();
}

bool config_save(const knob::Config& c) {
    Preferences p;
    if (!p.begin(kNamespace, false)) return false;
    bool ok = p.putString("wifi_ssid", c.wifi_ssid.c_str()) == c.wifi_ssid.size();
    ok = (p.putString("wifi_pass", c.wifi_pass.c_str()) == c.wifi_pass.size()) && ok;
    ok = (p.putString("host", c.host.c_str()) == c.host.size()) && ok;
    ok = p.putInt("port", c.port) > 0 && ok;
    ok = p.putDouble("max_db", c.max_db) > 0 && ok;
    ok = p.putDouble("src_safe_db", c.source_safe_db) > 0 && ok;
    ok = p.putDouble("step_db", c.step_db) > 0 && ok;
    ok = p.putString("optical_input", c.optical_input.c_str()) == c.optical_input.size() && ok;
    ok = p.putString("spotify_src", c.spotify_source.c_str()) == c.spotify_source.size() && ok;
    p.end();
    return ok;
}

bool live_load() {
    Preferences p;
    p.begin(kNamespace, true);
    const bool live = p.getBool("live", false);
    p.end();
    return live;
}

bool live_save(bool live) {
    Preferences p;
    if (!p.begin(kNamespace, false)) return false;
    const bool ok = p.putBool("live", live) == 1;
    p.end();
    return ok;
}
