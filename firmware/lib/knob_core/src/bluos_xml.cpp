#include "bluos_xml.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace knob {
namespace {

std::string decode_entities(const std::string& in) {
    static const struct {
        const char* entity;
        char ch;
    } kEntities[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        bool replaced = false;
        if (in[i] == '&') {
            for (const auto& e : kEntities) {
                const size_t n = std::char_traits<char>::length(e.entity);
                if (in.compare(i, n, e.entity) == 0) {
                    out += e.ch;
                    i += n - 1;
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced) out += in[i];
    }
    return out;
}

// Start of the root element's open tag, after any <?xml ...?> prolog.
size_t root_start(const std::string& xml, const char* name) {
    size_t pos = 0;
    while ((pos = xml.find('<', pos)) != std::string::npos) {
        if (xml.compare(pos, 2, "<?") == 0) {
            pos = xml.find("?>", pos);
            if (pos == std::string::npos) return std::string::npos;
            continue;
        }
        const std::string open = std::string("<") + name;
        if (xml.compare(pos, open.size(), open) == 0) {
            const char next = pos + open.size() < xml.size() ? xml[pos + open.size()] : '\0';
            if (next == ' ' || next == '>' || next == '/') return pos;
        }
        return std::string::npos;
    }
    return std::string::npos;
}

bool attribute(const std::string& xml, size_t tag_start, const char* name, std::string* out) {
    const size_t tag_end = xml.find('>', tag_start);
    if (tag_end == std::string::npos) return false;
    const std::string needle = std::string(" ") + name + "=\"";
    const size_t at = xml.find(needle, tag_start);
    if (at == std::string::npos || at > tag_end) return false;
    const size_t value_start = at + needle.size();
    const size_t value_end = xml.find('"', value_start);
    if (value_end == std::string::npos || value_end > tag_end) return false;
    *out = decode_entities(xml.substr(value_start, value_end - value_start));
    return true;
}

// Text of the first <name>...</name> child (exact tag name, so <title1> never matches <twoline_title1>).
bool element(const std::string& xml, const char* name, std::string* out) {
    const std::string open = std::string("<") + name + ">";
    const std::string close = std::string("</") + name + ">";
    const size_t start = xml.find(open);
    if (start == std::string::npos) return false;
    const size_t text_start = start + open.size();
    const size_t end = xml.find(close, text_start);
    if (end == std::string::npos) return false;
    *out = decode_entities(xml.substr(text_start, end - text_start));
    return true;
}

bool parse_finite(const std::string& text, double* out) {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end != text.c_str() + text.size() || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

}  // namespace

VolumeInfo parse_volume(const std::string& xml) {
    VolumeInfo info;
    const size_t root = root_start(xml, "volume");
    if (root == std::string::npos) return info;
    std::string db, mute;
    if (!attribute(xml, root, "db", &db) || !parse_finite(db, &info.db)) return info;
    info.mute = attribute(xml, root, "mute", &mute) && mute == "1";
    std::string mute_db;
    info.has_mute_db = attribute(xml, root, "muteDb", &mute_db) && parse_finite(mute_db, &info.mute_db);
    const size_t text_start = xml.find('>', root);
    const size_t text_end = xml.find("</volume>", text_start);
    double level = 0;
    if (text_start != std::string::npos && text_end != std::string::npos &&
        parse_finite(xml.substr(text_start + 1, text_end - text_start - 1), &level)) {
        info.level = static_cast<int>(level);
    }
    info.ok = true;
    return info;
}

StatusInfo parse_status(const std::string& xml) {
    StatusInfo info;
    if (root_start(xml, "status") == std::string::npos) return info;
    element(xml, "service", &info.service);
    element(xml, "title1", &info.title);
    element(xml, "inputTypeIndex", &info.input_type_index);
    element(xml, "state", &info.state);
    std::string text;
    info.has_db = element(xml, "db", &text) && parse_finite(text, &info.db);
    info.mute = element(xml, "mute", &text) && text == "1";
    info.ok = true;
    return info;
}

}  // namespace knob
