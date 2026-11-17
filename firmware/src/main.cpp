#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "amp_control.h"
#include "ble_knob.h"
#include "bluos_client.h"
#include "config.h"
#include "config_store.h"
#include "decoder.h"
#include "display_ui.h"
#include "knob_event.h"
#include "timing.h"

namespace {

constexpr size_t kQueueDepth = 16;
constexpr const char* kKnobNameHint = "ANTICATER";
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kActionShowMs = 1500;
constexpr uint32_t kResultShowMs = 2500;
constexpr const char* kLivePhrase = "I understand this controls the amplifier";
KnobLink g_prev_link = KnobLink::Idle;

QueueHandle_t g_events = nullptr;
BleKnob g_knob;
knob::Config g_config;
std::string g_config_error;
UiState g_ui{};
knob::Dimmer g_dimmer(kDimAfterMs);
uint32_t g_action_at = 0;
std::string g_last_amp_key;
String g_line;
bool g_wifi_reported = false;
volatile int g_wifi_reason = 0;  // last STA disconnect reason (esp_wifi wifi_err_reason_t)

const char* wifi_reason_text(int reason) {
    switch (reason) {
        case 201: return "Wi-Fi network not found (2.4 GHz only)";
        case 2:
        case 15:
        case 202:
        case 204: return "Wi-Fi password rejected?";
        case 203: return "Wi-Fi: too many devices on AP";
        case 200: return "Wi-Fi: beacon timeout";
        default: return nullptr;
    }
}

std::string json_escape(const std::string& in) {
    std::string out;
    for (const char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string esc(const String& s) { return json_escape(std::string(s.c_str())); }

const char* board_name() {
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickC: return "StickC";
        case m5::board_t::board_M5StickCPlus: return "StickC+";
        case m5::board_t::board_M5StickCPlus2: return "StickC+2";
        default: return "M5";
    }
}

String hex(const uint8_t* data, size_t len) {
    static const char* digits = "0123456789abcdef";
    String out;
    for (size_t i = 0; i < len; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0F];
    }
    return out;
}

void handle_event(const KnobEvent& ev) {
    const knob::Decoded decoded = knob::decode_report(ev.data, ev.len);
    if (decoded.action == knob::Action::NoOp) return;
    g_ui.events++;
    g_ui.last_action = decoded.action;
    g_action_at = millis();
    g_dimmer.touch(g_action_at);
    control_submit(decoded.action, ev.at_ms);
    Serial.printf("{\"seq\":%lu,\"action\":\"%s\",\"raw\":\"%s\",\"usage\":\"0x%04x\",\"t_ms\":%lu}\n",
                  static_cast<unsigned long>(g_ui.events), knob::action_name(decoded.action),
                  hex(ev.data, ev.len).c_str(), decoded.usage, static_cast<unsigned long>(ev.at_ms));
}

void wifi_begin() {
    WiFi.disconnect(true);
    g_config_error = knob::validate_config(g_config);
    if (!g_config_error.empty()) {
        Serial.printf("{\"config\":\"invalid\",\"error\":\"%s\"}\n", g_config_error.c_str());
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);  // modem sleep is required for Wi-Fi/BLE coexistence
    WiFi.setAutoReconnect(true);
    WiFi.begin(g_config.wifi_ssid.c_str(), g_config.wifi_pass.c_str());
    g_wifi_reported = false;
    Serial.printf("{\"wifi\":\"connecting\",\"ssid\":\"%s\"}\n", json_escape(g_config.wifi_ssid).c_str());
}

void wifi_register_events() {
    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            g_wifi_reason = info.wifi_sta_disconnected.reason;
            Serial.printf("{\"wifi\":\"disconnected\",\"reason\":%d}\n", g_wifi_reason);
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void print_config() {
    Serial.printf(
        "{\"wifi_ssid\":\"%s\",\"wifi_pass\":\"%s\",\"host\":\"%s\",\"port\":%d,\"max_db\":%.1f,"
        "\"source_safe_db\":%.1f,\"step_db\":%.1f,\"optical_input\":\"%s\",\"spotify_source\":\"%s\",\"valid\":%s}\n",
        json_escape(g_config.wifi_ssid).c_str(), g_config.wifi_pass.empty() ? "" : "****",
        json_escape(g_config.host).c_str(), g_config.port, g_config.max_db, g_config.source_safe_db, g_config.step_db,
        json_escape(g_config.optical_input).c_str(), json_escape(g_config.spotify_source).c_str(), knob::validate_config(g_config).empty() ? "true" : "false");
}

void handle_command(const String& line) {
    std::string cmd(line.c_str());
    if (cmd.rfind("set ", 0) != 0) {  // set values keep trailing spaces; other commands ignore them
        while (!cmd.empty() && isspace(static_cast<unsigned char>(cmd.back()))) cmd.pop_back();
    }
    if (cmd.rfind("set ", 0) == 0) {
        const size_t space = cmd.find(' ', 4);
        const std::string key = cmd.substr(4, space == std::string::npos ? std::string::npos : space - 4);
        const std::string value = space == std::string::npos ? "" : cmd.substr(space + 1);
        const std::string error = knob::apply_setting(g_config, key, value);
        if (!error.empty()) {
            Serial.printf("{\"set\":\"%s\",\"ok\":false,\"error\":\"%s\"}\n", json_escape(key).c_str(),
                          json_escape(error).c_str());
        } else if (!config_save(g_config)) {
            Serial.printf("{\"set\":\"%s\",\"ok\":false,\"error\":\"could not save\"}\n", json_escape(key).c_str());
        } else {
            Serial.printf("{\"set\":\"%s\",\"ok\":true}\n", json_escape(key).c_str());  // value never echoed
        }
    } else if (cmd == "live off") {
        control_set_live(false);
    } else if (cmd.rfind("live on", 0) == 0) {
        const std::string phrase = cmd.size() > 8 ? cmd.substr(8) : "";
        if (!g_config_error.empty()) {
            Serial.printf("{\"live\":false,\"error\":\"%s\"}\n", json_escape(g_config_error).c_str());
        } else if (phrase != kLivePhrase) {
            Serial.printf("{\"live\":false,\"error\":\"type: live on %s\"}\n", kLivePhrase);
        } else {
            control_set_live(true);
        }
    } else if (cmd == "show") {
        print_config();
    } else if (cmd == "reboot") {
        Serial.println("{\"reboot\":true}");
        Serial.flush();
        ESP.restart();
    } else if (cmd == "wifi scan") {
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(true);  // stop the pending connect; a scan fails while one is in progress
        WiFi.mode(WIFI_STA);
        delay(300);
        const int n = WiFi.scanNetworks(false, true);
        Serial.printf("{\"wifi_scan\":%d}\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"auth\":%d}\n", esc(WiFi.SSID(i)).c_str(),
                          WiFi.RSSI(i), WiFi.channel(i), static_cast<int>(WiFi.encryptionType(i)));
        }
        WiFi.scanDelete();
        wifi_begin();
    } else if (cmd == "scan") {
        g_knob.diag_scan(15000);
    } else if (cmd == "bonds clear") {
        g_knob.clear_bonds();
    } else if (cmd == "status") {
        const AmpState amp = bluos_snapshot();
        Serial.printf(
            "{\"board\":\"%s\",\"knob\":\"%s\",\"peer\":\"%s\",\"wifi\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
            "\"amp\":\"%s\",\"db\":%.1f,\"mute\":%d,\"source\":\"%s\",\"amp_failures\":%lu,\"fault\":\"%s\","
            "\"events\":%lu,\"dropped\":%lu,\"reconnects\":%lu,\"live\":%d,\"control_dropped\":%lu,"
            "\"free_heap\":%lu,\"min_free_heap\":%lu}\n",
            board_name(), knob_link_name(g_knob.link()), esc(g_knob.peer_name()).c_str(),
            WiFi.status() == WL_CONNECTED ? "connected" : "down", WiFi.localIP().toString().c_str(),
            WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0, amp.reachable ? "ok" : "unreachable", amp.volume.db,
            amp.volume.mute, json_escape(amp.status.title).c_str(), static_cast<unsigned long>(amp.failures),
            json_escape(g_ui.fault).c_str(), static_cast<unsigned long>(g_ui.events),
            static_cast<unsigned long>(g_knob.overflow_count()), static_cast<unsigned long>(g_knob.reconnects()),
            control_live(), static_cast<unsigned long>(control_dropped()), static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()));
    } else if (!cmd.empty()) {
        Serial.println(
            "{\"error\":\"unknown command\",\"commands\":[\"status\",\"show\",\"set <key> <value>\",\"reboot\","
            "\"scan\",\"wifi scan\",\"bonds clear\",\"live on <phrase>\",\"live off\"]}");
    }
}

