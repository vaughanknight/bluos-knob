#include <unity.h>

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "amp_policy.h"

using knob::Action;
using knob::CommandKind;
using knob::Outcome;

// Captured from the NAD M33 (2026-11-17), trimmed to two entries; full copy in tests/fixtures/bluos/radiobrowse_capture.xml.
static const char* kRadioBrowse =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<radiotime service=\"Capture\">\n"
    "  <item text=\"Optical 2\" id=\"xdynamic-Source2\" type=\"audio\" inputType=\"spdif\" "
    "URL=\"Capture%3Ahw%3Aimxnadadc%2C0%2F1%2F25%2F2%3Fid%3Dxdynamic-Source2\"></item>\n"
    "  <item text=\"Spotify\" id=\"Spotify\" type=\"audio\" URL=\"Spotify%3Aplay\" serviceType=\"CloudService\" "
    "image=\"/Sources/images/SpotifyIcon.png\"></item>\n</radiotime>";

// Behaviour fake of a BluOS player: holds dB/mute/source and answers like the real HTTP API.
class FakeAmp : public knob::AmpTransport {
   public:
    double db = -40;
    bool mute = false;
    std::string source = "optical-1";
    bool volume_readable = true;
    bool mute_db_reported = true;
    double race_db = 0;          // added to db just before any write applies (app/remote changed it meanwhile)
    bool play_sets_db = false;   // the new input brings its own level
    double play_db = 0;
    std::set<std::string> unreachable;   // paths that fail with no effect
    std::set<std::string> lose_response; // paths that apply but whose response is lost
    std::vector<std::string> requests;

    bool get(const std::string& path, std::string* body) override {
        requests.push_back(path);
        if (unreachable.count(path)) return false;
        if (path != "/Volume" && path != "/RadioBrowse?service=Capture") db += race_db;
        if (path.rfind("/Volume?db=", 0) == 0) db += std::atof(path.c_str() + 11);
        else if (path.rfind("/Volume?abs_db=", 0) == 0) db = std::atof(path.c_str() + 15);
        else if (path.rfind("/Volume?mute=", 0) == 0) mute = path.back() == '1';
        else if (path.rfind("/Play?inputTypeIndex=", 0) == 0) source = path.substr(21);
        else if (path.rfind("/Play?url=", 0) == 0) source = knob::url_decode(path.substr(10));
        if (play_sets_db && path.rfind("/Play?", 0) == 0) db = play_db;
        if (lose_response.count(path)) return false;
        if (path == "/RadioBrowse?service=Capture") {
            *body = kRadioBrowse;
            return true;
        }
        if (path.rfind("/Volume", 0) == 0) {
            char buf[96];
            if (volume_readable && mute && mute_db_reported)  // real BluOS: muted reports db=-100 + muteDb
                snprintf(buf, sizeof(buf), "<volume db=\"-100\" mute=\"1\" muteDb=\"%g\">0</volume>", db);
            else if (volume_readable && mute)
                snprintf(buf, sizeof(buf), "<volume db=\"-100\" mute=\"1\">0</volume>");
            else if (volume_readable)
                snprintf(buf, sizeof(buf), "<volume db=\"%g\" mute=\"%d\">50</volume>", db, mute ? 1 : 0);
            else
                snprintf(buf, sizeof(buf), "<volume mute=\"0\">50</volume>");
            *body = buf;
            return true;
        }
        *body = "<status></status>";
        return true;
    }
    int writes() const {
        int n = 0;
        for (const auto& r : requests) n += (r != "/Volume" && r != "/RadioBrowse?service=Capture");
        return n;
    }
};

static knob::Config config() {
    knob::Config c;
    c.wifi_ssid = "x";
    c.host = "amp";
    c.max_db = -24;
    c.source_safe_db = -40;
    c.step_db = 1;
    return c;
}

static knob::ExecResult run(Action action, FakeAmp& amp, bool live = true) {
    const knob::Config c = config();
    return knob::execute(knob::plan_command(action, c), c, amp, live);
}

void setUp() {}
void tearDown() {}

