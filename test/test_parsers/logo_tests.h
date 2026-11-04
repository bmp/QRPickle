#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/core/logo_file.h"

static void logo_hdr(uint8_t* b, uint16_t w, uint16_t h, uint16_t stride, uint8_t magic = 0x19, uint8_t cf = 0x14) {
    memset(b, 0, 12);
    b[0] = magic;
    b[1] = cf;
    b[4] = w & 0xFF;
    b[5] = w >> 8;
    b[6] = h & 0xFF;
    b[7] = h >> 8;
    b[8] = stride & 0xFF;
    b[9] = stride >> 8;
}

inline void test_logo_header_valid_sizes() {
    uint8_t b[12];
    uint16_t w = 0, h = 0;
    logo_hdr(b, 60, 60, 120);
    TEST_ASSERT_TRUE(core::logo::header_valid(b, 12, 12 + 60 * 60 * 3, &w, &h));
    TEST_ASSERT_EQUAL_UINT16(60, w);
    TEST_ASSERT_EQUAL_UINT16(60, h);
    logo_hdr(b, 60, 45, 120);
    TEST_ASSERT_TRUE(core::logo::header_valid(b, 12, 12 + 60 * 45 * 3));
    logo_hdr(b, 1, 1, 2);
    TEST_ASSERT_TRUE(core::logo::header_valid(b, 12, 15));
}

inline void test_logo_header_rejects_bad_files() {
    uint8_t b[12];
    logo_hdr(b, 60, 60, 120, 0x18);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 10812));  // magic
    logo_hdr(b, 60, 60, 120, 0x19, 0x12);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 10812));  // colour format
    logo_hdr(b, 60, 60, 60);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 10812));  // stride
    logo_hdr(b, 61, 60, 122);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 12 + 61 * 60 * 3));  // too wide
    logo_hdr(b, 0, 60, 0);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 12));  // empty
    logo_hdr(b, 60, 60, 120);
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 10811));  // truncated by one byte
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 12, 10813));  // one byte too many
    TEST_ASSERT_FALSE(core::logo::header_valid(b, 11, 10812));  // short header
    TEST_ASSERT_FALSE(core::logo::header_valid(nullptr, 12, 10812));
}
