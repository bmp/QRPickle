#pragma once
#include <stdint.h>
#include <stddef.h>

namespace services {

    struct AprsStation {
        char callsign[16];
        char type[16];
        float distance_km;
        int bearing_deg;
        char course_speed[24];
        char comment[64];
        uint32_t last_seen_millis;
        float lat;
        float lon;
    };

    struct AprsMessage {
        char from[16];
        char text[64];
    };

    class AprsManager {
    public:
        static void start();
        static void stop();
        
        static const AprsStation* get_stations();
        static size_t get_station_count();
        static bool is_connected();
        static bool is_dirty();
        static void clear_dirty();

        // Message Handling Endpoints
        static const AprsMessage* get_messages();
        static size_t get_message_count();
        static bool is_msg_dirty();
        static void clear_msg_dirty();
        
        // Thread-safe injection into the TX socket queue
        static void send_message(const char* target_callsign, const char* message_body);

        // Beacon Stats Endpoints
        static uint32_t get_tx_count() { return tx_count; }
        static uint32_t get_last_tx_time() { return last_tx_time; }
        static void get_current_payload(char* buf, size_t max_len);
        static void trigger_manual_beacon();
        
        // Sorting and Filtering Methods
        static void sort_stations(bool ascending);

    private:
        static AprsStation* stations;
        static size_t station_count;
        static AprsMessage* messages;
        static size_t message_count;
        
        static bool connected;
        static bool dirty;
        static bool msg_dirty;
        static bool running;
        
        static uint32_t tx_count;
        static uint32_t last_tx_time;

        static void task_loop(void* param);
        static void process_line(char* line);
        static void parse_uncompressed_position(const char* call, const char* info);
        static void parse_incoming_message(const char* call, const char* info);
        static void update_or_add_station(const char* call, float lat, float lon, const char* table_char, const char* symbol_char, const char* cmt);
        
        static void calc_distance_bearing(float lat1, float lon1, float lat2, float lon2, float& out_dist, int& out_bearing);
        static void get_cardinal(int bearing, char* out_str);
        static void convert_decimal_to_aprs(float lat, float lon, char* out_str, char& table, char& symbol);
    };

} // namespace services