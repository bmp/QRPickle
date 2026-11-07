#pragma once
#include "../../src/ui/xota_fetch.h"

// One rule for manual and cycle visits of xOTA; only the age limit differs (v0.2.8 bug: the manual
// path waited for HamAlert/APRS to stop and never loaded POTA).
static void test_xota_fetch_manual_and_cycle_share_one_rule() {
    using ui::xota_should_fetch;
    const uint32_t now = 10000000;
    // Never fetched: both kinds of visit download.
    TEST_ASSERT_TRUE(xota_should_fetch(false, true, 0, now));
    TEST_ASSERT_TRUE(xota_should_fetch(true, true, 0, now));
    // POTA: a manual visit refreshes after 30 s, a cycle visit after 5 min.
    TEST_ASSERT_FALSE(xota_should_fetch(false, true, now - 20000, now));
    TEST_ASSERT_TRUE(xota_should_fetch(false, true, now - 31000, now));
    TEST_ASSERT_FALSE(xota_should_fetch(true, true, now - 4 * 60000, now));
    TEST_ASSERT_TRUE(xota_should_fetch(true, true, now - 6 * 60000, now));
    // SOTA: a manual visit always connects (spots stream in), a cycle visit after 10 min.
    TEST_ASSERT_TRUE(xota_should_fetch(false, false, now - 1000, now));
    TEST_ASSERT_FALSE(xota_should_fetch(true, false, now - 9 * 60000, now));
    TEST_ASSERT_TRUE(xota_should_fetch(true, false, now - 11 * 60000, now));
    // Across the millis() wrap.
    TEST_ASSERT_TRUE(xota_should_fetch(true, true, 0xFFFFFFFFu - 6 * 60000, 1000));
}
