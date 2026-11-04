#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/services/dx_status_text.h"

inline void test_dx_status_line_follows_the_connection() {
    using namespace services;
    TEST_ASSERT_EQUAL_STRING("CONNECTED: waiting for spots...", dx_status_line(DX_STATUS_CONNECTED, 0, 0));
    TEST_ASSERT_EQUAL_STRING("No spots match the filters.", dx_status_line(DX_STATUS_CONNECTED, 5, 0));
    TEST_ASSERT_EQUAL_STRING("Tap a row to read the full spot.", dx_status_line(DX_STATUS_CONNECTED, 5, 3));
    TEST_ASSERT_EQUAL_STRING("OFFLINE: cluster not connected; retrying.", dx_status_line(DX_STATUS_DISCONNECTED, 0, 0));
    TEST_ASSERT_EQUAL_STRING("OFFLINE: cluster not connected; retrying.", dx_status_line(DX_STATUS_DISCONNECTED, 4, 4));
    TEST_ASSERT_EQUAL_STRING("CONNECTING to the DX cluster...", dx_status_line(DX_STATUS_DIALING, 0, 0));
    TEST_ASSERT_EQUAL_STRING("CONNECTING to the DX cluster...", dx_status_line(DX_STATUS_CONNECTING, 2, 1));
    TEST_ASSERT_EQUAL_STRING("CONNECTING to the DX cluster...", dx_status_line(DX_STATUS_AUTHORIZING, 0, 0));
}

inline void test_dx_row_text_has_fixed_columns() {
    services::DxSpot s = {};
    strcpy(s.dx_call, "VU2ABC");
    s.freq = 14074.0f;
    strcpy(s.mode, "FT8");
    strcpy(s.spotter, "VU3GLJ");
    strcpy(s.comment, "CQ DX");
    char out[80];
    services::dx_row_text(s, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VU2ABC    14074.000 FT8       VU3GLJ   CQ DX", out);
    strcpy(s.dx_call, "VU2ABCDEFGH");  // longer than its column: cut, columns stay aligned
    strcpy(s.spotter, "LONGSPOTTER");
    services::dx_row_text(s, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("VU2ABCDEF 14074.000 FT8       LONGSPOT CQ DX", out);
    char small[8];
    services::dx_row_text(s, small, sizeof(small));  // never overflows
    TEST_ASSERT_EQUAL_UINT32(7, strlen(small));
}
