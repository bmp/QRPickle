#pragma once
#include "../../src/services/wifi_retry.h"

// After a missed boot connect (6 of 10 boots in 2026-11 tests), retry the saved network every 15 s
// for 5 minutes, then every minute.
static void test_wifi_retry_fast_then_slow() {
    TEST_ASSERT_EQUAL_UINT32(15000, services::sta_retry_interval_ms(0));
    TEST_ASSERT_EQUAL_UINT32(15000, services::sta_retry_interval_ms(299999));
    TEST_ASSERT_EQUAL_UINT32(60000, services::sta_retry_interval_ms(300000));
    TEST_ASSERT_EQUAL_UINT32(60000, services::sta_retry_interval_ms(3600000));
}

// Core 3 retries a failed first connect once, then stops for AUTH_FAIL (reason 202: some routers
// refuse a board that reset seconds ago). While the board wants its saved network, a new attempt
// starts once the station has reported a disconnect since the last begin() and gone quiet for 3 s.
static void test_wifi_sta_retry_when_station_gave_up() {
    using services::sta_retry_due;
    TEST_ASSERT_FALSE(sta_retry_due(true, true, 10000, 1000));  // connected
    TEST_ASSERT_FALSE(sta_retry_due(false, false, 10000, 1000));  // no disconnect since begin(): still trying
    TEST_ASSERT_FALSE(sta_retry_due(false, true, 3999, 1000));  // the library may still be retrying
    TEST_ASSERT_TRUE(sta_retry_due(false, true, 4000, 1000));
    TEST_ASSERT_FALSE(sta_retry_due(false, true, 1500, 0xFFFFFC18u));  // millis() wrapped: 2.5 s ago
    TEST_ASSERT_TRUE(sta_retry_due(false, true, 2000, 0xFFFFFC18u));  // 3 s ago across the wrap
}
