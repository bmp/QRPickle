# Stability Fixes (Plan 1 of 5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the three owner-observed failures: the device stuck on the setup AP after a power cut (review 3.1), duplicate background tasks (3.2), and crashes when leaving xOTA (4.1). Also stop `web_server_init()` running twice.

**Architecture:**
- **WiFi:** after a failed boot connection, the device runs AP and station together (`WIFI_AP_STA`) and keeps retrying the saved network. Once it connects, it drops the AP.
- **HamAlert and APRS tasks:** each gets an atomic `task_alive` flag that the task clears on exit. `start()` refuses to run while an old task is still alive.
- **xOTA:** the 2KB `resume_task` is replaced by a one-shot `lv_timer` on the main loop. It waits until the old tasks are gone, and is cancelled if xOTA is re-entered. The xOTA fetch likewise waits for the old tasks to exit.

**Tech Stack:** ESP32 Arduino (ESP-IDF 4.4), FreeRTOS, LVGL 9.5, PlatformIO 6.2.

**Testing reality:** none of this code compiles for the `native` test environment (it uses WiFi, FreeRTOS and LVGL), so there are no unit tests in this plan. Each task is verified by (a) a successful firmware build and (b) an on-device check with the exact serial log lines to look for. Do not mark a task done on a build alone.

**Source of findings:** `docs/reviews/2026-10-code-review.md` (items 3.1, 3.2, 4.1).

---

## Roadmap

Moved to the "Status and roadmap" section of `docs/reviews/2026-10-code-review.md`.

---

## File map

| File | Change |
|---|---|
| `src/services/wifi_manager.h` | Add `WIFI_STATE_AP_FALLBACK` to the enum |
| `src/services/wifi_manager.cpp` | Shared `bring_up_ap()`; fallback AP in `WIFI_AP_STA`; periodic station retry; tear down the AP on reconnect |
| `src/services/web_server.cpp` | Make `web_server_init()` idempotent |
| `src/services/hamalert_manager.h/.cpp` | `task_alive` flag; `is_stopped()`; guarded `start()` |
| `src/services/aprs_manager.h/.cpp` | The same as HamAlert |
| `src/ui/screens/xota.cpp` | Replace `async_resume_task` with `resume_timer`; cancel it on re-entry; the fetch waits for tasks to stop |

Build command for every task: `pio run`. Expected: `[SUCCESS]`, with Flash still close to 86.9%. Then restore the artifacts with `git checkout -- release/`. Run the `build-verify` skill before each commit.

---

### Task 1: Make `web_server_init()` idempotent

**Files:**
- Modify: `src/services/web_server.cpp` (the start of `web_server_init()`, currently line 60)

**Why:** `wifi_manager_start_ap()` calls `web_server_init()`, and `setup()` calls it again afterwards. Every route is registered twice and `server.begin()` runs twice.

- [x] **Step 1: Add the guard**

Replace:
```cpp
void web_server_init() {
    WiFi.setSleep(false);
```
with:
```cpp
void web_server_init() {
    // Called from setup() and again whenever the setup AP comes up; register routes only once.
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    WiFi.setSleep(false);
```

- [x] **Step 2: Build**

Run: `pio run`. Expected: `[SUCCESS]`.

- [x] **Step 3: Commit**
```bash
git add src/services/web_server.cpp
git commit -m "fix: register web routes once even when setup AP starts"
```

---

### Task 2: Add the WiFi fallback state that keeps retrying the saved network (review 3.1)

**Files:**
- Modify: `src/services/wifi_manager.h` (the `wifi_state_t` enum)
- Modify: `src/services/wifi_manager.cpp` (lines 10–15, 46–63, 65–108)

- [x] **Step 1: Add the state to the enum** in `wifi_manager.h`:
```cpp
enum wifi_state_t {
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
    WIFI_STATE_AP_MODE,           // No network configured: setup AP only
    WIFI_STATE_AP_FALLBACK,       // Network configured but unreachable: setup AP + station retrying
    WIFI_STATE_BACKGROUND_RETRY
};
```

- [x] **Step 2: Add the retry constants** in `wifi_manager.cpp`, after `static char status_msg[64] = "DISCONNECTED";`:
```cpp
// While the fallback setup AP is up, retry the saved network this often.
static const unsigned long STA_RETRY_INTERVAL_MS = 60000;
static unsigned long last_sta_retry_mark = 0;
```