void given_actions_when_planned_then_match_daemon_command_table() {
    /*
    Test Doc:
    - Why: The ESP32 must map gestures to the same commands as scripts/bluos_daemon.py planned_command.
    - Contract: up/down -> +/-step_db; mute -> toggle; brightness_down -> optical; brightness_up -> Spotify; others ignored.
    - Usage Notes: step_db comes from config (validated 0 < step <= 3).
    - Quality Contribution: Catches swapped directions or source buttons.
    - Worked Example: VolumeDown with step 1 -> VolumeStep(-1).
    */
    const knob::Config c = config();
    TEST_ASSERT_EQUAL(CommandKind::VolumeStep, knob::plan_command(Action::VolumeUp, c).kind);
    TEST_ASSERT_EQUAL_DOUBLE(1, knob::plan_command(Action::VolumeUp, c).step_db);
    TEST_ASSERT_EQUAL_DOUBLE(-1, knob::plan_command(Action::VolumeDown, c).step_db);
    TEST_ASSERT_EQUAL(CommandKind::MuteToggle, knob::plan_command(Action::MuteToggle, c).kind);
    TEST_ASSERT_EQUAL(CommandKind::SourceOptical, knob::plan_command(Action::BrightnessDown, c).kind);
    TEST_ASSERT_EQUAL(CommandKind::SourceSpotify, knob::plan_command(Action::BrightnessUp, c).kind);
    TEST_ASSERT_EQUAL(CommandKind::Ignore, knob::plan_command(Action::Unknown, c).kind);
    TEST_ASSERT_EQUAL(CommandKind::Ignore, knob::plan_command(Action::NoOp, c).kind);
}

void given_volume_below_cap_when_stepping_up_then_reads_fresh_then_writes_one_step() {
    /*
    Test Doc:
    - Why: Normal use: a knob turn nudges the amp by exactly one step.
    - Contract: execute reads /Volume first; an increase is sent as /Volume?abs_db=<checked target>.
    - Usage Notes: The fresh read is the safety basis; cached poll data is never used.
    - Quality Contribution: Catches skipped reads or double writes.
    - Worked Example: amp -40, step +1 -> requests [/Volume, /Volume?abs_db=-39], after -39.
    */
    FakeAmp amp;
    const knob::ExecResult r = run(Action::VolumeUp, amp);
    TEST_ASSERT_EQUAL(Outcome::Done, r.outcome);
    TEST_ASSERT_EQUAL_STRING("/Volume", amp.requests.at(0).c_str());
    TEST_ASSERT_EQUAL_STRING("/Volume?abs_db=-39", amp.requests.at(1).c_str());
    TEST_ASSERT_EQUAL_INT(1, amp.writes());
    TEST_ASSERT_EQUAL_DOUBLE(-39, amp.db);
    TEST_ASSERT_EQUAL_DOUBLE(-40, r.before_db);
}

void given_volume_at_or_near_cap_when_stepping_up_then_refused_without_write() {
    /*
    Test Doc:
    - Why: Safety-first: the knob must never push the amp past max_db (default -24 dB).
    - Contract: if current + step > max_db the increase is refused, cap_blocked is set, and nothing is written.
    - Usage Notes: Mirrors assert_safe_step in scripts/bluos_volume_step.py.
    - Quality Contribution: Catches the core loudness guard regressing.
    - Worked Example: amp -24 +1 -> refused; amp -24.5 (changed in the app since the last poll) +1 -> refused.
    */
    FakeAmp amp;
    amp.db = -24;
    knob::ExecResult r = run(Action::VolumeUp, amp);
    TEST_ASSERT_EQUAL(Outcome::Refused, r.outcome);
    TEST_ASSERT_TRUE(r.cap_blocked);
    TEST_ASSERT_EQUAL_INT(0, amp.writes());
    amp.db = -24.5;
    r = run(Action::VolumeUp, amp);
    TEST_ASSERT_EQUAL(Outcome::Refused, r.outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-24.5, amp.db);
}

void given_volume_above_cap_when_stepping_down_then_allowed() {
    /*
    Test Doc:
    - Why: If the amp is already loud (set elsewhere), the knob must still be able to turn it down.
    - Contract: decreases are never refused by the cap.
    - Usage Notes: Matches Python, where the check applies only when step_db > 0.
    - Quality Contribution: Catches a cap that locks the user out of turning it down.
    - Worked Example: amp -20, step -1 -> done, -21.
    */
    FakeAmp amp;
    amp.db = -20;
    const knob::ExecResult r = run(Action::VolumeDown, amp);
    TEST_ASSERT_EQUAL(Outcome::Done, r.outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-21, amp.db);
}

