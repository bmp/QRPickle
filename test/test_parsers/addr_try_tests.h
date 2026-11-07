#pragma once
#include "../../src/services/addr_try.h"

// When a host has several addresses and one doesn't answer (2026-11-07: 185.199.109.133, one of
// raw.githubusercontent.com's four, was unreachable from the owner's network), try the next.
static void test_connect_any_tries_addresses_in_order() {
    int tried[4];
    int n_tried = 0;
    auto record = [&](int ok_from) {
        return [&, ok_from](int i) {
            tried[n_tried++] = i;
            return i >= ok_from;
        };
    };
    n_tried = 0;
    TEST_ASSERT_EQUAL_INT(0, services::connect_any(4, record(0)));  // first works: one try
    TEST_ASSERT_EQUAL_INT(1, n_tried);
    n_tried = 0;
    TEST_ASSERT_EQUAL_INT(2, services::connect_any(4, record(2)));  // two dead, third works
    TEST_ASSERT_EQUAL_INT(3, n_tried);
    TEST_ASSERT_EQUAL_INT(0, tried[0]);
    TEST_ASSERT_EQUAL_INT(2, tried[2]);
    n_tried = 0;
    TEST_ASSERT_EQUAL_INT(-1, services::connect_any(4, record(9)));  // none works: all tried
    TEST_ASSERT_EQUAL_INT(4, n_tried);
    n_tried = 0;
    TEST_ASSERT_EQUAL_INT(-1, services::connect_any(0, record(0)));  // no address (DNS failed)
    TEST_ASSERT_EQUAL_INT(0, n_tried);
}
