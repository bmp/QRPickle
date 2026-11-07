#include "net_lock.h"
#include "lock_watch.h"
#include "../core/crashlog.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
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

    // Who holds the lock and since when (outermost take only); changed only by the holder.
    static volatile uint32_t held_since_ms = 0;
    static const char* volatile holder_name = nullptr;
    static int depth = 0;
    // The task that made the board restart (lock_watch), kept across the restart in RTC memory.
    static constexpr uint32_t STUCK_MAGIC = 0x4e4c4b21;  // "NLK!"
    RTC_NOINIT_ATTR static uint32_t stuck_magic;
    RTC_NOINIT_ATTR static char stuck_name[16];
    static char last_stuck[16] = "";

    NetLock::NetLock(uint32_t timeout_ms) {
        SemaphoreHandle_t m = get_mutex();
        held_ = m && xSemaphoreTakeRecursive(m, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
        if (!held_) {
            Serial.println("[Net] Network busy; operation skipped.");
        } else if (depth++ == 0) {
            holder_name = pcTaskGetName(nullptr);
            held_since_ms = millis() | 1;
        }
    }

    NetLock::~NetLock() {
        if (!held_) return;
        if (--depth == 0) {
            held_since_ms = 0;
            holder_name = nullptr;
        }
        xSemaphoreGiveRecursive(mtx);
    }

    void net_lock_boot_report() {
        if (stuck_magic == STUCK_MAGIC) {
            stuck_name[sizeof(stuck_name) - 1] = 0;
            strlcpy(last_stuck, stuck_name, sizeof(last_stuck));
            Serial.printf("[Net] restarted after task %s held the network over %u s\n", last_stuck, LOCK_RESTART_S);
        }
        stuck_magic = 0;
    }

    const char* net_lock_last_stuck() { return last_stuck; }

    bool net_lock_busy() { return held_since_ms != 0; }

    void net_lock_watch() {
        static bool warned = false;
        const uint32_t since = held_since_ms;
        if (!since) {
            warned = false;
            return;
        }
        const unsigned s = lock_held_s(since, millis());
        const char* who = holder_name ? holder_name : "?";
        switch (lock_watch(s)) {
            case LockWatch::OK:
                break;
            case LockWatch::WARN:
                if (!warned) Serial.printf("[Net] %s has held the network for %u s\n", who, LOCK_WARN_S);
                warned = true;
                break;
            case LockWatch::RESTART:
                strlcpy(stuck_name, who, sizeof(stuck_name));
                stuck_magic = STUCK_MAGIC;
                Serial.printf("[Net] %s has held the network for %u s; restarting\n", who, s);
                crashlog::dump("network lock held too long");
                Serial.flush();
                esp_restart();
        }
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
