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

    class AprsManager {
    public:
        static void start();
        static void stop();
        
        static const AprsStation* get_stations();
        static size_t get_station_count();
        static bool is_connected();
        static bool is_dirty();
        static void clear_dirty();

        static void trigger_manual_beacon();

    private:
        static AprsStation* stations; // FIXED: Moved to dynamic pointer
        static size_t station_count;
        static bool connected;
        static bool dirty;
        static bool running;
        static uint32_t last_beacon_time;

        static void task_loop(void* param);
        static void process_line(char* line);
        static void parse_uncompressed_position(const char* call, const char* info);
        static void update_or_add_station(const char* call, float lat, float lon, const char* icon_char, const char* cmt);
        
        static void calc_distance_bearing(float lat1, float lon1, float lat2, float lon2, float& out_dist, int& out_bearing);
        static void get_cardinal(int bearing, char* out_str);
        static void convert_decimal_to_aprs(float lat, float lon, char* out_str);
    };

} // namespace services