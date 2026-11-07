#pragma once
#include <string.h>
#include "../../src/services/freshness.h"

// Per-source freshness (review F3/F4): a source that never succeeds or falls behind is reported.
static void test_freshness_states() {
    using namespace services;
    const uint32_t I = 60000;  // normal interval
    Freshness f{};
    TEST_ASSERT_TRUE(fresh_state(f, 1000, I) == FreshState::NONE);  // never tried
    f.fail(1000);
    TEST_ASSERT_TRUE(fresh_state(f, 2000, I) == FreshState::NONE);  // one failure, nothing yet
    f.fail(3000);
    f.fail(4000);
    TEST_ASSERT_TRUE(fresh_state(f, 5000, I) == FreshState::FAILING);  // 3 in a row, never OK
    f.ok(10000);
    TEST_ASSERT_EQUAL_UINT16(0, f.fails);
    TEST_ASSERT_TRUE(fresh_state(f, 10000 + 2 * I, I) == FreshState::FRESH);
    TEST_ASSERT_TRUE(fresh_state(f, 10000 + 2 * I + 1, I) == FreshState::STALE);  // behind
    f.fail(200000);
    f.fail(210000);
    f.fail(220000);
    TEST_ASSERT_TRUE(fresh_state(f, 230000, I) == FreshState::FAILING);  // stale and failing
    Freshness w{};
    w.ok(0xFFFFFF00u);  // across the millis() wrap
    TEST_ASSERT_TRUE(fresh_state(w, 1000, I) == FreshState::FRESH);
}

static void test_freshness_age_text() {
    char b[24];
    services::age_text(b, sizeof(b), 0);
    TEST_ASSERT_EQUAL_STRING("just now", b);
    services::age_text(b, sizeof(b), 59);
    TEST_ASSERT_EQUAL_STRING("just now", b);
    services::age_text(b, sizeof(b), 60);
    TEST_ASSERT_EQUAL_STRING("1 min ago", b);
    services::age_text(b, sizeof(b), 59 * 60 + 59);
    TEST_ASSERT_EQUAL_STRING("59 min ago", b);
    services::age_text(b, sizeof(b), 2 * 3600 + 5);
    TEST_ASSERT_EQUAL_STRING("2 h ago", b);
}
