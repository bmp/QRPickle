#include "net_lock.h"
#include <Arduino.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

namespace services {

    static SemaphoreHandle_t mtx = nullptr;
    static portMUX_TYPE create_mux = portMUX_INITIALIZER_UNLOCKED;

    static SemaphoreHandle_t get_mutex() {
        if (!mtx) {
            SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
            portENTER_CRITICAL(&create_mux);
            if (!mtx) { mtx = m; m = nullptr; }
            portEXIT_CRITICAL(&create_mux);
            if (m) vSemaphoreDelete(m);  // another task won the race
        }
        return mtx;
    }

    NetLock::NetLock(uint32_t timeout_ms) {
        SemaphoreHandle_t m = get_mutex();
        held_ = m && xSemaphoreTakeRecursive(m, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
        if (!held_) Serial.println("[Net] Network busy; operation skipped.");
    }

    NetLock::~NetLock() {
        if (held_) xSemaphoreGiveRecursive(mtx);
    }

    bool tls_heap_ready(const char* who, uint32_t wait_ms) {
        uint32_t free_b = 0, block = 0;
        for (uint32_t t = 0;; t += 100) {
            free_b = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            if (free_b >= TLS_MIN_FREE && block >= TLS_MIN_BLOCK) return true;
            if (t >= wait_ms) break;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        Serial.printf("[Net] %s postponed: %u B free, largest block %u B (TLS needs %u / %u)\n", who,
                      (unsigned)free_b, (unsigned)block, (unsigned)TLS_MIN_FREE, (unsigned)TLS_MIN_BLOCK);
        return false;
    }

}  // namespace services
