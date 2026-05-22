#include "wifi_manager.h"
#include "../config/config.h" 
#include "web_server.h"
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

#define AP_SSID "QRPickle-Setup"

static wifi_state_t current_state = WIFI_STATE_OFFLINE;
static unsigned long connection_timeout_mark = 0;
static unsigned long last_background_retry_mark = 0; // FIXED: Separate clock tracker
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
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);

    current_state = WIFI_STATE_CONNECTING;
    connection_timeout_mark = millis();
    snprintf(status_msg, sizeof(status_msg), "CONNECTING TO %s...", cfg.wifi_ssid);
}

void wifi_manager_start_ap() {
    WiFi.disconnect(true);
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
    if (millis() - last_poll < 500) return;
    last_poll = millis();

    const auto& cfg = config::get();

    switch (current_state) {
        case WIFI_STATE_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                current_state = WIFI_STATE_CONNECTED;
                IPAddress ip = WiFi.localIP();
                snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("[Wi-Fi] Association successful! IP: %s\n", status_msg);
            }
            else if (millis() - connection_timeout_mark > 20000) {
                // FIXED: Only falls back to AP during initial boot verification
                Serial.println("[Wi-Fi] Boot connection timeout. Dropping back to Fallback Setup AP Mode...");
                wifi_manager_start_ap();
            }
            break;

        case WIFI_STATE_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                // FIXED: Runtime link drops route to background retry instead of dropping to AP
                current_state = WIFI_STATE_BACKGROUND_RETRY;
                snprintf(status_msg, sizeof(status_msg), "LINK LOST. RETRYING...");
                Serial.println("[Wi-Fi] Connection lost. Initiating background recovery cycles.");
                last_background_retry_mark = millis();
                WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
            }
            break;

        case WIFI_STATE_BACKGROUND_RETRY:
            if (WiFi.status() == WL_CONNECTED) {
                current_state = WIFI_STATE_CONNECTED;
                IPAddress ip = WiFi.localIP();
                snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("[Wi-Fi] Background recovery successful! IP: %s\n", status_msg);
            } 
            else if (millis() - last_background_retry_mark >= 30000) {
                // FIXED: Issues non-blocking reconnection check command every 30 seconds
                last_background_retry_mark = millis();
                Serial.println("[Wi-Fi] Heartbeat retry: reconnecting radio interface...");
                WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
            }
            break;

        case WIFI_STATE_AP_MODE:
            break;

        default:
            break;
    }
}

bool wifi_manager_is_connected() { return (current_state == WIFI_STATE_CONNECTED); }
const char* wifi_manager_get_status_text() { return status_msg; }