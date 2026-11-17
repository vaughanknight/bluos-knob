// Field extraction from BluOS /Volume and /Status XML. Portable, allocation-light, no XML library.
#pragma once

#include <string>

namespace knob {

struct VolumeInfo {
    bool ok = false;  // false when db is missing or not a finite number
    double db = 0;
    bool mute = false;
    int level = -1;
};

struct StatusInfo {
    bool ok = false;
    std::string service;
    std::string title;  // title1, entity-decoded
    std::string input_type_index;
    std::string state;
    bool has_db = false;
    double db = 0;
    bool mute = false;
};

VolumeInfo parse_volume(const std::string& xml);
StatusInfo parse_status(const std::string& xml);

}  // namespace knob