- [x] **Step 3: Replace `wifi_manager_start_ap()`** (lines 46–63) with a shared helper, the AP-only entry point and the fallback entry point:
```cpp
static bool bring_up_ap(wifi_mode_t mode) {
    WiFi.mode(mode);
    if (!WiFi.softAP(AP_SSID)) {
        snprintf(status_msg, sizeof(status_msg), "HOTSPOT INITIALIZATION FAULT");
        return false;
    }
    IPAddress ap_ip = WiFi.softAPIP();
    snprintf(status_msg, sizeof(status_msg), "AP ACTIVE | SSID: %s | IP: %d.%d.%d.%d",
             AP_SSID, ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
    Serial.printf("[Wi-Fi] Hotspot Broadcast Up! %s\n", status_msg);
    web_server_init();
    hw::led_rgb::set_state(hw::led_rgb::STATE_BOOT_HW);
    return true;
}

// No network configured: AP only, nothing to retry.
void wifi_manager_start_ap() {
    WiFi.disconnect(true, true);
    delay(100);
    if (bring_up_ap(WIFI_AP)) current_state = WIFI_STATE_AP_MODE;
}

// Network configured but unreachable (e.g. router still booting after a power cut):
// keep the station interface so the saved network is retried while the setup AP is up.
static void start_fallback_ap() {
    if (bring_up_ap(WIFI_AP_STA)) {
        current_state = WIFI_STATE_AP_FALLBACK;
        last_sta_retry_mark = millis();
    }
}
```

- [x] **Step 4: Handle the fallback state in `wifi_manager_update()`.** Directly after `if (current_state == WIFI_STATE_AP_MODE) return;` (line 70), insert:
```cpp
    if (current_state == WIFI_STATE_AP_FALLBACK) {
        if (WiFi.status() != WL_CONNECTED) {
            if (millis() - last_sta_retry_mark > STA_RETRY_INTERVAL_MS) {
                Serial.println("[Wi-Fi] Fallback AP up. Retrying saved network...");
                WiFi.begin(config::get().wifi_ssid, config::get().wifi_password);
                last_sta_retry_mark = millis();
            }
            return;
        }
        Serial.println("[Wi-Fi] Saved network reachable again. Shutting down fallback AP...");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        // Fall through: the WL_CONNECTED branch below records the CONNECTED state.
    }
```

- [x] **Step 5: Use the fallback on the boot timeout.** Replace (lines 103–106):
```cpp
        if (current_state == WIFI_STATE_CONNECTING && (millis() - connection_timeout_mark > 20000)) {
            Serial.println("[Wi-Fi] Boot connection timeout. Dropping back to Fallback Setup AP Mode...");
            wifi_manager_start_ap();
        }
```
with:
```cpp
        if (current_state == WIFI_STATE_CONNECTING && (millis() - connection_timeout_mark > 20000)) {
            Serial.println("[Wi-Fi] Boot connection timeout. Starting fallback setup AP; will keep retrying saved network...");
            start_fallback_ap();
        }
```

- [x] **Step 6: Build.** Run: `pio run`. Expected: `[SUCCESS]`.

- [x] **Step 7: On-device test** (this is the scenario the owner observed)
  1. Power off the router. Power-cycle the CYD.
  2. After about 20 seconds, the serial log shows `Starting fallback setup AP; will keep retrying saved network...`, and the `QRPickle-Setup` network is visible.
  3. Power the router back on. Within about 60 seconds of the router being ready, the log shows `Fallback AP up. Retrying saved network...`, then `Saved network reachable again. Shutting down fallback AP...`, then `Network Link Stable!`.
  4. `QRPickle-Setup` disappears. The dashboard, weather and spots work.
  5. Regression check: erase the saved SSID (or test on a fresh device). It must go to AP-only mode with no retry messages.

- [x] **Step 8: Commit**
```bash
git add src/services/wifi_manager.h src/services/wifi_manager.cpp
git commit -m "fix: keep retrying saved WiFi while fallback setup AP is up"
```

---

### Task 3: HamAlert task lifecycle guard (review 3.2)

**Files:**
- Modify: `src/services/hamalert_manager.h` (the public section of the class)
- Modify: `src/services/hamalert_manager.cpp` (the includes, `start()` at lines 18–33, the end of `task_loop()` at line 209)

- [x] **Step 1: Declare `is_stopped()`** in the `public:` section of `HamAlertManager`, after `static void stop();`:
```cpp
        // True once the background task has fully exited (stop() only requests the exit).
        static bool is_stopped();
```

