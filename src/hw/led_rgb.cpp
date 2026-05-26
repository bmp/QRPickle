#include "led_rgb.h"

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

        // FIXED: Explicit output routing prevents pins from floating and glowing
        static void write_raw_rgb(uint8_t r, uint8_t g, uint8_t b) {
            // RED CHANNEL DRIVE
            if (r == 0) {
                pinMode(LED_PIN_R, OUTPUT);    // Re-assert standard digital output driver
                digitalWrite(LED_PIN_R, HIGH); // Lock rail to solid 3.3V to stop leakage glow
            } else {
                analogWrite(LED_PIN_R, 255 - r);
            }

            // GREEN CHANNEL DRIVE
            if (g == 0) {
                pinMode(LED_PIN_G, OUTPUT);    // Re-assert standard digital output driver
                digitalWrite(LED_PIN_G, HIGH); // Lock rail to solid 3.3V to stop leakage glow
            } else {
                analogWrite(LED_PIN_G, 255 - g);
            }

            // BLUE CHANNEL DRIVE
            if (b == 0) {
                pinMode(LED_PIN_B, OUTPUT);    // Re-assert standard digital output driver
                digitalWrite(LED_PIN_B, HIGH); // Lock rail to solid 3.3V to stop leakage glow
            } else {
                analogWrite(LED_PIN_B, 255 - b);
            }
        }

        static void led_engine_task(void* pvParameters) {
            while (true) {
                unsigned long now = millis();

                // ----------------------------------------------------
                // OVERRIDE 1: High Priority Inbound Strobe (APRS / HamAlert)
                // ----------------------------------------------------
                if (priority_strobe_active) {
                    if (now - priority_strobe_start > 3000) { 
                        priority_strobe_active = false;
                    } else {
                        if ((now / 125) % 2 == 0) {
                            write_raw_rgb(120, 120, 120); 
                        } else {
                            write_raw_rgb(150, 0, 150);   
                        }
                        vTaskDelay(20 / portTICK_PERIOD_MS);
                        continue;
                    }
                }

                // ----------------------------------------------------
                // OVERRIDE 2: Crisp 30ms Data Ingress Pulse
                // ----------------------------------------------------
                if (traffic_pulse_active) {
                    if (now - traffic_pulse_start > 30) { 
                        traffic_pulse_active = false;
                    } else {
                        write_raw_rgb(0, 40, 60); 
                        vTaskDelay(5 / portTICK_PERIOD_MS);
                        continue;
                    }
                }

                // ----------------------------------------------------
                // CORE STATE MACHINE PROCESSING
                // ----------------------------------------------------
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

                    case STATE_BOOT_SYNC: {
                        float phase = (float)(now % 2000) / 2000.0f * 2.0f * PI;
                        uint8_t val = 10 + (uint8_t)((sin(phase) + 1.0f) * 55.0f); 
                        write_raw_rgb(0, val, val); 
                        vTaskDelay(20 / portTICK_PERIOD_MS);
                        break;
                    }

                    case STATE_BOOT_READY:
                        write_raw_rgb(0, 25, 0); 
                        vTaskDelay(1000 / portTICK_PERIOD_MS); 
                        current_state = STATE_OFF; 
                        break;

                    case STATE_WIFI_LOST: {
                        float phase = (float)(now % 2500) / 2500.0f * 2.0f * PI;
                        uint8_t val = 10 + (uint8_t)((sin(phase) + 1.0f) * 45.0f); 
                        write_raw_rgb(val, 0, val / 2); 
                        vTaskDelay(25 / portTICK_PERIOD_MS);
                        break;
                    }

                    case STATE_FAULT: {
                        unsigned long loop_pos = now % 3000;
                        if (loop_pos < 600) {
                            if ((loop_pos / 100) % 2 == 0) {
                                write_raw_rgb(200, 0, 0); 
                            } else {
                                write_raw_rgb(0, 0, 0);
                            }
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
            traffic_pulse_start = millis();
            traffic_pulse_active = true;
        }

        void trigger_priority_strobe() {
            priority_strobe_start = millis();
            priority_strobe_active = true;
        }

    }
}