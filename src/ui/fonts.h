#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
    #endif

    // Public, mutable global font structures accessible across C and C++ environments
    extern lv_font_t font_atkinson_14;
    extern lv_font_t font_atkinson_18;
    extern lv_font_t font_jetbrains_14;
    extern lv_font_t font_jetbrains_24;

    // Initializes the proxy map and stitches fallback routing tables together
    void fonts_init(void);

    #ifdef __cplusplus
}
#endif
