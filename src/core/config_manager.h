#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

// Bounding structure containing all user variables to record to flash memory
struct sys_config_t {
    char wifi_ssid[32];
    char wifi_secret[64];
    char station_call[12];
    float timezone_offset; // Upgrade from int to float to support half-hour zones
};

// Initializes the LittleFS partition and attempts to load an existing configuration file
void config_manager_init();

// Commits the current runtime settings structure directly to flash memory storage
bool config_manager_save();

// Read-only reference getters to query configurations system-wide safely
sys_config_t * config_get_runtime();

#endif // CONFIG_MANAGER_H
