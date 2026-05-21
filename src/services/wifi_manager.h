#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// Structural tracking of our radio connection state machine
enum wifi_state_t {
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
    WIFI_STATE_AP_MODE // Added tracking for active local hotspot state
};

// Configures the ESP32 network stack and launches an async connection attempt
void wifi_manager_init();

// Non-blocking background state worker polled inside the main loop
void wifi_manager_update();

// Forces the radio interface to collapse client connections and host its own hotspot bubble
void wifi_manager_start_ap();

// Thread-safe queries to pull connection status and IP data for our UI views
bool wifi_manager_is_connected();
const char * wifi_manager_get_status_text();

#endif // WIFI_MANAGER_H
