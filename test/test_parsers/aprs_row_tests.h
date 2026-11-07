#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/services/aprs_row_text.h"

inline void test_aprs_row_text_columns() {
    char out[64];
    services::aprs_row_text("VU3HGG-WX", "Station", 12.34f, 47, false, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VU3HGG-W Station       12.3 km      47\xC2\xB0 NE", out);
    services::aprs_row_text("VU2ABC-9", "Fixed Base", 3.0f, 359, false, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VU2ABC-9 Fixed Base    3.0 km       359\xC2\xB0 N", out);
    services::aprs_row_text("VU2ABC-9", "Runner", 3.0f, 200, true, out, sizeof(out));  // screenshot builds
    TEST_ASSERT_EQUAL_STRING("VU2ABC-9 Runner        -- km        --", out);
    services::aprs_row_text("VU2ABC-9", "Runner", 3.0f, -5, false, out, sizeof(out));  // bad bearing -> 0
    TEST_ASSERT_EQUAL_STRING("VU2ABC-9 Runner        3.0 km       0\xC2\xB0 N", out);
}
