#include "wifi_manager.h"
#include "web_server.h" // Allow connection state adjustments to control server lifecycles
#include "../core/config_manager.h" // Include our active flash memory data definitions
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

#define AP_SSID "FoxClock-Setup"

static wifi_state_t current_state = WIFI_STATE_OFFLINE;
static unsigned long connection_timeout_mark = 0;
static char status_msg[64] = "DISCONNECTED";

void wifi_manager_init() {
    Serial.println("[Wi-Fi] Accessing storage data for network parameters...");

    // Fetch the loaded configuration struct directly from flash memory runtime pointers
    sys_config_t * cfg = config_get_runtime();

    // Check if the flash still contains the unconfigured factory placeholder token
    if (strcmp(cfg->wifi_ssid, "YOUR_WIFI_SSID_PLACEHOLDER") == 0 || strlen(cfg->wifi_ssid) == 0) {
        Serial.println("[Wi-Fi] Flash memory lacks configured network link. Launching setup AP...");
        wifi_manager_start_ap();
        return;
    }

    WiFi.mode(WIFI_STA);
    // Connect using strings loaded straight out of internal flash silicon!
    WiFi.begin(cfg->wifi_ssid, cfg->wifi_secret);

    current_state = WIFI_STATE_CONNECTING;
    connection_timeout_mark = millis();
    snprintf(status_msg, sizeof(status_msg), "CONNECTING TO %s...", cfg->wifi_ssid);
}

void wifi_manager_start_ap() {
  Serial.println("[Wi-Fi] Launching localized Access Point hotspot...");
  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(AP_SSID)) {
    current_state = WIFI_STATE_AP_MODE;
    IPAddress ap_ip = WiFi.softAPIP();
    snprintf(status_msg, sizeof(status_msg), "AP MODE ACTIVE | SSID: %s | IP: %d.%d.%d.%d",
             AP_SSID, ap_ip[0], ap_ip[1], ap_ip[2], ap_ip[3]);
    Serial.printf("[Wi-Fi] Hotspot Broadcast Up! Target string: %s\n", status_msg);

    // THE UPGRADE: Spin up our web configuration listener engine now!
    web_server_init();
  } else {
    snprintf(status_msg, sizeof(status_msg), "HOTSPOT INITIALIZATION FAULT");
  }
}

void wifi_manager_update() {
    static unsigned long last_poll = 0;
    if (millis() - last_poll < 500) return;
    last_poll = millis();

    sys_config_t * cfg = config_get_runtime();

    switch (current_state) {
        case WIFI_STATE_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                current_state = WIFI_STATE_CONNECTED;
                IPAddress ip = WiFi.localIP();
                snprintf(status_msg, sizeof(status_msg), "CONNECTED | IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("[Wi-Fi] Association successful! IP assigned: %s\n", status_msg);
            }
            else if (millis() - connection_timeout_mark > 15000) {
                Serial.println("[Wi-Fi] Station link handshake timeout. Shifting to fallback AP Mode...");
                wifi_manager_start_ap();
            }
            break;

        case WIFI_STATE_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                current_state = WIFI_STATE_OFFLINE;
                snprintf(status_msg, sizeof(status_msg), "LINK LOST. RECONNECTING...");
                WiFi.mode(WIFI_STA);
                WiFi.begin(cfg->wifi_ssid, cfg->wifi_secret);
                current_state = WIFI_STATE_CONNECTING;
                connection_timeout_mark = millis();
            }
            break;

        case WIFI_STATE_AP_MODE:
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
