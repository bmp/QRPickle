#include "theme.h"
#include "../config/config.h"
#include "../config/config_validation.h"

static_assert(ui::THEME_COUNT - 1 == config::THEME_ID_MAX, "update config::THEME_ID_MAX when adding a theme");

namespace ui {

    // Semantic tokens: Classic keeps the exact colours these places were hardcoded with; the other
    // themes use a token from their own palette, so every theme stays consistent with itself.
    struct Semantic {
        ThemeToken token;
        uint32_t classic;
        ThemeToken other;
    };
    static const Semantic SEMANTIC[] = {
        {COLOR_TEXT_ON_ACCENT, 0x000000, COLOR_BG_APP},
        {COLOR_TEXT_ON_ALERT, 0xFFFFFF, COLOR_TEXT_MAIN},
        {COLOR_STATUS_OK, 0x00FF00, COLOR_BAND_GOOD},
        {COLOR_STATUS_ERROR, 0xFF0000, COLOR_BAND_POOR},
        {COLOR_STATUS_WARN, 0xFFFF00, COLOR_BAND_FAIR},
        {COLOR_STATUS_BUSY, 0xFF9900, COLOR_BAND_FAIR},
        {COLOR_BG_SUNKEN, 0x050505, COLOR_BG_APP},
        {COLOR_BG_DEEP, 0x0A0A0A, COLOR_BG_APP},
        {COLOR_BG_INPUT, 0x111111, COLOR_BG_BAR},
        {COLOR_BG_BUTTON, 0x222222, COLOR_BG_BAR},
        {COLOR_SUCCESS, 0x3FB950, COLOR_BAND_GOOD},
        {COLOR_TEXT_ON_SUCCESS, 0xFFFFFF, COLOR_BG_APP},
        {COLOR_PENDING, 0xD4A373, COLOR_ACCENT_SECONDARY},
        {COLOR_INFO, 0x58A6FF, COLOR_ACCENT_SECONDARY},
    };

    lv_color_t theme_color(ThemeToken token) {
        uint8_t theme_id = config::get().theme_id;
        for (const Semantic& s : SEMANTIC) {
            if (s.token == token) {
                bool classic = theme_id == THEME_CLASSIC || theme_id >= THEME_COUNT;
                return classic ? lv_color_hex(s.classic) : theme_color(s.other);
            }
        }

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
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
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
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
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
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
                }
                break;

            case THEME_TERMINAL_GREEN:
                switch (token) {
                    case COLOR_BG_APP:           return lv_color_hex(0x000000);
                    case COLOR_BG_PANEL:         return lv_color_hex(0x000000);
                    case COLOR_BG_BAR:           return lv_color_hex(0x001A00);
                    case COLOR_BORDER:           return lv_color_hex(0x004400);
                    case COLOR_TEXT_MAIN:        return lv_color_hex(0x00CC00);
                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x005500);
                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0x00FF00);
                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0x009900);
                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0x00BB00);
                    case COLOR_SENSOR_PRES:      return lv_color_hex(0x00FF00);
                    case COLOR_BAND_GOOD:        return lv_color_hex(0x00FF00);
                    case COLOR_BAND_FAIR:        return lv_color_hex(0x008800);
                    case COLOR_BAND_POOR:        return lv_color_hex(0x004400);
                    case COLOR_BAND_DOWN:        return lv_color_hex(0x001500);
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
                }
                break;

            case THEME_EINK_LIGHT:
                switch (token) {
                    case COLOR_BG_APP:           return lv_color_hex(0xFFFFFF); // Pure White Base
                    case COLOR_BG_PANEL:         return lv_color_hex(0xFFFFFF);
                    case COLOR_BG_BAR:           return lv_color_hex(0xFFFFFF);
                    case COLOR_BORDER:           return lv_color_hex(0x000000); // Sharp Black Borders
                    case COLOR_TEXT_MAIN:        return lv_color_hex(0x000000); // Black Text
                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x666666); // Grayscale
                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0x000000);
                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0x333333);
                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0x000000);
                    case COLOR_SENSOR_PRES:      return lv_color_hex(0x444444);
                    case COLOR_BAND_GOOD:        return lv_color_hex(0x000000); // Solid Black
                    case COLOR_BAND_FAIR:        return lv_color_hex(0x555555); // Dark Gray
                    case COLOR_BAND_POOR:        return lv_color_hex(0xAAAAAA); // Light Gray
                    case COLOR_BAND_DOWN:        return lv_color_hex(0xDDDDDD); // Very Light Gray
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
                }
                break;

            case THEME_EINK_DARK:
                switch (token) {
                    case COLOR_BG_APP:           return lv_color_hex(0x000000); // Pure Black Base
                    case COLOR_BG_PANEL:         return lv_color_hex(0x000000);
                    case COLOR_BG_BAR:           return lv_color_hex(0x000000);
                    case COLOR_BORDER:           return lv_color_hex(0xFFFFFF); // Sharp White Borders
                    case COLOR_TEXT_MAIN:        return lv_color_hex(0xFFFFFF); // White Text
                    case COLOR_TEXT_MUTED:       return lv_color_hex(0x999999); // Grayscale
                    case COLOR_ACCENT_PRIMARY:   return lv_color_hex(0xFFFFFF);
                    case COLOR_ACCENT_SECONDARY: return lv_color_hex(0xCCCCCC);
                    case COLOR_SENSOR_TEMP:      return lv_color_hex(0xFFFFFF);
                    case COLOR_SENSOR_PRES:      return lv_color_hex(0xBBBBBB);
                    case COLOR_BAND_GOOD:        return lv_color_hex(0xFFFFFF); // Solid White
                    case COLOR_BAND_FAIR:        return lv_color_hex(0xAAAAAA); // Light Gray
                    case COLOR_BAND_POOR:        return lv_color_hex(0x555555); // Dark Gray
                    case COLOR_BAND_DOWN:        return lv_color_hex(0x222222); // Very Dark Gray
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
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
                    default:                     break;  // semantic tokens are resolved via SEMANTIC above
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
            case THEME_EINK_LIGHT:     return "E-Ink Monochrome Light";
            case THEME_EINK_DARK:      return "E-Ink Monochrome Dark";
            default:                   return "Classic Tactical";
        }
    }

} // namespace ui