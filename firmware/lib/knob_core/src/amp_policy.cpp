#include "amp_policy.h"

#include <cctype>
#include <cstdio>

#include "bluos_xml.h"

namespace knob {
namespace {

std::string fmt_db(double value) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%g", value);
    return buf;
}

bool read_volume(AmpTransport& amp, VolumeInfo* out, std::string* error) {
    std::string body;
    if (!amp.get("/Volume", &body)) {
        *error = "couldn't read the amp volume";
        return false;
    }
    *out = parse_volume(body);
    if (!out->ok) {
        *error = "amp volume unreadable";
        return false;
    }
    return true;
}

ExecResult failed(ExecResult r, const std::string& message) {
    r.outcome = Outcome::Failed;
    r.message = message;
    return r;
}

bool write(AmpTransport& amp, const std::string& path, ExecResult* r, std::string* body) {
    r->writes++;
    return amp.get(path, body);  // exactly once: a lost response may still have applied
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool attr(const std::string& tag, const char* name, std::string* out) {
    const std::string needle = std::string(" ") + name + "=\"";
    const size_t at = tag.find(needle);
    if (at == std::string::npos) return false;
    const size_t start = at + needle.size();
    const size_t end = tag.find('"', start);
    if (end == std::string::npos) return false;
    std::string raw = tag.substr(start, end - start);
    std::string decoded;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw.compare(i, 5, "&amp;") == 0) { decoded += '&'; i += 4; }
        else if (raw.compare(i, 4, "&lt;") == 0) { decoded += '<'; i += 3; }
        else if (raw.compare(i, 4, "&gt;") == 0) { decoded += '>'; i += 3; }
        else if (raw.compare(i, 6, "&quot;") == 0) { decoded += '"'; i += 5; }
        else if (raw.compare(i, 6, "&apos;") == 0) { decoded += '\''; i += 5; }
        else decoded += raw[i];
    }
    *out = decoded;
    return true;
}

ExecResult switch_source(const Command& command, const Config& config, AmpTransport& amp, bool live,
                         ExecResult r) {
    VolumeInfo volume;
    std::string error;
    if (!read_volume(amp, &volume, &error)) return failed(r, error);
    r.has_before = true;
    r.before_db = volume.db;

    std::string play_path;
    if (command.kind == CommandKind::SourceOptical) {
        play_path = "/Play?inputTypeIndex=" + url_encode_component(config.optical_input);
    } else {
        std::string list;
        if (!amp.get("/RadioBrowse?service=Capture", &list)) return failed(r, "couldn't read the amp's sources");
        const std::string url = find_source_url(list, config.spotify_source);
        if (url.empty()) return failed(r, config.spotify_source + " not found in the amp's sources");
        play_path = "/Play?url=" + url_encode_component(url_decode(url));
    }

    const bool lower_first = volume.db > config.source_safe_db;  // never raise on a switch
    r.has_after = true;
    r.after_db = lower_first ? config.source_safe_db : volume.db;
    if (!live) {
        r.outcome = Outcome::DryRun;
        r.message = "dry-run";
        return r;
    }
    std::string body;
    if (lower_first && !write(amp, "/Volume?abs_db=" + fmt_db(config.source_safe_db), &r, &body))
        return failed(r, "no reply lowering volume; source not switched");
    if (!write(amp, play_path, &r, &body)) return failed(r, "no reply switching source (not retried)");
    r.outcome = Outcome::Done;
    return r;
}

}  // namespace

Command plan_command(Action action, const Config& config) {
    switch (action) {
        case Action::VolumeUp: return {CommandKind::VolumeStep, config.step_db};
        case Action::VolumeDown: return {CommandKind::VolumeStep, -config.step_db};
        case Action::MuteToggle: return {CommandKind::MuteToggle, 0};
        case Action::BrightnessDown: return {CommandKind::SourceOptical, 0};
        case Action::BrightnessUp: return {CommandKind::SourceSpotify, 0};
        default: return {CommandKind::Ignore, 0};
    }
}

ExecResult execute(const Command& command, const Config& config, AmpTransport& amp, bool live) {
    ExecResult r;
    r.kind = command.kind;
    std::string error, body;
    VolumeInfo volume;

    switch (command.kind) {
        case CommandKind::Ignore:
            r.outcome = Outcome::Ignored;
            return r;

        case CommandKind::VolumeStep: {
            if (!read_volume(amp, &volume, &error)) return failed(r, error);
            r.has_before = true;
            r.before_db = volume.db;
            const double planned = volume.db + command.step_db;
            if (command.step_db > 0 && planned > config.max_db) {
                r.outcome = Outcome::Refused;
                r.cap_blocked = true;
                r.message = "max " + fmt_db(config.max_db) + " dB";
                return r;
            }
            r.has_after = true;
            r.after_db = planned;
            if (!live) {
                r.outcome = Outcome::DryRun;
                r.message = "dry-run";
                return r;
            }
            if (!write(amp, "/Volume?db=" + fmt_db(command.step_db), &r, &body))
                return failed(r, "no reply from amp (not retried)");
            const VolumeInfo confirmed = parse_volume(body);
            if (confirmed.ok) r.after_db = confirmed.db;
            r.outcome = Outcome::Done;
            return r;
        }

        case CommandKind::MuteToggle: {
            if (!read_volume(amp, &volume, &error)) return failed(r, error);
            r.has_before = true;
            r.before_db = volume.db;
            r.mute_after = !volume.mute;
            if (!live) {
                r.outcome = Outcome::DryRun;
                r.message = "dry-run";
                return r;
            }
            if (!write(amp, std::string("/Volume?mute=") + (r.mute_after ? "1" : "0"), &r, &body))
                return failed(r, "no reply from amp (not retried)");
            r.outcome = Outcome::Done;
            return r;
        }

        case CommandKind::SourceOptical:
        case CommandKind::SourceSpotify:
            return switch_source(command, config, amp, live, r);
    }
    return r;
}

bool event_is_stale(uint32_t event_ms, uint32_t now_ms, uint32_t max_age_ms) {
    return static_cast<uint32_t>(now_ms - event_ms) >= max_age_ms;
}

std::string find_source_url(const std::string& xml, const std::string& name) {
    const std::string wanted = lower(name);
    std::string found;
    int matches = 0;
    size_t pos = 0;
    while ((pos = xml.find("<item ", pos)) != std::string::npos) {
        const size_t end = xml.find('>', pos);
        if (end == std::string::npos) break;
        const std::string tag = xml.substr(pos, end - pos);
        std::string text, url;
        if (attr(tag, "text", &text) && attr(tag, "URL", &url) && !url.empty() && lower(text) == wanted) {
            found = url;
            matches++;
        }
        pos = end;
    }
    return matches == 1 ? found : "";
}

std::string url_decode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
            out += static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += in[i];
        }
    }
    return out;
}

std::string url_encode_component(const std::string& in) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (const char ch : in) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
        case Outcome::Ignored: return "ignored";
        case Outcome::DryRun: return "dry_run";
        case Outcome::Done: return "done";
        case Outcome::Refused: return "refused";
        case Outcome::Failed: return "failed";
    }
    return "unknown";
}

const char* command_name(CommandKind kind) {
    switch (kind) {
        case CommandKind::Ignore: return "ignore";
        case CommandKind::VolumeStep: return "volume_step";
        case CommandKind::MuteToggle: return "mute_toggle";
        case CommandKind::SourceOptical: return "source_optical";
        case CommandKind::SourceSpotify: return "source_spotify";
    }
    return "unknown";
}

}  // namespace knob
