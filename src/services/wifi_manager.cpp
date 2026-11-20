#include "wifi_retry.h"
#include "wifi_manager.h"
#include "../config/config.h" 
#include "web_server.h"
#include "../hw/led_rgb.h" // RGB LED controller inclusion
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include <string.h>
#include <atomic>

#define AP_SSID "QRPickle-Setup"

static wifi_state_t current_state = WIFI_STATE_OFFLINE;
static unsigned long connection_timeout_mark = 0;
static unsigned long last_drop_mark = 0; 
static char status_msg[64] = "DISCONNECTED";

// While the fallback setup AP is up, retry the saved network (wifi_retry.h: 15 s, after 5 min 60 s).
static unsigned long last_sta_retry_mark = 0;
static unsigned long fallback_since_mark = 0;

// Set by the WiFi event task, read by wifi_manager_update(): when the station last reported a
// disconnect, and whether it did since our last sta_begin() (wifi_retry.h: sta_retry_due).
static std::atomic<uint32_t> last_disconnect_ms{0};
static std::atomic<uint8_t> last_disconnect_reason{0};
static std::atomic<bool> disconnected_since_begin{false};

// Every connection attempt to the saved network starts here.
static void sta_begin() {
    disconnected_since_begin = false;
    WiFi.begin(config::get().wifi_ssid, config::get().wifi_password);
}

void wifi_manager_init() {
    Serial.println("[Wi-Fi] Querying NVS parameters for link parameters...");
    const auto& cfg = config::get();

    if (strcmp(cfg.wifi_ssid, "YOUR_WIFI_SSID_PLACEHOLDER") == 0 || strlen(cfg.wifi_ssid) == 0) {
        Serial.println("[Wi-Fi] NVS lacks configured network targets. Mounting AP standard...");
        wifi_manager_start_ap();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    // Let the ESP-IDF OS handle disconnects seamlessly on its own
    WiFi.setAutoReconnect(true);

    // THE MISSING PIECE: Protect the CYD from brownout crashes! (52 = 13 dBm)
    esp_wifi_set_max_tx_power(60);

    // Why a connect fails or drops (WIFI_REASON_*: e.g. 2 auth expired, 15 handshake timeout,
    // 201 no AP found), logged when it changes; many boots miss the first connect (TASKS.md).
    // Runs in the event task: Serial and a static only.
    // Runs in the event task: Serial and atomics only. Reason 8 (ASSOC_LEAVE) is our own disconnect.
    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            static uint8_t last = 0;
            const uint8_t r = info.wifi_sta_disconnected.reason;
            if (r != last) Serial.printf("[Wi-Fi] Disconnected, reason %u\n", (unsigned)r);
            last = r;
            if (r == WIFI_REASON_ASSOC_LEAVE) return;
            last_disconnect_reason = r;
            last_disconnect_ms = millis();
            disconnected_since_begin = true;
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    sta_begin();

    current_state = WIFI_STATE_CONNECTING;
    connection_timeout_mark = millis();
    snprintf(status_msg, sizeof(status_msg), "CONNECTING TO %s...", cfg.wifi_ssid);

    // Set Stage 2 -> Solid Blue when network search is active
    hw::led_rgb::set_state(hw::led_rgb::STATE_BOOT_WIFI);
}

static bool bring_up_ap(wifi_mode_t mode) {
    WiFi.mode(mode);
    // WPA2 with the device admin password (review 1.2); it is shown on the Network screen.
    if (!WiFi.softAP(AP_SSID, config::get().admin_password)) {
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
        fallback_since_mark = last_sta_retry_mark;
    }
}

void wifi_manager_update() {
    static unsigned long last_poll = 0;
    if (millis() - last_poll < 1000) return; // Only poll once a second
    last_poll = millis();

    if (current_state == WIFI_STATE_AP_MODE) return;

    if (current_state == WIFI_STATE_AP_FALLBACK) {
        if (WiFi.status() != WL_CONNECTED) {
            if (millis() - last_sta_retry_mark > services::sta_retry_interval_ms(millis() - fallback_since_mark)) {
                Serial.println("[Wi-Fi] Fallback AP up. Retrying saved network...");
                sta_begin();
                last_sta_retry_mark = millis();
            }
            return;
        }
        Serial.println("[Wi-Fi] Saved network reachable again. Shutting down fallback AP...");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        // Fall through: the WL_CONNECTED branch below records the CONNECTED state.
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (current_state != WIFI_STATE_CONNECTED) {
            current_state = WIFI_STATE_CONNECTED;
            IPAddress ip = WiFi.localIP();
            snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("[Wi-Fi] Network Link Stable! %s\n", status_msg);
            
            // Network link restored, clear lost warning
            hw::led_rgb::set_state(hw::led_rgb::STATE_OFF);
        }
        last_drop_mark = millis(); // Reset the watchdog timer while connected
    } else {
        if (current_state == WIFI_STATE_CONNECTED) {
            current_state = WIFI_STATE_BACKGROUND_RETRY;
            snprintf(status_msg, sizeof(status_msg), "LINK LOST. WAITING FOR OS RECONNECT...");
            Serial.println("[Wi-Fi] Link dropped. Trusting ESP-IDF native auto-reconnect...");
            last_drop_mark = millis();
            
            // Set Post-Boot warning -> Breathing Red/Magenta connection lost indicator
            hw::led_rgb::set_state(hw::led_rgb::STATE_WIFI_LOST);
        }

        // The station gave up (e.g. AUTH_FAIL right after a reset; wifi_retry.h): try again now
        // instead of waiting for the boot timeout or the watchdog below.
        if ((current_state == WIFI_STATE_CONNECTING || current_state == WIFI_STATE_BACKGROUND_RETRY) &&
            services::sta_retry_due(false, disconnected_since_begin, millis(), last_disconnect_ms)) {
            Serial.printf("[Wi-Fi] Station stopped trying (reason %u); connecting again\n",
                          (unsigned)last_disconnect_reason.load());
            sta_begin();
        }

        // WATCHDOG: If the OS fails to reconnect for 30 straight seconds, force a hard reset
        if (current_state == WIFI_STATE_BACKGROUND_RETRY && (millis() - last_drop_mark > 30000)) {
            Serial.println("[Wi-Fi] Watchdog timeout. Native reconnect failed. Forcing hardware radio reset...");
            WiFi.disconnect(true, true);
            delay(500);
            sta_begin();
            last_drop_mark = millis();
        }

        if (current_state == WIFI_STATE_CONNECTING && (millis() - connection_timeout_mark > 20000)) {
            Serial.println("[Wi-Fi] Boot connection timeout. Starting fallback setup AP; will keep retrying saved network...");
            start_fallback_ap();
        }
    }
}

bool wifi_manager_is_connected() { return (current_state == WIFI_STATE_CONNECTED); }
bool wifi_manager_setup_hotspot() {
    return current_state == WIFI_STATE_AP_MODE || current_state == WIFI_STATE_AP_FALLBACK;
}
const char* wifi_manager_get_status_text() { return status_msg; }