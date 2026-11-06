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

inline void test_mem_web_can_serve() {
    TEST_ASSERT_TRUE(mem::web_can_serve(74000, 34804));  // normal
    TEST_ASSERT_TRUE(mem::web_can_serve(12288, 6144));  // exactly at the limits
    TEST_ASSERT_FALSE(mem::web_can_serve(12287, 6144));  // too little free in total
    TEST_ASSERT_FALSE(mem::web_can_serve(60000, 6143));  // no block for a response
    TEST_ASSERT_FALSE(mem::web_can_serve(536, 276));  // the 2026-11-06 crash level
}
