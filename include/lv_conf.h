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

/* LVGL cache for fonts and icons */
#define LV_CACHE_DEF_SIZE       32768

/*==================
 * FONT USAGE
 *===================*/

/* Montserrat fonts with ASCII range and some symbols using bpp = 4 */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /* bpp = 3 */
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /* Hebrew, Arabic, Persian letters */
#define LV_FONT_SIMSUN_16_CJK            0  /* 1000 most common CJK radicals */
#define LV_FONT_UNSCII_8                 0
#define LV_FONT_UNSCII_16                0

/* Inject our global font instances cleanly into the library core headers */
#define LV_FONT_CUSTOM_DECLARE \
extern struct _lv_font_t font_atkinson_14; \
extern struct _lv_font_t font_atkinson_18; \
extern struct _lv_font_t font_jetbrains_14; \
extern struct _lv_font_t font_jetbrains_24;

/* Set our runtime-patched Atkinson 14px as the global default layout font */
#define LV_FONT_DEFAULT &font_atkinson_14

#endif /*LV_CONF_H*/
