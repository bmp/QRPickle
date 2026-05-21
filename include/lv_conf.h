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

#define LV_FONT_MONTSERRAT_14 1  // Keep this enabled for normal labels/buttons
#define LV_FONT_MONTSERRAT_24 1  // THE UPGRADE: Enable for Local Time/Callsign readouts
#define LV_FONT_MONTSERRAT_32 1  // THE UPGRADE: Enable for massive high-visibility UTC Time

#endif /*LV_CONF_H*/
