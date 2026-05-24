#pragma once
#include <stdint.h>
#include <stddef.h>

namespace services {

    struct HamAlertMessage {
        char time[8];
        char callsign[16];
        char freq[12];
        char mode[12];
        char type[12]; // POTA, SOTA, DXCC, etc.
        char info[36]; // Park/Summit references or descriptions
    };

    class HamAlertManager {
    public:
        static void start();
        static void stop();
        static bool is_connected();
        static bool is_dirty();
        static void clear_dirty();
        
        static const HamAlertMessage* get_messages();
        static size_t get_message_count();

    private:
        static HamAlertMessage* messages;
        static size_t message_count;
        static bool connected;
        static bool dirty;
        static bool running;

        static void task_loop(void* param);
        static void process_line(char* line);
    };

} // namespace services