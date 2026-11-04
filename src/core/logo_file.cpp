#include "logo_file.h"

namespace core {
    namespace logo {
        static uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

        bool header_valid(const uint8_t* hdr, size_t hdr_len, size_t file_size, uint16_t* w, uint16_t* h) {
            if (!hdr || hdr_len < HEADER_BYTES) return false;
            if (hdr[0] != 0x19 || hdr[1] != 0x14) return false;  // LVGL 9 magic, RGB565A8
            const uint16_t iw = u16(hdr + 4), ih = u16(hdr + 6), stride = u16(hdr + 8);
            if (iw < 1 || ih < 1 || iw > MAX_SIDE || ih > MAX_SIDE) return false;
            if (stride != iw * 2) return false;
            if (file_size != HEADER_BYTES + (size_t)iw * ih * 3) return false;
            if (w) *w = iw;
            if (h) *h = ih;
            return true;
        }
    }  // namespace logo
}  // namespace core
