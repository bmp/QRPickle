#pragma once
#include "../../src/services/link_watch.h"

// A connection that received nothing for longer than its limit is treated as dead (review F2):
// after a router restart or a NAT drop a socket can look connected for hours.
static void test_link_silent_after_limit() {
    using services::link_silent;
    TEST_ASSERT_FALSE(link_silent(1000, 1000, 60000));
    TEST_ASSERT_FALSE(link_silent(1000, 61000, 60000));  // exactly at the limit
    TEST_ASSERT_TRUE(link_silent(1000, 61001, 60000));
    TEST_ASSERT_TRUE(link_silent(0xFFFFFF00u, 60000, 30000));  // across the millis() wrap
    TEST_ASSERT_FALSE(link_silent(0xFFFFFF00u, 10000, 30000));
    TEST_ASSERT_FALSE(link_silent(0, 5000000, 60000));  // 0 = nothing received yet: not judged
}
