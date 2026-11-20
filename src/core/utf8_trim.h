#pragma once
// Text from downloads (park, city, event names) is UTF-8 and is cut to fit fixed buffers. A cut
// in the middle of a 2-4 byte letter leaves half a letter, which LVGL draws as a box. These keep
// whole letters only. Host-safe, header-only (test/test_parsers/utf8_tests.h).
#include <stddef.h>
#include <string.h>

// Removes an incomplete UTF-8 sequence (or stray continuation bytes) at the end of s.
inline void utf8_trim_partial(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    size_t i = n, cont = 0;
    while (i > 0 && cont < 4 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) {
        i--;
        cont++;
    }
    if (i == 0) {  // only continuation bytes
        if (cont) s[0] = '\0';
        return;
    }
    const unsigned char lead = (unsigned char)s[i - 1];
    size_t need = 0;  // continuation bytes the lead byte asks for
    if ((lead & 0xE0) == 0xC0) need = 1;
    else if ((lead & 0xF0) == 0xE0) need = 2;
    else if ((lead & 0xF8) == 0xF0) need = 3;
    if (lead < 0x80) {  // ASCII before the tail: any continuation bytes are stray
        s[i] = '\0';
    } else if (cont < need) {
        s[i - 1] = '\0';  // the letter is incomplete: drop it
    }
}

// strncpy into dst (size n, always terminated) without leaving half a letter at the end.
inline void utf8_copy(char* dst, size_t n, const char* src) {
    if (!dst || !n) return;
    strncpy(dst, src ? src : "", n - 1);
    dst[n - 1] = '\0';
    utf8_trim_partial(dst);
}
