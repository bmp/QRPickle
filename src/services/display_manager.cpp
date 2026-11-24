#include "display_manager.h"
#include "../config/config.h"
#include "../hw/board.h"
#include "../hw/touch.h"
#include <lvgl.h>
#include <driver/adc.h>

namespace services {
    namespace display_manager {

        static bool sleeping = false;
        static unsigned long wake_timestamp = 0;
        static constexpr uint32_t GHOST_TAP_LOCKOUT_MS = 350; 

        static unsigned long last_ldr_read_ms = 0;
        static float smoothed_pwm = 0.0f;

        static bool ready = false;    // the panel is cleared: the backlight may come on
        static int8_t bl_pin = -1;    // -1: this board's backlight must stay off (board selection)
        static bool bl_active_high = true;
        static int ldr_channel = -1;  // ADC1 channel of the light sensor, -1 if the board has none

        // GPIO -> ADC1 channel (ESP32): 36..39 -> 0..3, 32..35 -> 4..7.
        static int adc1_channel(int8_t gpio) {
            if (gpio >= 36 && gpio <= 39) return gpio - 36;
            if (gpio >= 32 && gpio <= 35) return gpio - 28;
            return -1;
        }

        // Every backlight write goes through here: off until the panel is ready, never on a pin the
        // board selection keeps off, inverted for boards whose backlight is active low.
        static void write_bl(uint8_t level) {
            if (bl_pin < 0) return;
            if (!ready) level = 0;
            analogWrite(bl_pin, bl_active_high ? level : 255 - level);
        }

        void init() {
            const hw::BoardDef& b = hw::board::active();
            const bool allowed = hw::board::selection().backlight_on && hw::board::pin_allowed(b.bl_pin);
            bl_pin = allowed ? b.bl_pin : -1;
            bl_active_high = b.bl_active_high;
            if (bl_pin >= 0) pinMode(bl_pin, OUTPUT);
            write_bl(0);

            ldr_channel = adc1_channel(b.ldr_gpio);
            if (ldr_channel >= 0) {
                adc1_config_width(ADC_WIDTH_BIT_12);
                adc1_config_channel_atten((adc1_channel_t)ldr_channel, ADC_ATTEN_DB_12);
            }
            smoothed_pwm = (float)config::get().brightness;
        }

        void backlight_ready() {
            ready = true;
            write_bl(config::get().brightness);
        }

        void force_ldr_sample() {
            last_ldr_read_ms = 0; // Forces update() to sample immediately
        }

        void update() {
            if (sleeping) return;

            // Sleep after the configured idle time. 0 = manual sleep only. Without a working touch chip
            // nothing could wake the screen again, so it never sleeps by itself there. (Before v0.3.2 a
            // timeout of 0 also returned here and so skipped the brightness logic below.)
            const uint32_t timeout_min = config::get().screen_timeout_min;
            if (timeout_min > 0 && touch_available() &&
                lv_display_get_inactive_time(lv_display_get_default()) > timeout_min * 60000) {
                sleep();
                return;
            }

            // LDR Auto-Brightness Logic (Runs every 15 seconds)
            if (config::get().auto_brightness && ldr_channel >= 0) {
                if (millis() - last_ldr_read_ms > 15000) {
                    last_ldr_read_ms = millis();

                    int raw_ldr = adc1_get_raw((adc1_channel_t)ldr_channel);
                    if (raw_ldr < 0) return;

                    float target_pwm = map(raw_ldr, 0, 4095, 10, 255);
                    if (target_pwm < 10) target_pwm = 10;
                    if (target_pwm > 255) target_pwm = 255;

                    smoothed_pwm = (0.3f * target_pwm) + (0.7f * smoothed_pwm);

                    uint8_t final_pwm = (uint8_t)smoothed_pwm;
                    write_bl(final_pwm);

                    // Optional: You can comment this line out now that we know it works!
                    // Serial.printf("[LDR] Raw HW ADC: %d | Target: %.1f | Smoothed: %.1f -> Writing: %d\n", raw_ldr, target_pwm, smoothed_pwm, final_pwm);
                    Serial.printf("[Display] Auto Mode | LDR ADC: %d | PWM Written: %d\n", raw_ldr, final_pwm);
                }
            } else {
                // If Auto is OFF, ensure the screen respects the manual config value!
                // We only write it once to avoid spamming the PWM controller.
                static uint8_t last_manual_write = 0;
                if (last_manual_write != config::get().brightness) {
                    last_manual_write = config::get().brightness;
                    write_bl(last_manual_write);

                    Serial.printf("[Display] Manual Mode | PWM Restored: %d\n", last_manual_write);
                }
            }
        }

        void sleep() {
            if (sleeping) return;
            sleeping = true;
            write_bl(0); 
            Serial.println("[DisplayManager] Screen asleep.");
        }

        void wake() {
            if (!sleeping) return;
            sleeping = false;
            wake_timestamp = millis();

            write_bl(config::get().brightness);
            lv_display_trigger_activity(lv_display_get_default());
            
            Serial.println("[DisplayManager] Hardware interrupt. Screen awakened.");
        }

        void set_brightness(uint8_t level) {
            if (!sleeping) {
                write_bl(level);
                Serial.printf("[Display] Slider Adjusted | PWM Written: %d\n", level);
            }
        }

        bool is_sleeping() { return sleeping; }

        bool should_block_touch() {
            if (millis() - wake_timestamp < GHOST_TAP_LOCKOUT_MS) return true;
            return false;
        }
    }
}
