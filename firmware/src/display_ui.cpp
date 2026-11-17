#include "display_ui.h"

#include <M5Unified.h>

namespace {

UiState g_drawn{};
bool g_first = true;

uint16_t link_colour(KnobLink link) {
    switch (link) {
        case KnobLink::Connected: return TFT_GREEN;
        case KnobLink::Scanning:
        case KnobLink::Connecting: return TFT_YELLOW;
        default: return TFT_RED;
    }
}

const char* action_label(knob::Action action) {
    switch (action) {
        case knob::Action::VolumeUp: return "VOL +";
        case knob::Action::VolumeDown: return "VOL -";
        case knob::Action::MuteToggle: return "MUTE";
        case knob::Action::BrightnessUp: return "SRC: Spotify";
        case knob::Action::BrightnessDown: return "SRC: Optical";
        case knob::Action::NoOp: return "-";
        case knob::Action::Unknown: return "?";
    }
    return "?";
}

bool same(const UiState& a, const UiState& b) {
    return a.link == b.link && a.last_action == b.last_action && a.events == b.events &&
           a.overflow == b.overflow;
}

}  // namespace

void ui_begin() {
    M5.Display.setRotation(1);
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.fillScreen(TFT_BLACK);
}

void ui_render(const UiState& s) {
    if (!g_first && same(s, g_drawn)) return;
    g_first = false;
    g_drawn = s;

    auto& d = M5.Display;
    const int w = d.width();
    const int h = d.height();
    const bool small = h < 100;  // StickC 80x160 vs StickC Plus 135x240

    d.startWrite();
    d.fillScreen(TFT_BLACK);

    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.drawString("BluOS Knob", 2, 2);
    d.drawRightString(s.board, w - 2, 2);

    d.fillCircle(6, small ? 18 : 22, 4, link_colour(s.link));
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(String("knob ") + knob_link_name(s.link), 14, small ? 14 : 18);

    d.setTextSize(small ? 2 : 3);
    d.setTextDatum(textdatum_t::middle_center);
    d.drawString(action_label(s.last_action), w / 2, h / 2 + (small ? 4 : 6));
    d.setTextDatum(textdatum_t::top_left);

    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    String footer = String("events ") + s.events;
    if (s.overflow) footer += String("  dropped ") + s.overflow;
    d.drawString(footer, 2, h - 10);
    d.endWrite();
}
