#include "led_rgb.h"
#include "../services/display_manager.h" 

#define LED_PIN_R  4
#define LED_PIN_G  16
#define LED_PIN_B  17

namespace hw {
    namespace led_rgb {

        static LEDState current_state = STATE_OFF;
        static bool traffic_pulse_active = false;
        static unsigned long traffic_pulse_start = 0;

        static bool priority_strobe_active = false;
        static unsigned long priority_strobe_start = 0;

        static void write_raw_rgb(uint8_t r, uint8_t g, uint8_t b) {
            analogWrite(LED_PIN_R, 255 - r);
            analogWrite(LED_PIN_G, 255 - g);
            analogWrite(LED_PIN_B, 255 - b);
        }

        static void led_engine_task(void* pvParameters) {
            while (true) {
                unsigned long now = millis();

                // Priority Inbound Strobe (APRS / HamAlert) - PRESERVED
                if (priority_strobe_active) {
                    if (now - priority_strobe_start > 3000) {   
                        priority_strobe_active = false;
                    } else {
                        if ((now / 125) % 2 == 0) write_raw_rgb(120, 120, 120);  
                        else write_raw_rgb(150, 0, 150);   
                        vTaskDelay(20 / portTICK_PERIOD_MS);
                        continue;
                    }
                }

                if (traffic_pulse_active) {
                    if (now - traffic_pulse_start > 30) {   
                        traffic_pulse_active = false;
                    } else {
                        write_raw_rgb(0, 40, 60);  
                        vTaskDelay(5 / portTICK_PERIOD_MS);
                        continue;
                    }
                }

                switch (current_state) {
                    case STATE_OFF:
                        write_raw_rgb(0, 0, 0);
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        break;

                    case STATE_BOOT_HW:
                        write_raw_rgb(180, 45, 0);  
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                        break;

                    case STATE_BOOT_WIFI:
                        write_raw_rgb(0, 0, 150);  
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                        break;

                    case STATE_BOOT_SYNC: 
                        // FIXED: Neutered the Cyan breathing loop triggered by the timekeeper!
                        write_raw_rgb(0, 0, 0);  
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        break;

                    case STATE_BOOT_READY:
                        write_raw_rgb(0, 25, 0);  
                        vTaskDelay(1000 / portTICK_PERIOD_MS);  
                        current_state = STATE_OFF;  
                        break;

                    case STATE_WIFI_LOST: 
                        // FIXED: Neutered the Purple breathing loop.
                        write_raw_rgb(0, 0, 0);  
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        break;

                    case STATE_FAULT: {
                        unsigned long loop_pos = now % 3000;
                        if (loop_pos < 600) {
                            if ((loop_pos / 100) % 2 == 0) write_raw_rgb(200, 0, 0);  
                            else write_raw_rgb(0, 0, 0);
                        } else {
                            write_raw_rgb(0, 0, 0);  
                        }
                        vTaskDelay(30 / portTICK_PERIOD_MS);
                        break;
                    }
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