void given_unreadable_volume_when_stepping_or_muting_then_fail_closed_without_write() {
    /*
    Test Doc:
    - Why: Plan review finding 1: unknown current volume must never default to a number.
    - Contract: if /Volume fails or lacks a valid db, the step or mute is Failed and nothing is written.
    - Usage Notes: The message is shown on screen.
    - Quality Contribution: Catches a 0 dB default turning a step into a jump.
    - Worked Example: /Volume unreachable -> Failed, writes 0; db attribute missing -> Failed, writes 0.
    */
    FakeAmp amp;
    amp.unreachable.insert("/Volume");
    TEST_ASSERT_EQUAL(Outcome::Failed, run(Action::VolumeUp, amp).outcome);
    TEST_ASSERT_EQUAL(Outcome::Failed, run(Action::MuteToggle, amp).outcome);
    TEST_ASSERT_EQUAL_INT(0, amp.writes());
    FakeAmp garbled;
    garbled.volume_readable = false;
    TEST_ASSERT_EQUAL(Outcome::Failed, run(Action::VolumeDown, garbled).outcome);
    TEST_ASSERT_EQUAL_INT(0, garbled.writes());
}

void given_dry_run_when_executing_then_reads_and_checks_but_never_writes() {
    /*
    Test Doc:
    - Why: AC-07: the device boots in dry-run and must not change the amp until live is enabled.
    - Contract: with live=false every command returns DryRun (or Refused/Failed from the checks) and performs zero writes.
    - Usage Notes: Dry-run still shows the planned dB and the cap refusal on screen.
    - Quality Contribution: Catches any write path that ignores the live flag.
    - Worked Example: up at -40 -> DryRun, planned -39, amp still -40; up at -24 -> Refused (cap).
    */
    FakeAmp amp;
    for (Action a : {Action::VolumeUp, Action::VolumeDown, Action::MuteToggle, Action::BrightnessDown,
                     Action::BrightnessUp}) {
        const knob::ExecResult r = run(a, amp, false);
        TEST_ASSERT_EQUAL(Outcome::DryRun, r.outcome);
    }
    TEST_ASSERT_EQUAL_INT(0, amp.writes());
    TEST_ASSERT_EQUAL_DOUBLE(-40, amp.db);
    const knob::ExecResult planned = run(Action::VolumeUp, amp, false);
    TEST_ASSERT_TRUE(planned.has_after);
    TEST_ASSERT_EQUAL_DOUBLE(-39, planned.after_db);
    amp.db = -24;
    TEST_ASSERT_EQUAL(Outcome::Refused, run(Action::VolumeUp, amp, false).outcome);
}

void given_lost_write_response_when_stepping_then_failed_and_never_retried() {
    /*
    Test Doc:
    - Why: Plan review finding 2: retrying a write whose response was lost can double-step the volume.
    - Contract: a write that returns no response is reported Failed and attempted exactly once.
    - Usage Notes: The next poll shows the real state; the user can turn again.
    - Quality Contribution: Catches retry loops on the write path.
    - Worked Example: /Volume?abs_db=-39 applies but its response is lost -> Failed, 1 write, amp -39.
    */
    FakeAmp amp;
    amp.lose_response.insert("/Volume?abs_db=-39");
    const knob::ExecResult r = run(Action::VolumeUp, amp);
    TEST_ASSERT_EQUAL(Outcome::Failed, r.outcome);
    TEST_ASSERT_EQUAL_INT(1, amp.writes());
    TEST_ASSERT_EQUAL_DOUBLE(-39, amp.db);
}

void given_mute_state_when_toggling_then_reads_fresh_and_flips_it() {
    /*
    Test Doc:
    - Why: The toggle must invert the amp's real state, which may have been changed in the app.
    - Contract: reads /Volume, then sends mute=1 when unmuted and mute=0 when muted.
    - Usage Notes: Mirrors bluos_daemon.py mute_toggle.
    - Quality Contribution: Catches a toggle based on stale local state.
    - Worked Example: mute 0 -> /Volume?mute=1; mute 1 -> /Volume?mute=0.
    */
    FakeAmp amp;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::MuteToggle, amp).outcome);
    TEST_ASSERT_EQUAL_STRING("/Volume?mute=1", amp.requests.back().c_str());
    TEST_ASSERT_TRUE(amp.mute);
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::MuteToggle, amp).outcome);
    TEST_ASSERT_FALSE(amp.mute);
}

