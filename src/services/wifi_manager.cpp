#include "wifi_manager.h"
#include "../config/config.h" 
#include "web_server.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include <string.h>

#define AP_SSID "QRPickle-Setup"

static wifi_state_t current_state = WIFI_STATE_OFFLINE;
static unsigned long connection_timeout_mark = 0;
static unsigned long last_drop_mark = 0; 
static char status_msg[64] = "DISCONNECTED";

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

    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);

    current_state = WIFI_STATE_CONNECTING;
    connection_timeout_mark = millis();
    snprintf(status_msg, sizeof(status_msg), "CONNECTING TO %s...", cfg.wifi_ssid);
}

void wifi_manager_start_ap() {
    WiFi.disconnect(true, true); 
    delay(100);
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(AP_SSID)) {
        current_state = WIFI_STATE_AP_MODE;
        IPAddress ap_ip = WiFi.softAPIP();
        snprintf(status_msg, sizeof(status_msg), "AP ACTIVE | SSID: %s | IP: %d.%d.%d.%d",
                 AP_SSID, ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
        Serial.printf("[Wi-Fi] Hotspot Broadcast Up! %s\n", status_msg);
        web_server_init();
    } else {
        snprintf(status_msg, sizeof(status_msg), "HOTSPOT INITIALIZATION FAULT");
    }
}

void wifi_manager_update() {
    static unsigned long last_poll = 0;
    if (millis() - last_poll < 1000) return; // Only poll once a second
    last_poll = millis();

    if (current_state == WIFI_STATE_AP_MODE) return;

    if (WiFi.status() == WL_CONNECTED) {
        if (current_state != WIFI_STATE_CONNECTED) {
            current_state = WIFI_STATE_CONNECTED;
            IPAddress ip = WiFi.localIP();
            snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("[Wi-Fi] Network Link Stable! %s\n", status_msg);
        }
        last_drop_mark = millis(); // Reset the watchdog timer while connected
    } else {
        if (current_state == WIFI_STATE_CONNECTED) {
            current_state = WIFI_STATE_BACKGROUND_RETRY;
            snprintf(status_msg, sizeof(status_msg), "LINK LOST. WAITING FOR OS RECONNECT...");
            Serial.println("[Wi-Fi] Link dropped. Trusting ESP-IDF native auto-reconnect...");
            last_drop_mark = millis();
        }

        // WATCHDOG: If the OS fails to reconnect for 30 straight seconds, force a hard reset
        if (current_state == WIFI_STATE_BACKGROUND_RETRY && (millis() - last_drop_mark > 30000)) {
            Serial.println("[Wi-Fi] Watchdog timeout. Native reconnect failed. Forcing hardware radio reset...");
            WiFi.disconnect(true, true);
            delay(500);
            WiFi.begin(config::get().wifi_ssid, config::get().wifi_password);
            last_drop_mark = millis();
        }

        if (current_state == WIFI_STATE_CONNECTING && (millis() - connection_timeout_mark > 20000)) {
            Serial.println("[Wi-Fi] Boot connection timeout. Dropping back to Fallback Setup AP Mode...");
            wifi_manager_start_ap();
        }
    }
}

bool wifi_manager_is_connected() { return (current_state == WIFI_STATE_CONNECTED); }
const char* wifi_manager_get_status_text() { return status_msg; }
