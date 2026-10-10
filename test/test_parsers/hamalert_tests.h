#pragma once
#include <unity.h>
#include "../../src/services/hamalert_parse.h"
#include "../../src/services/version.h"

// Same results as the sscanf("DX de %15[^:]: %15s %15s %79[^\r\n]") it replaces.
inline void test_hamalert_split_spot() {
    using namespace services::hamalert;
    SpotFields f;
    TEST_ASSERT_EQUAL_INT(4, split_spot("DX de HamAlert:  14074.0  VU2ABC       FT8 -12 dB 1712Z\r\n", f));
    TEST_ASSERT_EQUAL_STRING("HamAlert", f.spotter);
    TEST_ASSERT_EQUAL_STRING("14074.0", f.freq);
    TEST_ASSERT_EQUAL_STRING("VU2ABC", f.call);
    TEST_ASSERT_EQUAL_STRING("FT8 -12 dB 1712Z", f.rest);

    TEST_ASSERT_EQUAL_INT(3, split_spot("DX de K1ABC: 7025.0 VU3GLJ", f));  // no comment/time
    TEST_ASSERT_EQUAL_STRING("VU3GLJ", f.call);
    TEST_ASSERT_EQUAL_STRING("", f.rest);

    TEST_ASSERT_EQUAL_INT(0, split_spot("VU3GLJ de HamAlert >", f));  // greeting, not a spot
    TEST_ASSERT_EQUAL_INT(0, split_spot("", f));
    TEST_ASSERT_EQUAL_INT(0, split_spot(nullptr, f));
    TEST_ASSERT_EQUAL_INT(1, split_spot("DX de NOCOLON 14074.0 X", f));  // no ':' after the spotter
    TEST_ASSERT_EQUAL_INT(1, split_spot("DX de A123456789012345678: 1 B", f));  // spotter > 15 chars
    TEST_ASSERT_EQUAL_INT(2, split_spot("DX de K1ABC:   14074.0   ", f));  // no call
}

inline void test_version_parse_edge_cases() {
    using services::compare_versions;
    TEST_ASSERT_EQUAL_INT(0, compare_versions("v0.2.4", "V0.2.4"));
    TEST_ASSERT_TRUE(compare_versions("v1", "v0.9.9") > 0);  // missing parts are 0
    TEST_ASSERT_EQUAL_INT(0, compare_versions("garbage", "0.0.0"));
    TEST_ASSERT_EQUAL_INT(0, compare_versions(nullptr, "v0.0.0"));
    TEST_ASSERT_TRUE(compare_versions("v0.2.10", "v0.2.9") > 0);
}
