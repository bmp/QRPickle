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
