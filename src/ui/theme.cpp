#include "theme.h"
#include "../config/config.h"

namespace ui {

    lv_color_t theme_color(ThemeToken token) {
        uint8_t theme_id = config::get().theme_id;

        switch (theme_id) {
            case THEME_FIELD_RED:
                switch (token) {
                    case COLOR_BG_APP:           return lv_color_hex(0x000000);
                    case COLOR_BG_PANEL:         return lv_color_hex(0x100000);
                    case COLOR_BG_BAR:           return lv_color_hex(0x1A0000);
                    case COLOR_BORDER:           return lv_color_hex(0x4A0000);
                    case COLOR_TEXT_MAIN:        return lv_color_hex(0xFF0000);
                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x800000);
                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0xFF3333);
                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0xCC0000);
                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0xFF0000);
                    case COLOR_SENSOR_PRES:      return lv_color_hex(0xFF3333);
                    case COLOR_BAND_GOOD:        return lv_color_hex(0xFF5555);
                    case COLOR_BAND_FAIR:        return lv_color_hex(0xAA0000);
                    case COLOR_BAND_POOR:        return lv_color_hex(0x550000);
                    case COLOR_BAND_DOWN:        return lv_color_hex(0x220000);
                }
                break;

                    case THEME_SLATE_DARK:
                        switch (token) {
                            case COLOR_BG_APP:           return lv_color_hex(0x0D1117);
                            case COLOR_BG_PANEL:         return lv_color_hex(0x161B22);
                            case COLOR_BG_BAR:           return lv_color_hex(0x21262D);
                            case COLOR_BORDER:           return lv_color_hex(0x30363D);
                            case COLOR_TEXT_MAIN:        return lv_color_hex(0xC9D1D9);
                            case COLOR_TEXT_MUTED:       return lv_color_hex(0x8B949E);
                            case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0x58A6FF);
                            case COLOR_ACCENT_SECONDARY: return lv_color_hex(0x1F6FEB);
                            case COLOR_SENSOR_TEMP:      return lv_color_hex(0xC9D1D9);
                            case COLOR_SENSOR_PRES:      return lv_color_hex(0x58A6FF);
                            case COLOR_BAND_GOOD:        return lv_color_hex(0x58A6FF);
                            case COLOR_BAND_FAIR:        return lv_color_hex(0x8B949E);
                            case COLOR_BAND_POOR:        return lv_color_hex(0x30363D);
                            case COLOR_BAND_DOWN:        return lv_color_hex(0x21262D);
                        }
                        break;

                            case THEME_LIGHT:
                                switch (token) {
                                    case COLOR_BG_APP:           return lv_color_hex(0xF6F8FA);
                                    case COLOR_BG_PANEL:         return lv_color_hex(0xFFFFFF);
                                    case COLOR_BG_BAR:           return lv_color_hex(0xEAEEF2);
                                    case COLOR_BORDER:           return lv_color_hex(0xD0D7DE);
                                    case COLOR_TEXT_MAIN:        return lv_color_hex(0x24292F);
                                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x57606A);
                                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0x0969DA);
                                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0x2C3E50);
                                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0x24292F);
                                    case COLOR_SENSOR_PRES:      return lv_color_hex(0x0969DA);
                                    case COLOR_BAND_GOOD:        return lv_color_hex(0x1A7F37);
                                    case COLOR_BAND_FAIR:        return lv_color_hex(0x9A6700);
                                    case COLOR_BAND_POOR:        return lv_color_hex(0xCF222E);
                                    case COLOR_BAND_DOWN:        return lv_color_hex(0x6E7781);
                                }
                                break;

                                    case THEME_TERMINAL_GREEN:
                                        // FIXED: Locked Red and Blue subpixels strictly to 0x00 for pure monochrome green phosphor output
                                        switch (token) {
                                            case COLOR_BG_APP:           return lv_color_hex(0x000000); // Pure Cathode Black
                                            case COLOR_BG_PANEL:         return lv_color_hex(0x000000); // Pure Cathode Black
                                            case COLOR_BG_BAR:           return lv_color_hex(0x001A00); // Minimal green raster baseline glow
                                            case COLOR_BORDER:           return lv_color_hex(0x004400); // Dim green framing line
                                            case COLOR_TEXT_MAIN:        return lv_color_hex(0x00CC00); // Classic P1 Phosphor text green
                                            case COLOR_TEXT_MUTED:       return lv_color_hex(0x005500); // Lower intensity text readout
                                            case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0x00FF00); // Maximum intensity highlight green
                                            case COLOR_ACCENT_SECONDARY: return lv_color_hex(0x009900); // Command line green
                                            case COLOR_SENSOR_TEMP:      return lv_color_hex(0x00BB00);
                                            case COLOR_SENSOR_PRES:      return lv_color_hex(0x00FF00);
                                            case COLOR_BAND_GOOD:        return lv_color_hex(0x00FF00); // Full trace intensity
                                            case COLOR_BAND_FAIR:        return lv_color_hex(0x008800); // Medium burn intensity
                                            case COLOR_BAND_POOR:        return lv_color_hex(0x004400); // Dim fading phosphor trace
                                            case COLOR_BAND_DOWN:        return lv_color_hex(0x001500); // Near-dead ghost phosphor line
                                        }
                                        break;

                                            default: // THEME_CLASSIC
                                                switch (token) {
                                                    case COLOR_BG_APP:           return lv_color_hex(0x000000);
                                                    case COLOR_BG_PANEL:         return lv_color_hex(0x0D1117);
                                                    case COLOR_BG_BAR:           return lv_color_hex(0x1C2128);
                                                    case COLOR_BORDER:           return lv_color_hex(0x30363D);
                                                    case COLOR_TEXT_MAIN:        return lv_color_hex(0xE6EDF3);
                                                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x8B949E);
                                                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0xFFB000);
                                                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0xCCA040);
                                                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0x58A6FF);
                                                    case COLOR_SENSOR_PRES:      return lv_color_hex(0x7EE787);
                                                    case COLOR_BAND_GOOD:        return lv_color_hex(0x7EE787);
                                                    case COLOR_BAND_FAIR:        return lv_color_hex(0xDB6D28);
                                                    case COLOR_BAND_POOR:        return lv_color_hex(0xF85149);
                                                    case COLOR_BAND_DOWN:        return lv_color_hex(0x8B949E);
                                                }
                                                break;
        }
        return lv_color_hex(0x000000);
    }

    const char* theme_get_name(uint8_t theme_id) {
        switch (theme_id) {
            case THEME_FIELD_RED:      return "Tactical Field Red";
            case THEME_SLATE_DARK:     return "GitHub Slate Dark";
            case THEME_LIGHT:          return "Clean High-Contrast";
            case THEME_TERMINAL_GREEN: return "Terminal Green";
            default:                   return "Classic Tactical";
        }
    }

} // namespace ui
