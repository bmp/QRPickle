#include "aprs_manager.h"
#include "../config/config.h"
#include "../hw/sensor.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <math.h>
#include <strings.h>
#include "prop_manager.h"

namespace services {

    AprsStation* AprsManager::stations = nullptr;
    size_t AprsManager::station_count = 0;
    AprsMessage* AprsManager::messages = nullptr;
    size_t AprsManager::message_count = 0;
    
    bool AprsManager::connected = false;
    bool AprsManager::dirty = false;
    bool AprsManager::msg_dirty = false;
    bool AprsManager::running = false;
    
    uint32_t AprsManager::tx_count = 0;
    uint32_t AprsManager::last_tx_time = 0;
    static uint32_t loop_last_beacon_millis = 0;

    // TX queue buffers shifted to the heap
    static char* tx_msg_queue = nullptr;
    static bool tx_msg_pending = false;

    void AprsManager::start() {
        if (running || !config::get().aprs_enabled) return;
        
        if (!stations) stations = (AprsStation*)calloc(30, sizeof(AprsStation));
        if (!messages) messages = (AprsMessage*)calloc(10, sizeof(AprsMessage));
        if (!tx_msg_queue) tx_msg_queue = (char*)calloc(160, sizeof(char));
        
        if (!stations || !messages || !tx_msg_queue) return;

        running = true;
        xTaskCreate(task_loop, "aprs_task", 10240, NULL, 1, NULL);
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

    const AprsMessage* AprsManager::get_messages() { return messages; }
    size_t AprsManager::get_message_count() { return message_count; }
    bool AprsManager::is_msg_dirty() { return msg_dirty; }
    void AprsManager::clear_msg_dirty() { msg_dirty = false; }

    void AprsManager::trigger_manual_beacon() {
        loop_last_beacon_millis = 0; 
    }

    // --- ENHANCED MESSAGE DISPATCHER WITH LOGGING ---
    void AprsManager::send_message(const char* target, const char* message) {
        Serial.printf("\n[APRS-UI] User requested to send message.\n");
        Serial.printf("  -> Target: '%s'\n", target);
        Serial.printf("  -> Payload: '%s'\n", message);

        auto& cfg = config::get();
        if (!connected) {
            Serial.println("[APRS-TX] ABORTED: Not connected to APRS-IS.");
            return;
        }
        if (!tx_msg_queue) {
            Serial.println("[APRS-TX] ABORTED: TX Queue memory not allocated.");
            return;
        }
        if (strlen(target) == 0 || strlen(message) == 0) {
            Serial.println("[APRS-TX] ABORTED: Target or Message is completely empty.");
            return;
        }

        // FIX: Properly format source callsign. APRS servers reject "CALLSIGN-0".
        char src_call[16];
        if (cfg.aprs_ssid == 0) {
            snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
        } else {
            snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);
        }

        // APRS spec REQUIRES the target callsign to be exactly 9 characters, right-padded with spaces
        char padded_target[10] = "         ";
        memcpy(padded_target, target, min(strlen(target), (size_t)9));

        // Format: SOURCE>PATH::TARGET   :MESSAGE
        snprintf(tx_msg_queue, 160, "%s>APRS,TCPIP*::%s:%s\r\n", src_call, padded_target, message);
        
        tx_msg_pending = true;
        Serial.printf("[APRS-TX] Queued for network stream: %s", tx_msg_queue);

        if (message_count < 10) {
            strncpy(messages[message_count].from, target, 15);
            snprintf(messages[message_count].text, 63, "[TX] %s", message);
            message_count++;
        } else {
            for(int i=0; i<9; i++) messages[i] = messages[i+1];
            strncpy(messages[9].from, target, 15);
            snprintf(messages[9].text, 63, "[TX] %s", message);
        }
        msg_dirty = true;
    }

