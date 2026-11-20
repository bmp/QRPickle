#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/services/space_parse.h"

// The feed is read as a stream: "sats", "events" and "generated" are found by a key scanner
// that allows JSON whitespace around ':' (a hand-made feed "sats": [ was rejected before).
static bool scan_all(const char* text, const char* key, char open) {
    services::space::KeyScan k(key, open);
    for (const char* p = text; *p; p++)
        if (k.feed(*p)) return true;
    return false;
}

inline void test_feed_key_scan_allows_whitespace() {
    TEST_ASSERT_TRUE(scan_all("{\"v\":1,\"sats\":[{", "sats", '['));
    TEST_ASSERT_TRUE(scan_all("{\"v\": 1, \"sats\": [ {", "sats", '['));
    TEST_ASSERT_TRUE(scan_all("{\n  \"sats\" :\n  [", "sats", '['));
    TEST_ASSERT_TRUE(scan_all("\"generated\": 1795159545", "generated", ':'));  // stops after ':'
    TEST_ASSERT_FALSE(scan_all("{\"satsx\":[", "sats", '['));
    TEST_ASSERT_FALSE(scan_all("{\"sats\": 5}", "sats", '['));
    TEST_ASSERT_TRUE(scan_all("{\"sats\": 5, \"x\":\"\\\"sats\", \"sats\":[", "sats", '['));  // later match
    TEST_ASSERT_TRUE(scan_all("\"\"sats\":[", "sats", '['));  // a quote right before the key
}
