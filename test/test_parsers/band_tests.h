#pragma once
#include <unity.h>
#include "../../src/services/band_rating.h"

using services::bands::Rating;

// Ratings as one letter per band (160m .. 6m): P/F/G, E = Es possible, - = closed, ? = unknown.
inline void band_row(const services::bands::Inputs& in, bool night, char* out) {
    static const char L[] = {'?', 'P', 'F', 'G', 'E', '-'};
    for (int i = 0; i < services::bands::BAND_COUNT; i++) out[i] = L[(int)services::bands::rate_band(i, in, night)];
    out[services::bands::BAND_COUNT] = '\0';
}

// Snapshot of the documented model for the 2026-10-10 hamqsl data (SFI 121, K 3, A 11). If the
// model is retuned on purpose, update these rows and the manual appendix together.
inline void test_band_ratings_snapshot() {
    using namespace services::bands;
    char row[BAND_COUNT + 1];
    const Inputs today = {121, 3, 11, 12.9f, 10};
    band_row(today, false, row);
    TEST_ASSERT_EQUAL_STRING("PPFFGGGGFP-", row);  // 160 80 60 40 30 20 17 15 12 10 6
    band_row(today, true, row);
    TEST_ASSERT_EQUAL_STRING("FFFGGFPPPP-", row);

    const Inputs solar_min = {68, 1, 5, 12.9f, 10};
    band_row(solar_min, false, row);
    TEST_ASSERT_EQUAL_STRING("PPFGGGFPPP-", row);
    band_row(solar_min, true, row);
    TEST_ASSERT_EQUAL_STRING("GGGGFPPPPP-", row);
}

inline void test_band_ratings_unknown_until_data() {
    using namespace services::bands;
    TEST_ASSERT_TRUE(rate_band(5, {0, 3, 11, 12.9f, 10}, false) == Rating::UNKNOWN);  // no SFI
    TEST_ASSERT_TRUE(rate_band(5, {121, -1, 11, 12.9f, 10}, true) == Rating::UNKNOWN);  // no K
    TEST_ASSERT_TRUE(rate_band(99, {121, 3, 11, 12.9f, 10}, true) == Rating::UNKNOWN);
    TEST_ASSERT_TRUE(rate_group(0, {0, -1, -1, 0, 1}, false) == Rating::UNKNOWN);
}

// More solar flux never makes a band worse above the reach of daytime absorption (14 MHz by day,
// 7 MHz at night; by day more flux also means more absorption on the low bands). More geomagnetic
// activity never makes any band better.
inline void test_band_ratings_monotonic() {
    using namespace services::bands;
    for (int night = 0; night <= 1; night++) {
        for (int b = 0; b < BAND_COUNT; b++) {
            for (int16_t sfi = 66; sfi < 300; sfi += 2) {
                const Inputs lo = {sfi, 2, 8, 30.0f, 10}, hi = {(int16_t)(sfi + 2), 2, 8, 30.0f, 10};
                const float from = night ? 7.0f : 14.0f;
                if (BANDS[b].mhz >= from && BANDS[b].mhz < 50.0f)
                    TEST_ASSERT_TRUE(rate_band(b, hi, night) >= rate_band(b, lo, night));
            }
            for (int16_t k = 0; k < 9; k++) {
                const Inputs calm = {150, k, 10, 30.0f, 10}, worse = {150, (int16_t)(k + 1), 10, 30.0f, 10};
                if (BANDS[b].mhz < 50.0f) TEST_ASSERT_TRUE(rate_band(b, worse, night) <= rate_band(b, calm, night));
            }
        }
    }
}

inline void test_band_daytime_absorption_and_noise() {
    using namespace services::bands;
    // 160 m is never usable by day; at night it follows the K index.
    TEST_ASSERT_TRUE(rate_band(0, {68, 0, 2, 50.0f, 1}, false) == Rating::POOR);
    TEST_ASSERT_TRUE(rate_band(0, {150, 1, 5, 50.0f, 1}, true) == Rating::GOOD);
    TEST_ASSERT_TRUE(rate_band(0, {150, 3, 15, 50.0f, 1}, true) == Rating::FAIR);
    TEST_ASSERT_TRUE(rate_band(0, {150, 5, 40, 50.0f, 1}, true) == Rating::POOR);
    // High flux raises daytime absorption: 40 m drops from good to fair.
    TEST_ASSERT_TRUE(rate_band(3, {70, 1, 5, 50.0f, 1}, false) == Rating::GOOD);
    TEST_ASSERT_TRUE(rate_band(3, {220, 1, 5, 50.0f, 1}, false) == Rating::FAIR);
}

inline void test_band_6m_es_and_f2() {
    using namespace services::bands;
    const int SIX = BAND_COUNT - 1;
    TEST_ASSERT_TRUE(rate_band(SIX, {121, 2, 8, 12.9f, 6}, false) == Rating::ES_POSSIBLE);  // northern summer
    TEST_ASSERT_TRUE(rate_band(SIX, {121, 2, 8, 12.9f, 6}, true) == Rating::CLOSED);  // daytime only
    TEST_ASSERT_TRUE(rate_band(SIX, {121, 2, 8, 12.9f, 10}, false) == Rating::CLOSED);  // out of season
    TEST_ASSERT_TRUE(rate_band(SIX, {121, 2, 8, -33.9f, 12}, false) == Rating::ES_POSSIBLE);  // southern summer
    TEST_ASSERT_TRUE(rate_band(SIX, {121, 2, 8, -33.9f, 6}, false) == Rating::CLOSED);
    TEST_ASSERT_TRUE(rate_band(SIX, {300, 1, 5, 12.9f, 10}, false) == Rating::FAIR);  // F2 at an extreme maximum
}

inline void test_band_model_parts() {
    using namespace services::bands;
    // effective_ssn inverts SFI = 63.7 + 0.728 R + 0.00089 R^2.
    for (float r = 0; r <= 250; r += 25) {
        const float sfi = 63.7f + 0.728f * r + 0.00089f * r * r;
        TEST_ASSERT_FLOAT_WITHIN(1.0f, r, effective_ssn((int16_t)lroundf(sfi)));
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, effective_ssn(50));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, storm_factor(3, 19));
    TEST_ASSERT_EQUAL_FLOAT(0.95f, storm_factor(4, 10));
    TEST_ASSERT_EQUAL_FLOAT(0.85f, storm_factor(2, 35));  // A alone
    TEST_ASSERT_EQUAL_FLOAT(0.65f, storm_factor(8, 35));  // the stronger of K and A
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 28.1f, muf_mhz({121, 3, 11, 0, 1}, false));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, luf_mhz(0));  // floor
}

inline void test_band_groups_show_best_band() {
    using namespace services::bands;
    const Inputs today = {121, 3, 11, 12.9f, 10};
    TEST_ASSERT_TRUE(rate_group(0, today, false) == Rating::FAIR);  // 160 P, 80 P, 60 F
    TEST_ASSERT_TRUE(rate_group(3, today, false) == Rating::GOOD);  // 15 G, 12 F, 10 P
    TEST_ASSERT_TRUE(rate_group(3, today, true) == Rating::POOR);
    TEST_ASSERT_TRUE(rate_group(4, {121, 2, 8, 12.9f, 6}, false) == Rating::ES_POSSIBLE);
    TEST_ASSERT_TRUE(rate_group(GROUP_COUNT, today, false) == Rating::UNKNOWN);
}