    void AprsManager::sort_stations(bool ascending) {
        if (station_count < 2 || !stations) return;
        
        for (size_t i = 0; i < station_count - 1; i++) {
            for (size_t j = 0; j < station_count - i - 1; j++) {
                bool swap_needed = ascending ? 
                    (stations[j].distance_km > stations[j+1].distance_km) : 
                    (stations[j].distance_km < stations[j+1].distance_km);
                
                if (swap_needed) {
                    AprsStation temp = stations[j];
                    stations[j] = stations[j+1];
                    stations[j+1] = temp;
                }
            }
        }
        dirty = true;
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

    void AprsManager::convert_decimal_to_aprs(float lat, float lon, char* out_str, char& table, char& symbol) {
        auto& cfg = config::get();
        table = (strlen(cfg.aprs_icon) >= 2) ? cfg.aprs_icon[0] : '/';
        symbol = (strlen(cfg.aprs_icon) >= 2) ? cfg.aprs_icon[1] : '[';

        char lat_dir = (lat >= 0) ? 'N' : 'S';
        char lon_dir = (lon >= 0) ? 'E' : 'W';
        lat = fabs(lat); lon = fabs(lon);
        
        int lat_deg = (int)lat;
        float lat_min = (lat - lat_deg) * 60.0f;
        int lon_deg = (int)lon;
        float lon_min = (lon - lon_deg) * 60.0f;
        
        snprintf(out_str, 24, "%02d%05.2f%c%c%03d%05.2f%c%c", 
                 lat_deg, lat_min, lat_dir, table, lon_deg, lon_min, lon_dir, symbol);
    }

    void AprsManager::get_current_payload(char* buf, size_t max_len) {
        auto& cfg = config::get();
        float t = sensor_get_temp();
        float h = sensor_get_humidity();
        float p = sensor_get_pressure();
        
        snprintf(buf, max_len, "%s | T:%.1fC H:%.0f%% P:%.0fhPa", cfg.aprs_comment, t, h, p);
    }

    void AprsManager::update_or_add_station(const char* call, float lat, float lon, const char* table_char, const char* symbol_char, const char* cmt) {
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
            
            if (symbol_char[0] == '[' && table_char[0] == '/')      strncpy(stations[target_idx].type, "Runner", 15);
            else if (symbol_char[0] == '-' && table_char[0] == '/') strncpy(stations[target_idx].type, "Fixed Base", 15);
            else if (symbol_char[0] == '&' && table_char[0] == 'I') strncpy(stations[target_idx].type, "Gateway", 15);
            else if (symbol_char[0] == 'Y' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Radios/APRS", 15);
            else if (symbol_char[0] == ';' && table_char[0] == '/')  strncpy(stations[target_idx].type, "Portable", 15);
            else if (symbol_char[0] == 'F' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Field Day", 15);
            else if (symbol_char[0] == ';' && table_char[0] == '\\') strncpy(stations[target_idx].type, "Park/Picnic", 15);
            else strncpy(stations[target_idx].type, "Station", 15);

            if (cmt) {
                strncpy(stations[target_idx].comment, cmt, sizeof(stations[0].comment)-1);
                stations[target_idx].comment[sizeof(stations[0].comment)-1] = '\0';
                char* end = strchr(stations[target_idx].comment, '\r'); if(end) *end = '\0';
                end = strchr(stations[target_idx].comment, '\n'); if(end) *end = '\0';
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
        
        char table_char[2] = { info[9], '\0' };
        char symbol_char[2] = { info[18], '\0' };
        
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

        const char* cmt = (strlen(info) > 19) ? (info + 19) : "";
        update_or_add_station(call, dec_lat, dec_lon, table_char, symbol_char, cmt);
    }

    void AprsManager::parse_incoming_message(const char* call, const char* info) {
        auto& cfg = config::get();
        if (strlen(info) < 11 || info[0] != ':') return;

        char target_check[16];
        if (cfg.aprs_ssid == 0) snprintf(target_check, sizeof(target_check), "%s", cfg.callsign);
        else snprintf(target_check, sizeof(target_check), "%s-%d", cfg.callsign, cfg.aprs_ssid);
        
        char padded_target[10] = "         ";
        memcpy(padded_target, target_check, min(strlen(target_check), (size_t)9));

        if (memcmp(info + 1, padded_target, 9) == 0 && info[10] == ':') {
            const char* msg_body = info + 11;
            
            if (message_count < 10) {
                strncpy(messages[message_count].from, call, 15);
                strncpy(messages[message_count].text, msg_body, 63);
                message_count++;
            } else {
                for(int i=0; i<9; i++) messages[i] = messages[i+1];
                strncpy(messages[9].from, call, 15);
                strncpy(messages[9].text, msg_body, 63);
            }
            msg_dirty = true;
        }
    }

    void AprsManager::process_line(char* line) {
        services::PropagationManager::parse_cluster_line(line);

        char* colon = strchr(line, ':');
        if (!colon) return;
        
        *colon = '\0'; 
        char* header = line;
        char* info = colon + 1;
        
        char* call_end = strchr(header, '>');
        if (!call_end) return;
        *call_end = '\0';
        char* callsign = header;

        if (info[0] == ':') {
            parse_incoming_message(callsign, info);
        } else if (info[0] == '!' || info[0] == '=' || info[0] == '@' || info[0] == '/') {
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
                vTaskDelay(pdMS_TO_TICKS(4000));
                continue;
            }

            if (!client.connected()) {
                connected = false;
                if (client.connect("rotate.aprs.net", 14580)) {
                    char login[128];
                    char src_call[16];
                    if (cfg.aprs_ssid == 0) snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
                    else snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);

                    snprintf(login, sizeof(login), "user %s pass %s vers QRPickle 1.0 filter r/%.2f/%.2f/50\r\n", 
                             src_call, cfg.aprs_passcode, cfg.lat, cfg.lon);
                    client.print(login);
                    connected = true;
                    loop_last_beacon_millis = 0; 
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                }
            }

            if (millis() - loop_last_beacon_millis > (30 * 60 * 1000) || loop_last_beacon_millis == 0) {
                char coord_str[24]; char table_c; char symbol_c;
                convert_decimal_to_aprs(cfg.lat, cfg.lon, coord_str, table_c, symbol_c);
                
                char dynamic_cmt[70];
                get_current_payload(dynamic_cmt, sizeof(dynamic_cmt));

                char src_call[16];
                if (cfg.aprs_ssid == 0) snprintf(src_call, sizeof(src_call), "%s", cfg.callsign);
                else snprintf(src_call, sizeof(src_call), "%s-%d", cfg.callsign, cfg.aprs_ssid);

                char beacon[160];
                snprintf(beacon, sizeof(beacon), "%s>APRS,TCPIP*:=%s%s\r\n", 
                         src_call, coord_str, dynamic_cmt);
                
                client.print(beacon);
                tx_count++;
                last_tx_time = millis();
                loop_last_beacon_millis = millis();
                if (loop_last_beacon_millis == 0) loop_last_beacon_millis = 1;
            }

            // --- THE PHYSICAL NETWORK DISPATCH CHECK ---
            if (tx_msg_pending && connected) {
                client.print(tx_msg_queue);
                client.flush(); // FIX: Forcibly push the TCP packet out immediately
                Serial.printf("[APRS-TX] Hardware flushed packet to network: %s", tx_msg_queue);
                tx_msg_pending = false;
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
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        client.stop();
        connected = false;
        vTaskDelete(NULL);
    }
} // namespace services
