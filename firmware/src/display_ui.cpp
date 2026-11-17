#include "display_ui.h"

#include <M5Unified.h>

namespace {

constexpr uint8_t kBright = 128;
constexpr uint8_t kDim = 8;

std::string g_last_key;
bool g_dimmed_drawn = false;

uint16_t knob_colour(KnobLink link) {
    switch (link) {
        case KnobLink::Connected: return TFT_GREEN;
        case KnobLink::Scanning:
        case KnobLink::Connecting: return TFT_YELLOW;
        default: return TFT_RED;
    }
}

uint16_t wifi_colour(WifiLink wifi) {
    switch (wifi) {
        case WifiLink::Connected: return TFT_GREEN;
        case WifiLink::Connecting: return TFT_YELLOW;
        default: return TFT_RED;
    }
}

const char* action_label(knob::Action action) {
    switch (action) {
        case knob::Action::VolumeUp: return "VOL +";
        case knob::Action::VolumeDown: return "VOL -";
        case knob::Action::MuteToggle: return "MUTE";
        case knob::Action::BrightnessUp: return "Spotify";
        case knob::Action::BrightnessDown: return "Optical";
        default: return "";
    }
}

std::string db_text(const AmpState& amp) {
    if (!amp.have_data) return "--";
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", amp.volume.db);
    std::string s(buf);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
    return s;
}

std::string source_text(const AmpState& amp) {
    if (!amp.have_data) return "";
    if (!amp.status.title.empty()) return amp.status.title;
    return amp.status.service;
}

std::string render_key(const UiState& s) {
    char buf[32];
    snprintf(buf, sizeof(buf), "|%d|%d|%d|%d|%d|%d", static_cast<int>(s.link), static_cast<int>(s.wifi),
             s.amp.reachable, s.amp.volume.mute, static_cast<int>(s.last_action), s.show_action);
    return db_text(s.amp) + source_text(s.amp) + s.fault + buf;
}

}  // namespace

void ui_begin() {
    M5.Display.setRotation(1);
    M5.Display.setBrightness(kBright);
    M5.Display.fillScreen(TFT_BLACK);
}

void ui_render(const UiState& s) {
    auto& d = M5.Display;
    if (s.dimmed != g_dimmed_drawn) {
        d.setBrightness(s.dimmed ? kDim : kBright);
        g_dimmed_drawn = s.dimmed;
    }
    const std::string key = render_key(s);
    if (key == g_last_key) return;
    g_last_key = key;

    const int w = d.width();
    const int h = d.height();
    const bool small = h < 100;  // StickC 80x160 vs StickC Plus 135x240

    d.startWrite();
    d.fillScreen(TFT_BLACK);

    // Status bar: Wi-Fi left, knob right, 16 px proportional font.
    d.setTextSize(1);
    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.setTextDatum(textdatum_t::middle_left);
    d.fillCircle(8, 9, 5, wifi_colour(s.wifi));
    d.drawString("Wi-Fi", 18, 9);
    d.setTextDatum(textdatum_t::middle_right);
    d.fillCircle(w - 8, 9, 5, knob_colour(s.link));
    d.drawString("Knob", w - 18, 9);

    // Centre: big dB (or the knob action briefly, or MUTED).
    const int cy = h / 2 - (small ? 0 : 2);
    d.setTextDatum(textdatum_t::middle_center);
    if (s.show_action && s.last_action != knob::Action::NoOp && s.last_action != knob::Action::Unknown) {
        d.setFont(&fonts::Font4);
        d.setTextSize(small ? 1 : 2);
        d.setTextColor(TFT_CYAN, TFT_BLACK);
        d.drawString(action_label(s.last_action), w / 2, cy);
    } else if (s.amp.have_data && s.amp.volume.mute) {
        d.setFont(&fonts::Font4);
        d.setTextSize(small ? 1 : 2);
        d.setTextColor(TFT_ORANGE, TFT_BLACK);
        d.drawString("MUTED", w / 2, cy);
    } else {
        const std::string db = db_text(s.amp);
        d.setFont(small ? &fonts::Font4 : &fonts::Font7);
        d.setTextSize(1);
        d.setTextColor(s.amp.reachable ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
        const int db_w = d.textWidth(db.c_str());
        d.setFont(small ? &fonts::Font2 : &fonts::Font4);
        const int unit_w = d.textWidth("dB");
        const int left = (w - db_w - 4 - unit_w) / 2;
        d.setFont(small ? &fonts::Font4 : &fonts::Font7);
        d.setTextDatum(textdatum_t::middle_left);
        d.drawString(db.c_str(), left, cy);
        d.setFont(small ? &fonts::Font2 : &fonts::Font4);
        d.setTextDatum(textdatum_t::bottom_left);
        d.drawString("dB", left + db_w + 4, cy + (small ? 9 : 22));
    }

    // Source line.
    d.setFont(small ? &fonts::Font2 : &fonts::Font4);
    d.setTextSize(1);
    d.setTextDatum(textdatum_t::middle_center);
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.drawString(source_text(s.amp).substr(0, small ? 18 : 16).c_str(), w / 2, h - (small ? 24 : 36));

    // Fault / status line.
    d.setFont(&fonts::Font2);
    d.setTextDatum(textdatum_t::bottom_center);
    if (!s.fault.empty()) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.drawString(s.fault.substr(0, small ? 22 : 34).c_str(), w / 2, h - 1);
    } else {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.drawString("read-only", w / 2, h - 1);
    }
    d.endWrite();
}
