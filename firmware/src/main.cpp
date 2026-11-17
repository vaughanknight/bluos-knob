#include <Arduino.h>
#include <M5Unified.h>

#include "ble_knob.h"
#include "decoder.h"
#include "display_ui.h"
#include "knob_event.h"

namespace {

constexpr size_t kQueueDepth = 16;
constexpr const char* kKnobNameHint = "ANTICATER";

QueueHandle_t g_events = nullptr;
BleKnob g_knob;
UiState g_ui{};
String g_line;

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
    Serial.printf("{\"seq\":%lu,\"action\":\"%s\",\"raw\":\"%s\",\"usage\":\"0x%04x\",\"t_ms\":%lu}\n",
                  static_cast<unsigned long>(g_ui.events), knob::action_name(decoded.action),
                  hex(ev.data, ev.len).c_str(), decoded.usage, static_cast<unsigned long>(ev.at_ms));
}

void handle_command(const String& cmd) {
    if (cmd == "bonds clear") {
        g_knob.clear_bonds();
    } else if (cmd == "status") {
        Serial.printf("{\"board\":\"%s\",\"knob\":\"%s\",\"peer\":\"%s\",\"events\":%lu,\"dropped\":%lu,"
                      "\"reconnects\":%lu,\"free_heap\":%lu,\"min_free_heap\":%lu}\n",
                      board_name(), knob_link_name(g_knob.link()), g_knob.peer_name().c_str(),
                      static_cast<unsigned long>(g_ui.events),
                      static_cast<unsigned long>(g_knob.overflow_count()),
                      static_cast<unsigned long>(g_knob.reconnects()),
                      static_cast<unsigned long>(ESP.getFreeHeap()),
                      static_cast<unsigned long>(ESP.getMinFreeHeap()));
    } else if (cmd.length() > 0) {
        Serial.printf("{\"error\":\"unknown command\",\"commands\":[\"status\",\"bonds clear\"]}\n");
    }
}

void poll_serial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            g_line.trim();
            handle_command(g_line);
            g_line = "";
        } else if (g_line.length() < 128) {
            g_line += c;
        }
    }
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    g_events = xQueueCreate(kQueueDepth, sizeof(KnobEvent));
    g_ui.board = board_name();
    g_ui.last_action = knob::Action::NoOp;
    ui_begin();
    Serial.printf("{\"boot\":\"bluos-knob\",\"board\":\"%s\",\"free_heap\":%lu}\n", g_ui.board,
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    g_knob.begin(g_events, kKnobNameHint);
}

void loop() {
    M5.update();
    g_knob.loop();
    KnobEvent ev;
    while (xQueueReceive(g_events, &ev, 0) == pdTRUE) handle_event(ev);
    poll_serial();
    g_ui.link = g_knob.link();
    g_ui.overflow = g_knob.overflow_count();
    ui_render(g_ui);
    delay(10);
}
