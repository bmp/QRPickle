#include "wifi_manager.h"
#include <WiFi.h>
#include <Arduino.h>

// PLACEHOLDERS: Change these to match your local router parameters
#define WIFI_SSID     "YOUR_WIFI_SSID_PLACEHOLDER"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD_PLACEHOLDER"

static wifi_state_t current_state = WIFI_STATE_OFFLINE;
static unsigned long connection_timeout_mark = 0;
static char status_msg[64] = "DISCONNECTED";

void wifi_manager_init() {
    Serial.println("[Wi-Fi] Initializing network subsystem...");
    WiFi.mode(WIFI_STA);

    // Begin background association request
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    current_state = WIFI_STATE_CONNECTING;
    connection_timeout_mark = millis();
    snprintf(status_msg, sizeof(status_msg), "CONNECTING TO NETWORK...");
}

void wifi_manager_update() {
    // Coarse 500ms execution cadence limit to save CPU cycle overhead
    static unsigned long last_poll = 0;
    if (millis() - last_poll < 500) return;
    last_poll = millis();

    switch (current_state) {
        case WIFI_STATE_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                current_state = WIFI_STATE_CONNECTED;
                IPAddress ip = WiFi.localIP();
                snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("[Wi-Fi] Association successful! IP assigned: %s\n", status_msg);
            }
            // Enforce a strict 15-second timeout window to prevent infinite hanging
            else if (millis() - connection_timeout_mark > 15000) {
                current_state = WIFI_STATE_FAILED;
                WiFi.disconnect();
                snprintf(status_msg, sizeof(status_msg), "CONNECTION TIMEOUT");
                Serial.println("[Wi-Fi] Association timeout hit.");
            }
            break;

        case WIFI_STATE_CONNECTED:
            // Continuous verification link check
            if (WiFi.status() != WL_CONNECTED) {
                current_state = WIFI_STATE_OFFLINE;
                snprintf(status_msg, sizeof(status_msg), "LINK LOST. RECONNECTING...");
                Serial.println("[Wi-Fi] Physical connection link lost. Retrying...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                current_state = WIFI_STATE_CONNECTING;
                connection_timeout_mark = millis();
            }
            break;

        case WIFI_STATE_FAILED:
            // Auto-retry cadence loop after failure state cooldowns
            static unsigned long retry_mark = 0;
            if (retry_mark == 0) retry_mark = millis();
            if (millis() - retry_mark > 10000) { // Retry after 10s
                retry_mark = 0;
                Serial.println("[Wi-Fi] Executing scheduled network connection retry...");
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
                current_state = WIFI_STATE_CONNECTING;
                connection_timeout_mark = millis();
            }
            break;

        default:
            break;
    }
}

bool wifi_manager_is_connected() {
    return (current_state == WIFI_STATE_CONNECTED);
}

const char * wifi_manager_get_status_text() {
    return status_msg;
}
