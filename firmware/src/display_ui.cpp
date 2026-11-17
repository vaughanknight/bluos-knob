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
    char buf[96];
    const knob::ExecResult& r = s.control.result;
    snprintf(buf, sizeof(buf), "|%d|%d|%d|%d|%d|%d|%d|%d|%lu|%d|%.1f", static_cast<int>(s.link),
             static_cast<int>(s.wifi), s.amp.reachable, s.amp.volume.mute, static_cast<int>(s.last_action),
             s.show_action, s.live, s.show_result, static_cast<unsigned long>(s.control.at_ms),
             static_cast<int>(r.outcome), r.after_db);
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
    d.setTextDatum(textdatum_t::middle_center);
    d.setTextColor(s.live ? TFT_BLACK : TFT_BLACK, s.live ? TFT_RED : TFT_YELLOW);
    const char* badge = s.live ? " LIVE " : " DRY-RUN ";
    const int bw = d.textWidth(badge);
    d.fillRoundRect(w / 2 - bw / 2 - 2, 1, bw + 4, 17, 3, s.live ? TFT_RED : TFT_YELLOW);
    d.drawString(badge, w / 2, 9);

    // Centre: big dB (or the knob action briefly, or MUTED).
    const int cy = h / 2 - (small ? 0 : 2);
    d.setTextDatum(textdatum_t::middle_center);
    const knob::ExecResult& r = s.control.result;
    if (s.show_result && r.kind == knob::CommandKind::VolumeStep && r.has_before) {
        // Big dB with a bar against the cap; red MAX when an increase was refused.
        const double shown = r.has_after ? r.after_db : r.before_db;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", shown);
        std::string db(buf);
        if (db.size() > 2 && db.compare(db.size() - 2, 2, ".0") == 0) db.resize(db.size() - 2);
        d.setFont(small ? &fonts::Font4 : &fonts::Font7);
        d.setTextColor(r.cap_blocked ? TFT_RED : TFT_WHITE, TFT_BLACK);
        d.drawString(db.c_str(), w / 2, cy - (small ? 4 : 8));
        const int bx = 10, bw = w - 20, by = cy + (small ? 12 : 26), bh = small ? 5 : 8;
        const double floor_db = -80;
        double frac = (shown - floor_db) / (s.max_db - floor_db);
        frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
        d.drawRect(bx, by, bw, bh, TFT_DARKGREY);
        d.fillRect(bx + 1, by + 1, static_cast<int>((bw - 2) * frac), bh - 2, r.cap_blocked ? TFT_RED : TFT_GREEN);
        if (r.cap_blocked) {
            d.setFont(&fonts::Font2);
            d.setTextColor(TFT_RED, TFT_BLACK);
            d.drawString((std::string("MAX ") + r.message.substr(4)).c_str(), w / 2, by + bh + 10);
        }
    } else if (s.show_result && r.kind == knob::CommandKind::MuteToggle && r.outcome != knob::Outcome::Failed) {
        d.setFont(&fonts::Font4);
        d.setTextSize(small ? 1 : 2);
        d.setTextColor(TFT_ORANGE, TFT_BLACK);
        d.drawString(r.mute_after ? "MUTED" : "UNMUTED", w / 2, cy);
    } else if (s.show_result && (r.kind == knob::CommandKind::SourceOptical || r.kind == knob::CommandKind::SourceSpotify) &&
               r.outcome != knob::Outcome::Failed) {
        d.setFont(&fonts::Font4);
        d.setTextSize(small ? 1 : 2);
        d.setTextColor(TFT_CYAN, TFT_BLACK);
        d.drawString(r.kind == knob::CommandKind::SourceOptical ? "Optical" : "Spotify", w / 2, cy);
    } else if (s.show_action && s.last_action != knob::Action::NoOp && s.last_action != knob::Action::Unknown) {
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
    } else if (s.show_result && r.outcome == knob::Outcome::Failed) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.drawString(r.message.substr(0, small ? 22 : 34).c_str(), w / 2, h - 1);
    } else if (s.show_result && r.outcome == knob::Outcome::DryRun) {
        d.setTextColor(TFT_YELLOW, TFT_BLACK);
        d.drawString("dry-run: amp not changed", w / 2, h - 1);
    }
    d.endWrite();
}
