#include "../core/crashlog.h"
#include "led_rgb.h"
#include "../services/display_manager.h" 

#define LED_PIN_R  4
#define LED_PIN_G  16
#define LED_PIN_B  17

namespace hw {
    namespace led_rgb {

        static volatile LEDState current_state = STATE_OFF;
        static volatile bool traffic_pulse_active = false;
        static volatile unsigned long traffic_pulse_start = 0;

        static volatile bool priority_strobe_active = false;
        static volatile unsigned long priority_strobe_start = 0;

        static void write_raw_rgb(uint8_t r, uint8_t g, uint8_t b) {
            crashlog::mark(crashlog::SLOT_LED, 2); analogWrite(LED_PIN_R, pwm_duty(r));
            crashlog::mark(crashlog::SLOT_LED, 3); analogWrite(LED_PIN_G, pwm_duty(g));
            crashlog::mark(crashlog::SLOT_LED, 4); analogWrite(LED_PIN_B, pwm_duty(b));
            crashlog::mark(crashlog::SLOT_LED, 5);
        }

        // How long to hold a colour before re-evaluating (unchanged from the original loop).
        static uint32_t hold_ms(LEDState state, bool strobe, bool pulse) {
            if (strobe) return 20;
            if (pulse) return 5;
            switch (state) {
                case STATE_BOOT_HW:
                case STATE_BOOT_WIFI:  return 50;
                case STATE_BOOT_READY: return 1000;
                case STATE_FAULT:      return 30;
                default:               return 100;
            }
        }

        static void led_engine_task(void* pvParameters) {
            Rgb shown = {0, 0, 0};  // init() leaves the LED off
            while (true) {
                crashlog::mark(crashlog::SLOT_LED, 1);
                const uint32_t now = millis();

                int32_t strobe_age = priority_strobe_active ? (int32_t)(now - priority_strobe_start) : -1;
                if (strobe_age > (int32_t)STROBE_MS) { priority_strobe_active = false; strobe_age = -1; }
                int32_t pulse_age = traffic_pulse_active ? (int32_t)(now - traffic_pulse_start) : -1;
                if (pulse_age > (int32_t)PULSE_MS) { traffic_pulse_active = false; pulse_age = -1; }

                const LEDState state = current_state;
                const Rgb c = pattern_color(state, now, strobe_age, pulse_age);
                // Only touch the PWM channels when the colour changes (review 5.3): in standby the
                // task used to rewrite all three every 100 ms forever.
                if (c.r != shown.r || c.g != shown.g || c.b != shown.b) {
                    write_raw_rgb(c.r, c.g, c.b);
                    shown = c;
                }
                vTaskDelay(pdMS_TO_TICKS(hold_ms(state, strobe_age >= 0, pulse_age >= 0)));

                // "Ready" green is shown for one hold (1 s), then the LED goes dark.
                if (strobe_age < 0 && pulse_age < 0 && state == STATE_BOOT_READY && current_state == STATE_BOOT_READY) {
                    current_state = STATE_OFF;
                }
            }
        }

        void init() {
            pinMode(LED_PIN_R, OUTPUT);
            pinMode(LED_PIN_G, OUTPUT);
            pinMode(LED_PIN_B, OUTPUT);
            write_raw_rgb(0, 0, 0);  
            xTaskCreatePinnedToCore(led_engine_task, "rgb_telemetry_loop", 2048, NULL, 1, NULL, 0);
        }

        void set_state(LEDState new_state) {
            current_state = new_state;
        }

        void trigger_traffic_pulse() {
            if (services::display_manager::is_sleeping()) return;  
            traffic_pulse_start = millis();
            traffic_pulse_active = true;
        }

        void trigger_priority_strobe() {
            priority_strobe_start = millis();
            priority_strobe_active = true;
        }

    }
}