#pragma once
#include "../../src/services/lock_watch.h"

static void test_lock_watch_warns_then_restarts() {
    using services::LockWatch;
    TEST_ASSERT_TRUE(services::lock_watch(0) == LockWatch::OK);
    TEST_ASSERT_TRUE(services::lock_watch(60) == LockWatch::OK);
    TEST_ASSERT_TRUE(services::lock_watch(61) == LockWatch::WARN);
    TEST_ASSERT_TRUE(services::lock_watch(180) == LockWatch::WARN);
    TEST_ASSERT_TRUE(services::lock_watch(181) == LockWatch::RESTART);
}

static void test_lock_held_s_never_wraps() {
    TEST_ASSERT_EQUAL_UINT(0, services::lock_held_s(1001, 1000));  // start stored as millis() | 1
    TEST_ASSERT_EQUAL_UINT(0, services::lock_held_s(5, 5));
    TEST_ASSERT_EQUAL_UINT(2, services::lock_held_s(1001, 3500));
    TEST_ASSERT_EQUAL_UINT(181, services::lock_held_s(0xFFFFFF01u, 181000u - 0xFFu));  // across the millis() wrap
}
