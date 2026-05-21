#ifndef LV_CONF_H
#define LV_CONF_H

/* CYD ILI9341 Color depth: 16-bit (RGB565) */
#define LV_COLOR_DEPTH 16

/* Route LVGL memory allocation directly to standard C malloc/free */
#define LV_USE_BUILTIN_MALLOC 1

/* Disable ARM-specific Assembly for ESP32 (Xtensa) */
#define LV_USE_DRAW_SW_ASM 0
#define LV_USE_DRAW_ARM2D_SYNC 0
#define LV_USE_NATIVE_HELIUM_ASM 0

#define LV_FONT_MONTSERRAT_14 1  // Keep this enabled for native symbols

/* Inject our global font instances cleanly into the library core headers */
#define LV_FONT_CUSTOM_DECLARE \
extern struct _lv_font_t font_atkinson_14; \
extern struct _lv_font_t font_atkinson_18; \
extern struct _lv_font_t font_jetbrains_14; \
extern struct _lv_font_t font_jetbrains_24;

/* Set our runtime-patched Atkinson 14px as the global default layout font */
#define LV_FONT_DEFAULT &font_atkinson_14

#endif /*LV_CONF_H*/
