// Knob action -> amplifier command, and the guarded executor. Portable; HTTP sits behind AmpTransport.
//
// Safety contract (mirrors scripts/bluos_daemon.py + bluos_volume_step.py, tightened):
//  - every volume step and mute toggle starts with a fresh /Volume read; unreadable -> refuse (fail closed)
//  - an increase that would exceed max_db is refused; decreases are always allowed
//  - a source switch never raises the volume: it lowers to source_safe_db first when above it, then switches
//  - a write whose response is lost is reported as failed and never retried (it may have applied)
//  - dry-run performs the reads and the safety checks but no writes
#pragma once

#include <cstdint>
#include <string>

#include "config.h"
#include "decoder.h"

namespace knob {

class AmpTransport {
   public:
    virtual ~AmpTransport() = default;
    // HTTP GET of `path` (e.g. "/Volume?db=1"); false when no complete 200 response arrived.
    virtual bool get(const std::string& path, std::string* body) = 0;
};

enum class CommandKind { Ignore, VolumeStep, MuteToggle, SourceOptical, SourceSpotify };

struct Command {
    CommandKind kind = CommandKind::Ignore;
    double step_db = 0;
};

enum class Outcome { Ignored, DryRun, Done, Refused, Failed };

struct ExecResult {
    Outcome outcome = Outcome::Ignored;
    CommandKind kind = CommandKind::Ignore;
    std::string message;   // plain words for screen/serial
    bool has_before = false;
    double before_db = 0;
    bool has_after = false;
    double after_db = 0;   // planned (dry-run) or confirmed
    bool cap_blocked = false;
    bool mute_after = false;
    int writes = 0;        // write requests attempted
};

Command plan_command(Action action, const Config& config);
ExecResult execute(const Command& command, const Config& config, AmpTransport& amp, bool live);

bool event_is_stale(uint32_t event_ms, uint32_t now_ms, uint32_t max_age_ms);

// Exact (case-insensitive) match on <item text=...> in /RadioBrowse?service=Capture; "" when absent or ambiguous.
std::string find_source_url(const std::string& radiobrowse_xml, const std::string& name);
std::string url_decode(const std::string& in);
std::string url_encode_component(const std::string& in);

const char* outcome_name(Outcome outcome);
const char* command_name(CommandKind kind);

}  // namespace knob