void given_source_switch_when_louder_than_safe_level_then_lowers_first_and_never_raises() {
    /*
    Test Doc:
    - Why: Switching inputs must not blast; tightening of bluos_source.py, which set abs_db even when that raised the volume.
    - Contract: if current > source_safe_db, set abs_db first, then switch; if already at/below, only switch.
    - Usage Notes: source_safe_db is validated <= -24 dB.
    - Quality Contribution: Catches the switch happening before the volume drop, and a switch that raises volume.
    - Worked Example: -30 -> [/Volume, /Volume?abs_db=-40, /Play?inputTypeIndex=optical-2]; -50 -> no abs_db, stays -50.
    */
    FakeAmp amp;
    amp.db = -30;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::BrightnessDown, amp).outcome);
    TEST_ASSERT_EQUAL_STRING("/Volume?abs_db=-40", amp.requests.at(1).c_str());
    TEST_ASSERT_EQUAL_STRING("/Play?inputTypeIndex=optical-2", amp.requests.at(2).c_str());
    TEST_ASSERT_EQUAL_DOUBLE(-40, amp.db);
    FakeAmp quiet;
    quiet.db = -50;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::BrightnessDown, quiet).outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-50, quiet.db);
    TEST_ASSERT_EQUAL_INT(1, quiet.writes());
}

void given_spotify_button_when_switching_then_plays_listed_url_or_fails_without_write() {
    /*
    Test Doc:
    - Why: Spotify is selected via the source list's URL, like bluos_source.py find_source + play_url_parameter.
    - Contract: url = encode(decode(item URL)); missing source -> Failed with no write.
    - Usage Notes: Matching is exact text, case-insensitive.
    - Quality Contribution: Catches double-encoding and silent wrong-source switches.
    - Worked Example: URL="Spotify%3Aplay" -> /Play?url=Spotify%3Aplay.
    */
    FakeAmp amp;
    amp.db = -45;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::BrightnessUp, amp).outcome);
    bool played = false;
    for (const auto& req : amp.requests) played = played || req == "/Play?url=Spotify%3Aplay";
    TEST_ASSERT_TRUE(played);
    TEST_ASSERT_EQUAL_STRING("", knob::find_source_url(kRadioBrowse, "Tidal").c_str());
    TEST_ASSERT_EQUAL_STRING("Spotify%3Aplay", knob::find_source_url(kRadioBrowse, "spotify").c_str());
    knob::Config c = config();
    c.spotify_source = "Tidal";
    FakeAmp none;
    const knob::ExecResult r = knob::execute(knob::plan_command(Action::BrightnessUp, c), c, none, true);
    TEST_ASSERT_EQUAL(Outcome::Failed, r.outcome);
    TEST_ASSERT_EQUAL_INT(0, none.writes());
}

void given_event_ages_when_checked_then_old_events_are_stale() {
    /*
    Test Doc:
    - Why: Plan review finding 2: gestures queued during an outage must not fire seconds later.
    - Contract: an event is stale when now - event >= max_age (wrap-safe).
    - Usage Notes: The app drops stale events before planning a command.
    - Quality Contribution: Catches delayed volume changes after reconnect.
    - Worked Example: max 1000: event 0, now 999 -> fresh; now 1000 -> stale.
    */
    TEST_ASSERT_FALSE(knob::event_is_stale(0, 999, 1000));
    TEST_ASSERT_TRUE(knob::event_is_stale(0, 1000, 1000));
    TEST_ASSERT_FALSE(knob::event_is_stale(0xFFFFFF00u, 0x00000010u, 1000));
}

void given_muted_amp_when_stepping_or_switching_then_cap_uses_the_real_unmuted_level() {
    /*
    Test Doc:
    - Why: Found live on 2026-11-17: a muted BluOS player reports db="-100" (muteDb holds the real level), so
      checking the cap against -100 would let a step go past max_db once the relative step applies.
    - Contract: while muted, the safety basis is muteDb; muted without muteDb is Failed with no write.
    - Usage Notes: BluOS publishes muteDb/muteVolume only while muted (Custom Integration API).
    - Quality Contribution: Catches the muted-at-cap bypass in both the volume step and the source switch.
    - Worked Example: muted, real -24 -> step up refused; muted, real -30 -> optical lowers to -40 first.
    */
    FakeAmp amp;
    amp.db = -24;
    amp.mute = true;
    knob::ExecResult r = run(Action::VolumeUp, amp);
    TEST_ASSERT_EQUAL(Outcome::Refused, r.outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-24, r.before_db);
    TEST_ASSERT_EQUAL_INT(0, amp.writes());
    FakeAmp loud;
    loud.db = -30;
    loud.mute = true;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::BrightnessDown, loud).outcome);
    TEST_ASSERT_EQUAL_STRING("/Volume?abs_db=-40", loud.requests.at(1).c_str());
    FakeAmp unknown;
    unknown.mute = true;
    unknown.mute_db_reported = false;
    TEST_ASSERT_EQUAL(Outcome::Failed, run(Action::VolumeUp, unknown).outcome);
    TEST_ASSERT_EQUAL(Outcome::Failed, run(Action::BrightnessUp, unknown).outcome);
    TEST_ASSERT_EQUAL_INT(0, unknown.writes());
    // Unmuting never raises above the pre-mute level, so it must always work.
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::MuteToggle, unknown).outcome);
    TEST_ASSERT_FALSE(unknown.mute);
}