void poll_serial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            // Only leading whitespace is dropped: `set` values (e.g. passphrases) may end in spaces.
            while (g_line.length() > 0 && isspace(static_cast<unsigned char>(g_line[0]))) g_line.remove(0, 1);
            handle_command(g_line);
            g_line = "";
        } else if (g_line.length() < 160) {
            g_line += c;
        }
    }
}

std::string current_fault(const AmpState& amp) {
    if (!g_config_error.empty()) return g_config_error;
    if (WiFi.status() != WL_CONNECTED) {
        const char* why = wifi_reason_text(g_wifi_reason);
        return why ? std::string(why) : std::string("Wi-Fi: connecting to ") + g_config.wifi_ssid;
    }
    if (!amp.reachable && amp.failures > 0) return amp.error;
    if (g_knob.link() != KnobLink::Connected) return "knob not connected";
    return "";
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    g_events = xQueueCreate(kQueueDepth, sizeof(KnobEvent));
    g_ui.board = board_name();
    g_ui.max_db = -24;
    g_ui.last_action = knob::Action::NoOp;
    config_load(g_config);
    g_ui.max_db = g_config.max_db;
    ui_begin();
    Serial.printf("{\"boot\":\"bluos-knob\",\"board\":\"%s\",\"free_heap\":%lu}\n", g_ui.board,
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    g_knob.begin(g_events, kKnobNameHint);
    wifi_register_events();
    wifi_begin();
    if (g_config_error.empty()) {
        bluos_start(g_config.host, g_config.port);
        control_start(g_config);
    }
    Serial.printf("{\"live\":%s,\"note\":\"remembered across restarts; live off to stop\"}\n",
                  control_live() ? "true" : "false");
    g_dimmer.touch(millis());
}

void loop() {
    M5.update();
    g_knob.loop();
    KnobEvent ev;
    while (xQueueReceive(g_events, &ev, 0) == pdTRUE) handle_event(ev);
    poll_serial();

    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED && !g_wifi_reported) {
        g_wifi_reported = true;
        g_wifi_reason = 0;
        Serial.printf("{\"wifi\":\"connected\",\"ip\":\"%s\",\"rssi\":%d}\n", WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
    } else if (WiFi.status() != WL_CONNECTED && g_wifi_reported) {
        g_wifi_reported = false;
        Serial.println("{\"wifi\":\"lost\"}");
    }

    g_ui.amp = bluos_snapshot();
    char amp_key[48];
    snprintf(amp_key, sizeof(amp_key), "%.1f|%d|", g_ui.amp.volume.db, g_ui.amp.volume.mute);
    const std::string key = std::string(amp_key) + g_ui.amp.status.title;
    if (g_ui.amp.have_data && key != g_last_amp_key) {
        if (!g_last_amp_key.empty()) g_dimmer.touch(now);  // wake on volume/source change
        g_last_amp_key = key;
    }

    g_ui.link = g_knob.link();
    if (g_ui.link != g_prev_link) {  // queued gestures never survive a knob reconnect
        control_flush();
        g_prev_link = g_ui.link;
    }
    g_ui.live = control_live();
    g_ui.control = control_last();
    g_ui.show_result = g_ui.control.valid && (now - g_ui.control.at_ms) < kResultShowMs;
    if (g_ui.show_result) g_dimmer.touch(now);
    g_ui.wifi = !g_config_error.empty() ? WifiLink::NotConfigured
                : WiFi.status() == WL_CONNECTED ? WifiLink::Connected
                                                : WifiLink::Connecting;
    g_ui.fault = current_fault(g_ui.amp);
    g_ui.show_action = (now - g_action_at) < kActionShowMs && g_ui.events > 0;
    g_ui.dimmed = g_dimmer.dimmed(now) && g_ui.fault.empty();
    // The screen is for debugging: never redraw while knob events or amp commands are pending, so a
    // full-screen redraw can't delay draining the knob queue. One redraw happens once it's idle.
    if (uxQueueMessagesWaiting(g_events) == 0 && !control_busy()) ui_render(g_ui);
    delay(10);
}
