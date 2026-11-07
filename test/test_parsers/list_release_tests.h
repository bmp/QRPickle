#pragma once
#include "../../src/services/list_release.h"

// A spot list is freed only when it exists, no task writes it, no screen shows it and screen
// cycling won't come straight back to it.
static void test_list_release_only_when_unused() {
    using services::may_release_list;
    TEST_ASSERT_TRUE(may_release_list(true, false, false, false));
    TEST_ASSERT_FALSE(may_release_list(false, false, false, false));  // nothing to free
    TEST_ASSERT_FALSE(may_release_list(true, true, false, false));  // a fetch or socket task writes it
    TEST_ASSERT_FALSE(may_release_list(true, false, true, false));  // its screen is open
    TEST_ASSERT_FALSE(may_release_list(true, false, false, true));  // cycling comes back to it
}
