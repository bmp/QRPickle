#include "crashlog.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <string.h>

namespace crashlog {

    struct Mark { uint32_t ms; uint16_t step; uint16_t core; };

    static const uint32_t MAGIC = 0x4C4B5051;  // "QPKL"
    RTC_NOINIT_ATTR static uint32_t magic;
    RTC_NOINIT_ATTR static Mark marks[SLOT_COUNT];
    static const char* const NAMES[SLOT_COUNT] = {"loop", "aprs", "hamalert", "gh_ota", "led", "sota", "net"};

    void mark(Slot slot, uint16_t step) {
        marks[slot] = {millis(), step, (uint16_t)xPortGetCoreID()};
    }

    void report_previous() {
        esp_reset_reason_t r = esp_reset_reason();
        bool abnormal = r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
                        r == ESP_RST_PANIC || r == ESP_RST_BROWNOUT;
        if (magic == MAGIC && abnormal) {
            Serial.printf("[CRASHLOG] previous reset reason=%d, last steps:\n", (int)r);
            for (int i = 0; i < SLOT_COUNT; i++) {
                Serial.printf("[CRASHLOG]   %-8s step=%u t=%lu ms core=%u\n", NAMES[i], marks[i].step,
                              (unsigned long)marks[i].ms, marks[i].core);
            }
        }
        memset(marks, 0, sizeof(marks));
        magic = MAGIC;
    }

    void dump(const char* why) {
        Serial.printf("[CRASHLOG] dump (%s) at %lu ms:\n", why, (unsigned long)millis());
        for (int i = 0; i < SLOT_COUNT; i++) {
            Serial.printf("[CRASHLOG]   %-8s step=%u t=%lu ms core=%u\n", NAMES[i], marks[i].step,
                          (unsigned long)marks[i].ms, marks[i].core);
        }
    }

}  // namespace crashlog
