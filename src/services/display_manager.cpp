#include "display_manager.h"
#include "../config/config.h"
#include <lvgl.h>
#include <driver/adc.h>

#define TFT_BL_PIN 21
#define LDR_ADC_PIN ADC1_CHANNEL_6

namespace services {
    namespace display_manager {

        static bool sleeping = false;
        static unsigned long wake_timestamp = 0;
        static constexpr uint32_t GHOST_TAP_LOCKOUT_MS = 350; 

        static unsigned long last_ldr_read_ms = 0;
        static float smoothed_pwm = 0.0f;

        void init() {
            pinMode(TFT_BL_PIN, OUTPUT);
            adc1_config_width(ADC_WIDTH_BIT_12);
            adc1_config_channel_atten(LDR_ADC_PIN, ADC_ATTEN_DB_12);

            smoothed_pwm = (float)config::get().brightness;
            analogWrite(TFT_BL_PIN, config::get().brightness);
        }

        void force_ldr_sample() {
            last_ldr_read_ms = 0; // Forces update() to sample immediately
        }

        void update() {
            if (sleeping) return;

            // FIXED: Pull dynamic timer target from active configuration
            uint32_t timeout_min = config::get().screen_timeout_min;
            
            // 0 means manual sleep only (bypass inactivity checks entirely)
            if (timeout_min == 0) return;

            if (timeout_min > 0) {
                uint32_t timeout_ms = timeout_min * 60000;
                if (lv_display_get_inactive_time(lv_display_get_default()) > timeout_ms) {
                    sleep();
                    return; // Stop processing if we went to sleep
                }
            }

            // 2. LDR Auto-Brightness Logic (Runs every 15 seconds)
            if (config::get().auto_brightness) {
                if (millis() - last_ldr_read_ms > 15000) {
                    last_ldr_read_ms = millis();

                    int raw_ldr = adc1_get_raw(LDR_ADC_PIN);
                    if (raw_ldr < 0) return;

                    float target_pwm = map(raw_ldr, 0, 4095, 10, 255);
                    if (target_pwm < 10) target_pwm = 10;
                    if (target_pwm > 255) target_pwm = 255;

                    smoothed_pwm = (0.3f * target_pwm) + (0.7f * smoothed_pwm);

                    uint8_t final_pwm = (uint8_t)smoothed_pwm;
                    analogWrite(TFT_BL_PIN, final_pwm);

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
                    analogWrite(TFT_BL_PIN, last_manual_write);

                    Serial.printf("[Display] Manual Mode | PWM Restored: %d\n", last_manual_write);
                }
            }

            uint32_t timeout_ms = timeout_min * 60000;

            if (lv_display_get_inactive_time(lv_display_get_default()) > timeout_ms) {
                sleep();
            }
        }

        void sleep() {
            if (sleeping) return;
            sleeping = true;
            analogWrite(TFT_BL_PIN, 0); 
            Serial.println("[DisplayManager] Screen asleep.");
        }

        void wake() {
            if (!sleeping) return;
            sleeping = false;
            wake_timestamp = millis();

            analogWrite(TFT_BL_PIN, config::get().brightness);
            lv_display_trigger_activity(lv_display_get_default());
            
            Serial.println("[DisplayManager] Hardware interrupt. Screen awakened.");
        }

        void set_brightness(uint8_t level) {
            if (!sleeping) {
                analogWrite(TFT_BL_PIN, level);
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
