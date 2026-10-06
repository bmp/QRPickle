#pragma once
#include <unity.h>
#include "../../src/services/version.h"

inline void test_version_compare() {
    using services::compare_versions;
    TEST_ASSERT_TRUE(compare_versions("v0.1.12", "v0.1.11") > 0);
    TEST_ASSERT_TRUE(compare_versions("v0.1.9", "v0.1.11") < 0);   // numeric, not lexical
    TEST_ASSERT_TRUE(compare_versions("v1.0.0", "v0.9.99") > 0);
    TEST_ASSERT_EQUAL_INT(0, compare_versions("v0.1.11", "0.1.11"));
    TEST_ASSERT_TRUE(compare_versions("v0.1.10", "v0.1.11") < 0);  // older release is not an update
}
