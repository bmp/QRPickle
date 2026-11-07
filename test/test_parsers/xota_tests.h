#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/services/xota_row_text.h"

inline void test_xota_row_text_columns() {
    char out[64];
    services::xota_row_text("06:02", "US-11782", 14.074f, "FT8", "KC5JEC", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("06:02 US-11782    14.074 FT8  KC5JEC", out);
    services::xota_row_text("10:40", "I/LO-297", 10.0f, "CW", "IR0GY/P", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("10:40 I/LO-297    10.000 CW   IR0GY/P", out);
    services::xota_row_text("10:41", "W7W/LC-001X", 145.5f, "SSB", "VERYLONGCALL1/P", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("10:41 W7W/LC-001 145.500 SSB  VERYLONGCALL", out);  // cut, columns kept
    char small[10];
    services::xota_row_text("06:02", "US-11782", 14.074f, "FT8", "KC5JEC", small, sizeof(small));
    TEST_ASSERT_EQUAL_UINT32(9, strlen(small));  // never overflows
}