- [x] **Step 2: Add the alive flag.** In `hamalert_manager.cpp`, add `#include <atomic>` with the other includes. After `bool HamAlertManager::running = false;`, add:
```cpp
    // Set before the task is created, cleared by the task as its last action.
    static std::atomic<bool> task_alive{false};
```

- [x] **Step 3: Guard `start()` and check the task creation result.** Change the first line of `start()` from:
```cpp
        if (running) return;
```
to:
```cpp
        if (running || task_alive) return;  // previous task may still be exiting
```
Then replace:
```cpp
        running = true;
        xTaskCreate(task_loop, "hamalert_task", 3072, NULL, 1, NULL);
```
with:
```cpp
        running = true;
        task_alive = true;
        if (xTaskCreate(task_loop, "hamalert_task", 3072, NULL, 1, NULL) != pdPASS) {
            running = false;
            task_alive = false;
            Serial.println("[HamAlert-Engine] Task creation failed (heap).");
        }
```

- [x] **Step 4: Add `is_stopped()`** after `stop()`:
```cpp
    bool HamAlertManager::is_stopped() { return !task_alive; }
```

- [x] **Step 5: Clear the flag on exit.** At the end of `task_loop()`, replace:
```cpp
        Serial.println("[HamAlert-Socket] Safely suspended for Time-Slicing.");
        vTaskDelete(NULL);
```
with:
```cpp
        Serial.println("[HamAlert-Socket] Safely suspended for Time-Slicing.");
        task_alive = false;
        vTaskDelete(NULL);
```

- [x] **Step 6: Build.** Run: `pio run`. Expected: `[SUCCESS]`.

- [x] **Step 7: Commit**
```bash
git add src/services/hamalert_manager.h src/services/hamalert_manager.cpp
git commit -m "fix: prevent duplicate HamAlert tasks on fast stop/start"
```

---

### Task 4: APRS task lifecycle guard (review 3.2)

**Files:**
- Modify: `src/services/aprs_manager.h` (the public section of the class)
- Modify: `src/services/aprs_manager.cpp` (the includes, `start()` at lines 32–43, the end of `task_loop()` at lines 400–402)

- [x] **Step 1: Declare `is_stopped()`** in the `public:` section of `AprsManager`, after `static void stop();`:
```cpp
        // True once the background task has fully exited (stop() only requests the exit).
        static bool is_stopped();
```

- [x] **Step 2: Add the alive flag.** In `aprs_manager.cpp`, add `#include <atomic>` with the other includes. After `bool AprsManager::running = false;`, add:
```cpp
    // Set before the task is created, cleared by the task as its last action.
    static std::atomic<bool> task_alive{false};
```

- [x] **Step 3: Guard `start()` and check the task creation result.** Change:
```cpp
        if (running || !config::get().aprs_enabled) return;
```
to:
```cpp
        if (running || task_alive || !config::get().aprs_enabled) return;  // previous task may still be exiting
```
Then replace:
```cpp
        running = true;
        xTaskCreate(task_loop, "aprs_task", 10240, NULL, 1, NULL);
```
with:
```cpp
        running = true;
        task_alive = true;
        if (xTaskCreate(task_loop, "aprs_task", 10240, NULL, 1, NULL) != pdPASS) {
            running = false;
            task_alive = false;
            Serial.println("[APRS] Task creation failed (heap).");
        }
```

- [x] **Step 4: Add `is_stopped()`** after `stop()`:
```cpp
    bool AprsManager::is_stopped() { return !task_alive; }
```

- [x] **Step 5: Clear the flag on exit.** At the end of `task_loop()`, replace:
```cpp
        client.stop();
        connected = false;
        vTaskDelete(NULL);
```
with:
```cpp
        client.stop();
        connected = false;
        task_alive = false;
        vTaskDelete(NULL);
```

- [x] **Step 6: Build.** Run: `pio run`. Expected: `[SUCCESS]`.

- [x] **Step 7: Commit**
```bash
git add src/services/aprs_manager.h src/services/aprs_manager.cpp
git commit -m "fix: prevent duplicate APRS tasks on fast stop/start"
```

---

### Task 5: Replace the xOTA resume task with a main-loop timer (review 4.1, completes 3.2)

**Files:**
- Modify: `src/ui/screens/xota.cpp` (the statics near line 34; `execute_delayed_fetch` at line 75; `async_resume_task` at lines 232–239; the start of `draw_xota_page` at line 241; the delete handler at line 438; lines 442–443)

