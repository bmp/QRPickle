#include "aprs_manager.h"
#include "../config/config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <math.h>

namespace services {

    AprsStation* AprsManager::stations = nullptr; // Initialize to null
    size_t AprsManager::station_count = 0;
    bool AprsManager::connected = false;
    bool AprsManager::dirty = false;
    bool AprsManager::running = false;
    uint32_t AprsManager::last_beacon_time = 0;

    void AprsManager::start() {
        if (running || !config::get().aprs_enabled) return;
        
        // FIXED: Allocate the heavy 4.2KB array to the dynamic heap
        if (!stations) {
            stations = (AprsStation*)calloc(30, sizeof(AprsStation));
            if (!stations) {
                Serial.println("[APRS] CRITICAL: Failed to allocate heap memory!");
                return;
            }
        }

        running = true;
        xTaskCreate(task_loop, "aprs_task", 8192, NULL, 1, NULL);
    }

    void AprsManager::stop() {
        running = false;
        connected = false;
    }

    const AprsStation* AprsManager::get_stations() { return stations; }
    size_t AprsManager::get_station_count() { return station_count; }
    bool AprsManager::is_connected() { return connected; }
    bool AprsManager::is_dirty() { return dirty; }
    void AprsManager::clear_dirty() { dirty = false; }

    void AprsManager::trigger_manual_beacon() {
        last_beacon_time = 0; 
    }

