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