**Depends on:** Tasks 3 and 4 (`is_stopped()`).

- [x] **Step 1: Add the timer handle.** After `static lv_timer_t* delayed_fetch_timer = nullptr;` (line 34), add:
```cpp
    // Lives outside the screen's lifetime: restarts the socket services after leaving xOTA.
    static lv_timer_t* resume_timer = nullptr;
```

- [x] **Step 2: Make the fetch wait for the old tasks to exit.** At the very top of `execute_delayed_fetch()`, before `if (active_tab == TAB_POTA) {`, insert:
```cpp
        // Don't open a TLS session while the old APRS/HamAlert tasks still hold their stacks
        // and sockets; stop() only requests the exit. Retry on the next tick (100 ms).
        if (!services::HamAlertManager::is_stopped() || !services::AprsManager::is_stopped()) {
            return;
        }
```

- [x] **Step 3: Replace `async_resume_task`** (lines 232–239) with a timer callback:
```cpp
    static void resume_services_cb(lv_timer_t* t) {
        // Wait until the previous task instances have fully exited, then restart once.
        if (!services::HamAlertManager::is_stopped() || !services::AprsManager::is_stopped()) {
            return;  // try again on the next tick
        }
        Serial.println("[xOTA] Quiet period ended. Re-establishing core TCP sockets...");
        services::DxManager::start();
        services::HamAlertManager::start();
        services::AprsManager::start();
        lv_timer_delete(t);
        resume_timer = nullptr;
    }
```

- [x] **Step 4: Cancel a pending resume on re-entry.** At the top of `draw_xota_page()`, before `Serial.println("[xOTA] Entry. ...`, insert:
```cpp
        // Re-entered within the quiet period: the services must stay stopped.
        if (resume_timer) { lv_timer_delete(resume_timer); resume_timer = nullptr; }
```

- [x] **Step 5: Schedule the timer instead of creating a task.** In the `LV_EVENT_DELETE` handler, replace:
```cpp
            xTaskCreate(async_resume_task, "resume_task", 2048, NULL, 1, NULL);
```
with:
```cpp
            if (!resume_timer) resume_timer = lv_timer_create(resume_services_cb, 2000, NULL);
```

- [x] **Step 6: Remove the dead nested handler call** (review 4.2). Replace:
```cpp
        // Force LVGL to physically draw the initial canvas and the big loading label
        lv_timer_handler();

        // Queue the initial dynamic fetch sequence
```
with:
```cpp
        // The fetch runs from a 100 ms timer so LVGL renders the loading label first
        // (a nested lv_timer_handler() call here would be ignored by LVGL's re-entrancy guard).
```

- [x] **Step 7: Build.** Run: `pio run`. Expected: `[SUCCESS]`, with no reference to `async_resume_task` remaining (`grep -n async_resume_task src/ui/screens/xota.cpp` prints nothing).

- [x] **Step 8: On-device test** (this is the crash scenario the owner observed). Enable APRS and HamAlert so their tasks run.
  1. Open xOTA, wait for the spots to load, then leave. After about 2 seconds the log shows `[xOTA] Quiet period ended...` exactly **once**.
  2. Repeat 10 times **quickly**: enter xOTA and leave within 1 second. There must be no reboot, no `Guru Meditation`, no `Stack canary`, and no repeated `Quiet period ended` lines within the same 2 seconds.
  3. Enter xOTA, leave, and re-enter within 1 second. The log must **not** show `Quiet period ended` while you're on xOTA; the POTA fetch must complete.
  4. Stay on the dashboard for 5 minutes. APRS and HamAlert reconnect (each shows its own connected log line once, with no duplicates).

- [x] **Step 9: Commit**
```bash
git add src/ui/screens/xota.cpp
git commit -m "fix: resume services from main-loop timer after xOTA (no 2KB task, no re-entry race)"
```

---

### Task 6: Record the results in the review

**Files:**
- Modify: `docs/reviews/2026-10-code-review.md`

- [x] **Step 1:** Under items 3.1, 3.2 and 4.1, add a line `**Fixed:** <commit sha> (on-device verified YYYY-MM-DD)`, or `**Fixed, awaiting device test:** <sha>` if Steps 7–8 above haven't been done yet.

- [x] **Step 2: Commit**
```bash
git add docs/reviews/2026-10-code-review.md
git commit -m "docs: mark stability findings fixed"
```
