#pragma once
#include <unity.h>
#include <string.h>
#include "../../src/core/utf8_trim.h"
#include "../../src/services/hamalert_parse.h"

// A fixed buffer can cut a 2-4 byte UTF-8 letter in half; the half shows as a box on screen.
inline void test_utf8_trim_partial_end() {
    char a[] =
        "\xC3\x89"
        "cole";  // "École" whole: unchanged
    utf8_trim_partial(a);
    TEST_ASSERT_EQUAL_STRING(
        "\xC3\x89"
        "cole",
        a);

    char b[] = "Caf\xC3";  // "Café" cut after the lead byte
    utf8_trim_partial(b);
    TEST_ASSERT_EQUAL_STRING("Caf", b);

    char c[] = "x\xE2\x82";  // 3-byte "€" cut after 2 bytes
    utf8_trim_partial(c);
    TEST_ASSERT_EQUAL_STRING("x", c);

    char d[] = "x\xE2\x82\xAC";  // whole "€"
    utf8_trim_partial(d);
    TEST_ASSERT_EQUAL_STRING("x\xE2\x82\xAC", d);

    char e[] = "ab\x89";  // a stray continuation byte
    utf8_trim_partial(e);
    TEST_ASSERT_EQUAL_STRING("ab", e);

    char f[] = "";
    utf8_trim_partial(f);
    TEST_ASSERT_EQUAL_STRING("", f);
    utf8_trim_partial(nullptr);  // no crash
}

inline void test_utf8_copy_cuts_whole_letters() {
    char out[5];
    utf8_copy(out, sizeof(out), "Caf\xC3\xA9s");  // 4 bytes fit: "Caf" + half of "é" -> "Caf"
    TEST_ASSERT_EQUAL_STRING("Caf", out);
    utf8_copy(out, sizeof(out), "ab");
    TEST_ASSERT_EQUAL_STRING("ab", out);
    utf8_copy(out, sizeof(out), nullptr);
    TEST_ASSERT_EQUAL_STRING("", out);
}

// A HamAlert comment longer than its 80-byte field, cut inside "é": no half letter at the end.
inline void test_hamalert_comment_cut_keeps_whole_letters() {
    using namespace services::hamalert;
    char line[160] = "DX de HamAlert: 14074.0 VU2ABC ";
    char comment[100];
    memset(comment, 'a', 78);
    strcpy(comment + 78, "\xC3\xA9t\xC3\xA9");  // byte 79 is the lead byte of "é"
    strcat(line, comment);
    SpotFields f;
    TEST_ASSERT_EQUAL_INT(4, split_spot(line, f));
    TEST_ASSERT_EQUAL_size_t(78, strlen(f.rest));
}
