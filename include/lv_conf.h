#ifndef LV_CONF_H
#define LV_CONF_H

/* CYD ILI9341 Color depth: 16-bit (RGB565) */
/* Colour format of the display buffer (LVGL 9.6 replaced LV_COLOR_DEPTH 16 with this). */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565

/* LVGL's 64 KB memory pool is allocated from the heap once at lv_init() instead of
 * living in static DRAM (.bss). Static DRAM (dram0_0_seg) had only 64 B headroom; the
 * heap grows by the same 64 KB, so total free RAM is unchanged. (Review finding 6.5) */
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC   malloc

/* Flash budget (review area 6, S2/S3). The display is RGB565, so only the RGB565
 * software renderer (plus RGB565A8 sources and A8 masks/fonts) is needed: ~53 KB saved.
 * NOTE: LVGL renders semi-transparent or transformed containers into ARGB8888 layers;
 * if a screen ever needs that, re-enable LV_DRAW_SW_SUPPORT_ARGB8888. */
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED       0
#define LV_DRAW_SW_SUPPORT_RGB888               0
#define LV_DRAW_SW_SUPPORT_XRGB8888             0
#define LV_DRAW_SW_SUPPORT_ARGB8888             0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8                   0
#define LV_DRAW_SW_SUPPORT_AL88                 0
#define LV_DRAW_SW_SUPPORT_I1                   0

/* Widgets and features QRPickle never uses (each also drags in theme styles). Used: label, button,
 * image, textarea + keyboard (+ buttonmatrix), dropdown, checkbox, slider (+ bar), msgbox, tabview,
 * flex layout. v0.2.4 size work: the second group below saved 17 KB. */
#define LV_USE_CHART    0
#define LV_USE_SCALE    0
#define LV_USE_CALENDAR 0
#define LV_USE_TABLE    0
#define LV_USE_ROLLER   0
#define LV_USE_SPINBOX  0
#define LV_USE_ANIMIMG     0
#define LV_USE_ARC         0
#define LV_USE_ARCLABEL    0
#define LV_USE_CANVAS      0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_LED         0
#define LV_USE_LINE        0
#define LV_USE_LIST        0
#define LV_USE_MENU        0
#define LV_USE_SPAN        0
#define LV_USE_SPINNER     0
#define LV_USE_SWITCH      0
#define LV_USE_TILEVIEW    0
#define LV_USE_WIN         0
#define LV_USE_OBSERVER    0
#define LV_USE_GRID        0

/* Route LVGL memory allocation directly to standard C malloc/free */
#define LV_USE_BUILTIN_MALLOC 1

/* Disable ARM-specific Assembly for ESP32 (Xtensa) */
#define LV_USE_DRAW_SW_ASM 0
#define LV_USE_DRAW_ARM2D_SYNC 0
#define LV_USE_NATIVE_HELIUM_ASM 0

/* Layers (semi-transparent or transformed objects) render in chunks of this size, allocated from
 * the pool: the 24 KB default made boot the pool's peak (v0.2.4 memory work, mem_stats). */
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (8 * 1024)

/* Image cache (decoded icons from LittleFS), allocated from the 64 KB pool. 32 KB let the weather
 * page's forecast icons fill half the pool; the next busy page (DX cluster) then ran LVGL out of
 * memory and crashed in lv_draw_add_task (v0.2.4, mem_stats). Icons beyond the cache are re-read. */
#define LV_CACHE_DEF_SIZE       (16 * 1024)

/*==================
 * FONT USAGE
 *===================*/

/* Montserrat fonts with ASCII range and some symbols using bpp = 4 */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0  /* replaced by src/ui/fonts/font_symbols_10 (icons only) */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0  /* replaced by src/ui/fonts/font_symbols_14 (icons only) */
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