    void AprsManager::calc_distance_bearing(float lat1, float lon1, float lat2, float lon2, float& out_dist, int& out_bearing) {
        float R = 6371.0f; 
        float dLat = (lat2 - lat1) * PI / 180.0f;
        float dLon = (lon2 - lon1) * PI / 180.0f;
        float rLat1 = lat1 * PI / 180.0f;
        float rLat2 = lat2 * PI / 180.0f;

        float a = sin(dLat/2) * sin(dLat/2) + cos(rLat1) * cos(rLat2) * sin(dLon/2) * sin(dLon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        out_dist = R * c;

        float y = sin(dLon) * cos(rLat2);
        float x = cos(rLat1) * sin(rLat2) - sin(rLat1) * cos(rLat2) * cos(dLon);
        float brg = atan2(y, x) * 180.0f / PI;
        out_bearing = ((int)brg + 360) % 360;
    }

    void AprsManager::get_cardinal(int bearing, char* out_str) {
        const char* dirs[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        strcpy(out_str, dirs[((bearing + 11) % 360) / 22]);
    }

    void AprsManager::convert_decimal_to_aprs(float lat, float lon, char* out_str) {
        char lat_dir = (lat >= 0) ? 'N' : 'S';
        char lon_dir = (lon >= 0) ? 'E' : 'W';
        
        lat = fabs(lat);
        lon = fabs(lon);
        
        int lat_deg = (int)lat;
        float lat_min = (lat - lat_deg) * 60.0f;
        
        int lon_deg = (int)lon;
        float lon_min = (lon - lon_deg) * 60.0f;
        
        snprintf(out_str, 24, "%02d%05.2f%c/%03d%05.2f%c", lat_deg, lat_min, lat_dir, lon_deg, lon_min, lon_dir);
    }

    void AprsManager::update_or_add_station(const char* call, float lat, float lon, const char* icon_char, const char* cmt) {
        auto& cfg = config::get();
        float dist; int brg;
        calc_distance_bearing(cfg.lat, cfg.lon, lat, lon, dist, brg);

        int target_idx = -1;
        for (size_t i = 0; i < station_count; i++) {
            if (strcmp(stations[i].callsign, call) == 0) {
                target_idx = i;
                break;
            }
        }

        if (target_idx == -1) {
            if (station_count < 30) {
                target_idx = station_count++;
            } else {
                uint32_t oldest = 0xFFFFFFFF;
                for (size_t i = 0; i < 30; i++) {
                    if (stations[i].last_seen_millis < oldest) {
                        oldest = stations[i].last_seen_millis;
                        target_idx = i;
                    }
                }
            }
        }

        if (target_idx != -1) {
            strncpy(stations[target_idx].callsign, call, sizeof(stations[0].callsign)-1);
            stations[target_idx].lat = lat;
            stations[target_idx].lon = lon;
            stations[target_idx].distance_km = dist;
            stations[target_idx].bearing_deg = brg;
            stations[target_idx].last_seen_millis = millis();
            
            if (icon_char[0] == '>') strncpy(stations[target_idx].type, "Car", 15);
            else if (icon_char[0] == '_') strncpy(stations[target_idx].type, "WX Station", 15);
            else if (icon_char[0] == '-') strncpy(stations[target_idx].type, "House/Fixed", 15);
            else if (icon_char[0] == '&') strncpy(stations[target_idx].type, "Gateway", 15);
            else strncpy(stations[target_idx].type, "Station", 15);

            if (cmt) {
                strncpy(stations[target_idx].comment, cmt, sizeof(stations[0].comment)-1);
            }
            dirty = true;
        }
    }

    void AprsManager::parse_uncompressed_position(const char* call, const char* info) {
        if (strlen(info) < 19) return;
        
        char lat_str[9] = {0};
        char lon_str[10] = {0};
        
        strncpy(lat_str, info + 1, 8); 
        strncpy(lon_str, info + 10, 9); 
        
        char lat_dir = lat_str[7];
        char lon_dir = lon_str[8];
        lat_str[7] = '\0';
        lon_str[8] = '\0';
        
        float lat_deg = (lat_str[0]-'0')*10 + (lat_str[1]-'0');
        float lat_min = atof(lat_str + 2);
        float dec_lat = lat_deg + (lat_min / 60.0f);
        if (lat_dir == 'S') dec_lat *= -1.0f;
        
        float lon_deg = (lon_str[0]-'0')*100 + (lon_str[1]-'0')*10 + (lon_str[2]-'0');
        float lon_min = atof(lon_str + 3);
        float dec_lon = lon_deg + (lon_min / 60.0f);
        if (lon_dir == 'W') dec_lon *= -1.0f;

        char symbol[2] = { info[19], '\0' };
        const char* cmt = (strlen(info) > 20) ? (info + 20) : "";
        
        update_or_add_station(call, dec_lat, dec_lon, symbol, cmt);
    }

    void AprsManager::process_line(char* line) {
        char* colon = strchr(line, ':');
        if (!colon) return;
        
        *colon = '\0'; 
        char* header = line;
        char* info = colon + 1;
        
        char* call_end = strchr(header, '>');
        if (!call_end) return;
        *call_end = '\0';
        char* callsign = header;

        if (info[0] == '!' || info[0] == '=' || info[0] == '@' || info[0] == '/') {
            const char* coord_start = info;
            if (info[0] == '@' || info[0] == '/') {
                if (strlen(info) > 8) coord_start = info + 7; 
            }
            parse_uncompressed_position(callsign, coord_start);
        }
    }

    void AprsManager::task_loop(void* param) {
        WiFiClient client;
        char buffer[256];
        size_t buf_idx = 0;

        while (running) {
            auto& cfg = config::get();
            if (!WiFi.isConnected() || strlen(cfg.aprs_passcode) == 0) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            if (!client.connected()) {
                connected = false;
                Serial.println("[APRS] Connecting to rotate.aprs.net:14580...");
                if (client.connect("rotate.aprs.net", 14580)) {
                    char login[128];
                    snprintf(login, sizeof(login), "user %s-%d pass %s vers QRPickle 1.0 filter r/%.2f/%.2f/50\r\n", 
                             cfg.callsign, cfg.aprs_ssid, cfg.aprs_passcode, cfg.lat, cfg.lon);
                    
                    client.write(login);
                    connected = true;
                    Serial.println("[APRS] Login dispatched. Socket stream open.");
                    last_beacon_time = 0; 
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                }
            }

            if (millis() - last_beacon_time > (30 * 60 * 1000) || last_beacon_time == 0) {
                char coord_str[24];
                convert_decimal_to_aprs(cfg.lat, cfg.lon, coord_str);
                
                char beacon[128];
                snprintf(beacon, sizeof(beacon), "%s-%d>APRS,TCPIP*:=%s-ESP32 QRPickle Dashboard Active\r\n", 
                         cfg.callsign, cfg.aprs_ssid, coord_str);
                
                client.write(beacon);
                Serial.print("[APRS] Position Beacon Transmitted: ");
                Serial.println(beacon);
                
                last_beacon_time = millis();
                if (last_beacon_time == 0) last_beacon_time = 1; 
            }

            while (client.available() && running) {
                char c = client.read();
                if (c == '\n') {
                    buffer[buf_idx] = '\0';
                    if (buf_idx > 0 && buffer[buf_idx - 1] == '\r') buffer[buf_idx - 1] = '\0';
                    
                    if (buffer[0] != '#') process_line(buffer);
                    buf_idx = 0;
                } else if (buf_idx < sizeof(buffer) - 1) {
                    buffer[buf_idx++] = c;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }

        client.stop();
        connected = false;
        vTaskDelete(NULL);
    }

} // namespace services