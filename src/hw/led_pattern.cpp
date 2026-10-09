#include "led_pattern.h"

namespace hw {
    namespace led_rgb {

        Rgb pattern_color(LEDState state, uint32_t now_ms, int32_t strobe_age_ms, int32_t pulse_age_ms) {
            // Priority inbound strobe: White / Magenta every 125 ms for 3 s.
            if (strobe_age_ms >= 0 && (uint32_t)strobe_age_ms <= STROBE_MS) {
                return (now_ms / 125) % 2 == 0 ? Rgb{120, 120, 120} : Rgb{150, 0, 150};
            }
            // Traffic pulse: faint Cyan for 30 ms.
            if (pulse_age_ms >= 0 && (uint32_t)pulse_age_ms <= PULSE_MS) {
                return Rgb{0, 40, 60};
            }
            switch (state) {
                case STATE_BOOT_HW:    return Rgb{180, 45, 0};   // Amber
                case STATE_BOOT_WIFI:  return Rgb{0, 0, 150};    // Blue
                case STATE_BOOT_READY: return Rgb{0, 25, 0};     // Dim Green (the task turns it off after 1 s)
                case STATE_FAULT: {
                    // Three 100 ms red flashes in the first 600 ms of every 3 s.
                    uint32_t pos = now_ms % 3000;
                    return (pos < 600 && (pos / 100) % 2 == 0) ? Rgb{200, 0, 0} : Rgb{0, 0, 0};
                }
                case STATE_OFF:
                case STATE_BOOT_SYNC:   // breathing Cyan disabled
                case STATE_WIFI_LOST:   // breathing Magenta disabled
                default:
                    return Rgb{0, 0, 0};
            }
        }

    }
}
