#include <unity.h>
#include "../../src/services/sota_cluster_parse.h"
#include "version_tests.h"
#include "aprs_tests.h"
#include "solar_tests.h"

using services::sota_cluster::ParsedSpot;
using services::sota_cluster::parse_line;
using services::sota_cluster::is_summit_ref;
using services::sota_cluster::mode_for_freq;

void setUp() {}
void tearDown() {}

// Lines below are verbatim from cluster.sota.org.uk (2026-10-06) unless noted.

void test_parses_standard_spot() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de K1GC:      14054.0  K1GC         W1/DI-009                      1704Z", s));
    TEST_ASSERT_EQUAL_STRING("K1GC", s.spotter);
    TEST_ASSERT_EQUAL_STRING("K1GC", s.activator);
    TEST_ASSERT_EQUAL_STRING("W1/DI-009", s.summit);
    TEST_ASSERT_EQUAL_STRING("17:04", s.time);
    TEST_ASSERT_EQUAL_STRING("", s.comment);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 14.054f, s.freq_mhz);
    TEST_ASSERT_EQUAL_STRING("CW", s.mode);
}

void test_parses_vhf_fm_spot() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de K2CPT:    146520.0  K2CPT        W2/GA-026                      1719Z", s));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 146.52f, s.freq_mhz);
    TEST_ASSERT_EQUAL_STRING("W2/GA-026", s.summit);
    TEST_ASSERT_EQUAL_STRING("FM", s.mode);
}

// Synthetic: long spotter calls are glued to the frequency on some clusters.
void test_parses_spotter_glued_to_frequency() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de KG7LBY-#:14325.0  KG7LBY  W6/NE-124  1737Z", s));
    TEST_ASSERT_EQUAL_STRING("KG7LBY-#", s.spotter);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 14.325f, s.freq_mhz);
    TEST_ASSERT_EQUAL_STRING("SSB", s.mode);
}

// Synthetic: free-text comment between summit and time.
void test_parses_comment_between_summit_and_time() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de W7DLZ:  14259.0  W7DLZ  W6/NE-124  tnx all qrp 1735Z", s));
    TEST_ASSERT_EQUAL_STRING("W6/NE-124", s.summit);
    TEST_ASSERT_EQUAL_STRING("tnx all qrp", s.comment);
    TEST_ASSERT_EQUAL_STRING("17:35", s.time);
}

void test_mode_word_in_comment_wins() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de N1XX:  14300.0  N1XX  W1/HA-001  FT8 1200Z", s));
    TEST_ASSERT_EQUAL_STRING("FT8", s.mode);
}

void test_rbnhole_spotter_means_cw() {
    ParsedSpot s;
    TEST_ASSERT_TRUE(parse_line("DX de RBNHOLE:    7120.0  KX0R         W0C/SR-046                     1715Z", s));
    TEST_ASSERT_EQUAL_STRING("CW", s.mode);
}

void test_rejects_non_spot_lines() {
    ParsedSpot s;
    TEST_ASSERT_FALSE(parse_line("login: ", s));
    TEST_ASSERT_FALSE(parse_line("VU3GLJ de GM4LLD sota_cluster >", s));
    TEST_ASSERT_FALSE(parse_line("", s));
    TEST_ASSERT_FALSE(parse_line("DX de K1GC:  abc  K1GC  W1/DI-009  1704Z", s));   // bad frequency
    TEST_ASSERT_FALSE(parse_line("DX de K1GC:  14054.0  K1GC  hello  1704Z", s));   // no summit
}

void test_summit_ref_shapes() {
    TEST_ASSERT_TRUE(is_summit_ref("W0C/SR-046"));
    TEST_ASSERT_TRUE(is_summit_ref("3B8/MU-001"));
    TEST_ASSERT_TRUE(is_summit_ref("G/LD-001"));
    TEST_ASSERT_FALSE(is_summit_ref("K1GC"));
    TEST_ASSERT_FALSE(is_summit_ref("W1/DI-09"));
    TEST_ASSERT_FALSE(is_summit_ref("W1DI-009"));
    TEST_ASSERT_FALSE(is_summit_ref("/DI-009"));
}

void test_mode_for_freq_band_plan() {
    TEST_ASSERT_EQUAL_STRING("FT8", mode_for_freq(14.074f));
    TEST_ASSERT_EQUAL_STRING("CW",  mode_for_freq(10.113f));
    TEST_ASSERT_EQUAL_STRING("CW",  mode_for_freq(7.059f));
    TEST_ASSERT_EQUAL_STRING("SSB", mode_for_freq(7.195f));
    TEST_ASSERT_EQUAL_STRING("CW",  mode_for_freq(144.100f));
    TEST_ASSERT_EQUAL_STRING("SSB", mode_for_freq(144.200f));
    TEST_ASSERT_EQUAL_STRING("FM",  mode_for_freq(433.500f));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_standard_spot);
    RUN_TEST(test_parses_vhf_fm_spot);
    RUN_TEST(test_parses_spotter_glued_to_frequency);
    RUN_TEST(test_parses_comment_between_summit_and_time);
    RUN_TEST(test_mode_word_in_comment_wins);
    RUN_TEST(test_rbnhole_spotter_means_cw);
    RUN_TEST(test_rejects_non_spot_lines);
    RUN_TEST(test_summit_ref_shapes);
    RUN_TEST(test_mode_for_freq_band_plan);
    RUN_TEST(test_version_compare);
    RUN_TEST(test_aprs_uncompressed_position);
    RUN_TEST(test_aprs_rejects_compressed_and_garbage);
    RUN_TEST(test_aprs_addressee);
    RUN_TEST(test_solar_parses_hamqsl_sample);
    RUN_TEST(test_solar_rejects_garbage_and_missing_fields);
    RUN_TEST(test_solar_parse_updated);
    RUN_TEST(test_solar_fetch_schedule);
    RUN_TEST(test_solar_manual_refresh_limit);
    return UNITY_END();
}
