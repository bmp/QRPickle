#pragma once
#include <unity.h>
#include "../../src/core/mem_math.h"

inline void test_mem_frag_pct() {
    TEST_ASSERT_EQUAL_UINT8(52, mem::frag_pct(74000, 34804));  // the v0.2.7 baseline
    TEST_ASSERT_EQUAL_UINT8(0, mem::frag_pct(0, 0));  // nothing free: not "100 % fragmented"
    TEST_ASSERT_EQUAL_UINT8(0, mem::frag_pct(1000, 1000));  // one block
    TEST_ASSERT_EQUAL_UINT8(100, mem::frag_pct(1000, 0));
    TEST_ASSERT_EQUAL_UINT8(0, mem::frag_pct(1000, 2000));  // largest > free (sampled apart): clamp
}
