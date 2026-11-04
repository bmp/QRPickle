#pragma once
#include <stddef.h>
#include <stdint.h>

// Custom splash logos (web console upload, LittleFS /logos/<side>.bin): LVGL 9 RGB565A8 images
// scaled into a 60 x 60 box. Host-safe (test/test_parsers).
namespace core {
    namespace logo {
        constexpr int MAX_SIDE = 60;
        constexpr size_t HEADER_BYTES = 12;
        constexpr size_t MAX_BYTES = HEADER_BYTES + MAX_SIDE * MAX_SIDE * 3;  // 10,812

        // True when hdr (the file's first hdr_len bytes) is an RGB565A8 header of at most 60 x 60 and
        // file_size matches it exactly; w/h (optional) get the size.
        bool header_valid(const uint8_t* hdr, size_t hdr_len, size_t file_size, uint16_t* w = nullptr,
                          uint16_t* h = nullptr);
    }  // namespace logo
}  // namespace core
