#include <unity.h>

#include <string>

#include "bluos_xml.h"

// Captured from the NAD M33 (192.168.1.67) on 2026-11-17; copies in tests/fixtures/bluos/.
static const std::string kVolumeXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<volume db=\"-39\" offsetDb=\"0\" mute=\"0\" etag=\"d7429bdf863af05d666c68ec4474a85c\" source=\"\">70</volume>";
static const std::string kStatusXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<status etag=\"0726818f8cc9f7334224ef8d04e55983\"><actions><action name=\"back\"></action></actions>"
    "<canSeek>0</canSeek><currentImage>/images/capture/ic_opticalinputNP.png</currentImage><cursor>0</cursor>"
    "<db>-39</db><image>/images/capture/ic_opticalinputNP.png</image><indexing>0</indexing>"
    "<inputId>xdynamic-Source1</inputId><inputTypeIndex>optical-1</inputTypeIndex><mid>0</mid><mode>1</mode>"
    "<mute>0</mute><pid>31</pid><prid>0</prid><repeat>2</repeat><secs>98670</secs><service>Capture</service>"
    "<serviceType>AudioInputs</serviceType><settingsGroupId>capture-xdynamic-Source1</settingsGroupId>"
    "<shuffle>0</shuffle><sid>6</sid><sleep></sleep><song>0</song><state>stream</state>"
    "<stationImage>/images/capture/ic_opticalinputNP.png</stationImage>"
    "<streamUrl>Capture:hw:imxnadadc,0/1/25/2?id=xdynamic-Source1</streamUrl><syncStat>1155</syncStat>"
    "<title1>Optical 1</title1><twoline_title1>Optical 1</twoline_title1><volume>70</volume></status>";

void setUp() {}
void tearDown() {}

void given_real_volume_response_when_parsed_then_db_mute_level() {
    /*
    Test Doc:
    - Why: The screen and (later) the safety policy depend on the amp's current dB.
    - Contract: parse_volume reads the db and mute attributes and the level text of <volume>.
    - Usage Notes: Feed the raw HTTP body.
    - Quality Contribution: Catches attribute parsing that confuses db with offsetDb.
    - Worked Example: db="-39" offsetDb="0" mute="0" >70 -> ok, -39, false, 70.
    */
    knob::VolumeInfo v = knob::parse_volume(kVolumeXml);
    TEST_ASSERT_TRUE(v.ok);
    TEST_ASSERT_EQUAL_DOUBLE(-39, v.db);
    TEST_ASSERT_FALSE(v.mute);
    TEST_ASSERT_EQUAL_INT(70, v.level);
    knob::VolumeInfo m = knob::parse_volume("<volume db=\"-45.5\" mute=\"1\">60</volume>");
    TEST_ASSERT_TRUE(m.ok);
    TEST_ASSERT_EQUAL_DOUBLE(-45.5, m.db);
    TEST_ASSERT_TRUE(m.mute);
}

void given_missing_or_garbled_db_when_parsed_then_not_ok() {
    /*
    Test Doc:
    - Why: Fail closed: an unknown volume must never be treated as a number (plan review finding 1).
    - Contract: parse_volume.ok is false when db is absent, empty, non-numeric, or not finite; or the body is not <volume>.
    - Usage Notes: Callers must check ok before using db.
    - Quality Contribution: Catches a 0 dB default sneaking through.
    - Worked Example: <volume mute="0">70</volume> -> ok=false; db="loud" -> ok=false; db="nan" -> ok=false.
    */
    TEST_ASSERT_FALSE(knob::parse_volume("<volume mute=\"0\">70</volume>").ok);
    TEST_ASSERT_FALSE(knob::parse_volume("<volume db=\"\" mute=\"0\">70</volume>").ok);
    TEST_ASSERT_FALSE(knob::parse_volume("<volume db=\"loud\">70</volume>").ok);
    TEST_ASSERT_FALSE(knob::parse_volume("<volume db=\"nan\">70</volume>").ok);
    TEST_ASSERT_FALSE(knob::parse_volume("<html>error</html>").ok);
    TEST_ASSERT_FALSE(knob::parse_volume("").ok);
}

void given_real_status_response_when_parsed_then_source_fields() {
    /*
    Test Doc:
    - Why: The idle screen shows the current source and volume.
    - Contract: parse_status extracts service, title1, inputTypeIndex, state, db and mute from <status>.
    - Usage Notes: <db> may be absent for some services; check has_db.
    - Quality Contribution: Catches matching <title1> inside <twoline_title1>.
    - Worked Example: real capture -> Capture / "Optical 1" / optical-1 / stream / -39 dB.
    */
    knob::StatusInfo s = knob::parse_status(kStatusXml);
    TEST_ASSERT_TRUE(s.ok);
    TEST_ASSERT_EQUAL_STRING("Capture", s.service.c_str());
    TEST_ASSERT_EQUAL_STRING("Optical 1", s.title.c_str());
    TEST_ASSERT_EQUAL_STRING("optical-1", s.input_type_index.c_str());
    TEST_ASSERT_EQUAL_STRING("stream", s.state.c_str());
    TEST_ASSERT_TRUE(s.has_db);
    TEST_ASSERT_EQUAL_DOUBLE(-39, s.db);
    TEST_ASSERT_FALSE(s.mute);
}

void given_status_with_entities_and_no_db_when_parsed_then_decoded_and_flagged() {
    /*
    Test Doc:
    - Why: Track titles contain &amp; etc., and some sources omit <db>.
    - Contract: XML entities in text are decoded; has_db is false without <db>.
    - Usage Notes: Titles are shown on screen as-is after decoding.
    - Quality Contribution: Catches "&amp;" on screen and a phantom 0 dB.
    - Worked Example: <title1>Simon &amp; Garfunkel</title1> -> "Simon & Garfunkel".
    */
    knob::StatusInfo s = knob::parse_status(
        "<status><service>Spotify</service><title1>Simon &amp; Garfunkel &lt;Live&gt;</title1>"
        "<state>play</state><mute>1</mute></status>");
    TEST_ASSERT_TRUE(s.ok);
    TEST_ASSERT_EQUAL_STRING("Simon & Garfunkel <Live>", s.title.c_str());
    TEST_ASSERT_FALSE(s.has_db);
    TEST_ASSERT_TRUE(s.mute);
    TEST_ASSERT_FALSE(knob::parse_status("<volume db=\"-3\"/>").ok);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(given_real_volume_response_when_parsed_then_db_mute_level);
    RUN_TEST(given_missing_or_garbled_db_when_parsed_then_not_ok);
    RUN_TEST(given_real_status_response_when_parsed_then_source_fields);
    RUN_TEST(given_status_with_entities_and_no_db_when_parsed_then_decoded_and_flagged);
    return UNITY_END();
}
