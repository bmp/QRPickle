#pragma once
#include <math.h>
#include <unity.h>
#include "../../src/services/solar_parse.h"

// Verbatim from https://www.hamqsl.com/solarxml.php (2026-10-10), VHF block shortened.
static const char* SOLAR_XML = R"(<?xml version="1.0" encoding="UTF-8" ?>
<solar>
	<solardata>
		<source url="http://www.hamqsl.com/solar.html">N0NBH</source>
		<updated> 10 Oct 2026 0727 GMT</updated>
		<solarflux>121</solarflux>
		<aindex> 11</aindex>
		<kindex> 3</kindex>
		<kindexnt>No Report</kindexnt>
		<xray>B9.2</xray>
		<sunspots>93</sunspots>
		<aurora> 3</aurora>
		<solarwind>423.8</solarwind>
		<magneticfield> -1.4</magneticfield>
		<calculatedconditions>
			<band name="80m-40m" time="day">Poor</band>
		</calculatedconditions>
		<geomagfield>UNSETTLD</geomagfield>
		<signalnoise>S2-S3</signalnoise>
		<fof2></fof2>
		<muf>NoRpt</muf>
	</solardata>
</solar>)";

inline void test_solar_parses_hamqsl_sample() {
    using namespace services::solar;
    SolarData d;
    TEST_ASSERT_TRUE(parse_xml(SOLAR_XML, d));
    TEST_ASSERT_EQUAL_INT(121, d.sfi);
    TEST_ASSERT_EQUAL_INT(93, d.sunspots);
    TEST_ASSERT_EQUAL_INT(11, d.a_index);
    TEST_ASSERT_EQUAL_INT(3, d.k_index);
    TEST_ASSERT_EQUAL_INT(3, d.aurora);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 423.8f, d.solar_wind);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.4f, d.bz);
    TEST_ASSERT_EQUAL_STRING("B9.2", d.xray);
    TEST_ASSERT_EQUAL_STRING("UNSETTLD", d.geomag);
    TEST_ASSERT_EQUAL_STRING("S2-S3", d.noise);
    TEST_ASSERT_EQUAL_STRING("", d.muf);  // "NoRpt"
    TEST_ASSERT_EQUAL_UINT32(1791617220u, d.updated_utc);  // 2026-10-10 07:27 UTC
    TEST_ASSERT_FALSE(is_storm(d));
}

inline void test_solar_rejects_garbage_and_missing_fields() {
    using namespace services::solar;
    SolarData d;
    TEST_ASSERT_FALSE(parse_xml(nullptr, d));
    TEST_ASSERT_FALSE(parse_xml("<html>Service unavailable</html>", d));
    TEST_ASSERT_FALSE(parse_xml("<solardata><solarflux>121</solarflux><kindex>No Report</kindex></solardata>", d));
    TEST_ASSERT_FALSE(parse_xml("<solardata><solarflux>121</solarflux><kindex>12</kindex></solardata>", d));
    TEST_ASSERT_TRUE(parse_xml("<solardata><solarflux>70</solarflux><kindex>6</kindex><xray>A1.0</xray></solardata>", d));
    TEST_ASSERT_EQUAL_INT(NONE, d.a_index);
    TEST_ASSERT_TRUE(isnan(d.bz));
    TEST_ASSERT_TRUE(is_storm(d));  // K 6
    TEST_ASSERT_TRUE(parse_xml("<solardata><solarflux>200</solarflux><kindex>1</kindex><xray>X1.2</xray></solardata>", d));
    TEST_ASSERT_TRUE(is_storm(d));  // X flare
}

inline void test_solar_parse_updated() {
    using services::solar::parse_updated;
    TEST_ASSERT_EQUAL_UINT32(1791617220u, parse_updated(" 10 Oct 2026 0727 GMT"));
    TEST_ASSERT_EQUAL_UINT32(1704067200u, parse_updated("1 Jan 2024 0000 GMT"));
    TEST_ASSERT_EQUAL_UINT32(1709164800u + 23 * 3600 + 59 * 60, parse_updated("29 Feb 2024 2359 GMT"));
    TEST_ASSERT_EQUAL_UINT32(0, parse_updated("10 Okt 2026 0727 GMT"));
    TEST_ASSERT_EQUAL_UINT32(0, parse_updated("10 Oct 2026 2460 GMT"));
    TEST_ASSERT_EQUAL_UINT32(0, parse_updated("eb 10 2026"));
    TEST_ASSERT_EQUAL_UINT32(0, parse_updated(""));
}

