#include "net_lock.h"
#include <Arduino.h>
#include <freertos/semphr.h>

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

}  // namespace services