void given_volume_raised_elsewhere_between_read_and_write_when_stepping_up_then_never_exceeds_cap() {
    /*
    Test Doc:
    - Why: Phase 3 review finding 1: a relative db=+1 applies to the level at arrival; if the app/remote raised
      the volume after our read, the result could pass max_db.
    - Contract: increases are written as abs_db=<target checked against max_db>; decreases stay relative.
    - Usage Notes: the worst case is undoing a concurrent change, never exceeding the cap.
    - Quality Contribution: Catches a return to relative increases.
    - Worked Example: read -25, app raises +3 meanwhile, step +1 -> amp -24 (not -21).
    */
    FakeAmp amp;
    amp.db = -25;
    amp.race_db = 3;
    TEST_ASSERT_EQUAL(Outcome::Done, run(Action::VolumeUp, amp).outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-24, amp.db);
    FakeAmp down;
    down.db = -30;
    run(Action::VolumeDown, down);
    TEST_ASSERT_EQUAL_STRING("/Volume?db=-1", down.requests.back().c_str());
}

void given_input_with_its_own_level_when_switching_source_then_rechecked_and_lowered() {
    /*
    Test Doc:
    - Why: Phase 3 review finding 2: an input can come up at its own (louder) level after /Play.
    - Contract: after switching, /Volume is re-read; above source_safe_db it is set to source_safe_db.
    - Usage Notes: mirrors the Python order (safe level applied after the switch) while keeping lower-first.
    - Quality Contribution: Catches a switch that leaves the amp loud.
    - Worked Example: amp -50, new input comes up at -20 -> after switch abs_db=-40 -> amp -40.
    */
    FakeAmp amp;
    amp.db = -50;
    amp.play_sets_db = true;
    amp.play_db = -20;
    const knob::ExecResult r = run(Action::BrightnessDown, amp);
    TEST_ASSERT_EQUAL(Outcome::Done, r.outcome);
    TEST_ASSERT_EQUAL_DOUBLE(-40, amp.db);
    TEST_ASSERT_EQUAL_STRING("/Volume?abs_db=-40", amp.requests.back().c_str());
    TEST_ASSERT_EQUAL_DOUBLE(-40, r.after_db);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(given_actions_when_planned_then_match_daemon_command_table);
    RUN_TEST(given_volume_below_cap_when_stepping_up_then_reads_fresh_then_writes_one_step);
    RUN_TEST(given_volume_at_or_near_cap_when_stepping_up_then_refused_without_write);
    RUN_TEST(given_volume_above_cap_when_stepping_down_then_allowed);
    RUN_TEST(given_unreadable_volume_when_stepping_or_muting_then_fail_closed_without_write);
    RUN_TEST(given_dry_run_when_executing_then_reads_and_checks_but_never_writes);
    RUN_TEST(given_lost_write_response_when_stepping_then_failed_and_never_retried);
    RUN_TEST(given_mute_state_when_toggling_then_reads_fresh_and_flips_it);
    RUN_TEST(given_source_switch_when_louder_than_safe_level_then_lowers_first_and_never_raises);
    RUN_TEST(given_spotify_button_when_switching_then_plays_listed_url_or_fails_without_write);
    RUN_TEST(given_event_ages_when_checked_then_old_events_are_stale);
    RUN_TEST(given_muted_amp_when_stepping_or_switching_then_cap_uses_the_real_unmuted_level);
    RUN_TEST(given_volume_raised_elsewhere_between_read_and_write_when_stepping_up_then_never_exceeds_cap);
    RUN_TEST(given_input_with_its_own_level_when_switching_source_then_rechecked_and_lowered);
    return UNITY_END();
}