inline void test_solar_fetch_schedule() {
    using namespace services::solar;
    const uint32_t day = 1791590400u;  // 2026-10-10 00:00 UTC
    Schedule s = {0, 0, false};
    TEST_ASSERT_TRUE(fetch_due(s, day + 7 * 3600));  // never fetched: fetch now (boot)

    // Fetched at 07:27: next at the 09:15 slot, not before.
    s = {day + 7 * 3600 + 27 * 60, day + 7 * 3600 + 27 * 60, false};
    TEST_ASSERT_FALSE(fetch_due(s, day + 8 * 3600 + 30 * 60));
    TEST_ASSERT_FALSE(fetch_due(s, day + 9 * 3600 + 14 * 60));
    TEST_ASSERT_TRUE(fetch_due(s, day + 9 * 3600 + 15 * 60));

    // Fetched at 08:50: the 09:15 slot is under an hour later, so wait until 09:50.
    s = {day + 8 * 3600 + 50 * 60, day + 8 * 3600 + 50 * 60, false};
    TEST_ASSERT_FALSE(fetch_due(s, day + 9 * 3600 + 15 * 60));
    TEST_ASSERT_TRUE(fetch_due(s, day + 9 * 3600 + 50 * 60));

    // Storm: hourly.
    s = {day + 10 * 3600, day + 10 * 3600, true};
    TEST_ASSERT_FALSE(fetch_due(s, day + 10 * 3600 + 59 * 60));
    TEST_ASSERT_TRUE(fetch_due(s, day + 11 * 3600));

    // Failure after a success: retry after 15 min, while the slot is still unserved.
    s = {day + 6 * 3600 + 15 * 60, day + 9 * 3600 + 15 * 60, false};
    TEST_ASSERT_FALSE(fetch_due(s, day + 9 * 3600 + 29 * 60));
    TEST_ASSERT_TRUE(fetch_due(s, day + 9 * 3600 + 30 * 60));

    // The clock set back a little after an attempt (NTP correction) must not trigger a retry...
    s = {0, day + 8 * 3600, false};
    TEST_ASSERT_FALSE(fetch_due(s, day + 8 * 3600 - 2));
    // ...but an attempt "in the future" by days (clock wrong at the time) doesn't block forever.
    s = {0, day + 5 * 86400, false};
    TEST_ASSERT_TRUE(fetch_due(s, day));

    // Midnight slot (00:15) after a fetch at 21:15 the day before.
    s = {day - 3 * 3600 + 15 * 60, day - 3 * 3600 + 15 * 60, false};
    TEST_ASSERT_FALSE(fetch_due(s, day + 14 * 60));
    TEST_ASSERT_TRUE(fetch_due(s, day + 15 * 60));
}

inline void test_solar_manual_refresh_limit() {
    using namespace services::solar;
    const uint32_t t = 1791617220u;
    TEST_ASSERT_TRUE(manual_allowed({0, 0, false}, t));
    TEST_ASSERT_FALSE(manual_allowed({t, t, false}, t + 59 * 60));
    TEST_ASSERT_TRUE(manual_allowed({t, t, false}, t + 3600));
    TEST_ASSERT_FALSE(manual_allowed({0, t, false}, t + 30));  // failed a moment ago
    TEST_ASSERT_TRUE(manual_allowed({0, t, false}, t + 60));
    TEST_ASSERT_EQUAL_UINT32(0, manual_unlock_utc({0, 0, false}, t));
    TEST_ASSERT_EQUAL_UINT32(t + 3600, manual_unlock_utc({t, t, false}, t + 10));
    TEST_ASSERT_EQUAL_UINT32(t + 60, manual_unlock_utc({0, t, false}, t + 10));
}
