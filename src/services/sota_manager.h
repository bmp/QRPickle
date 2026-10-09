#ifndef SOTA_MANAGER_H
#define SOTA_MANAGER_H

#include <cstdint>
#include <cstddef>
#include "sota_cluster_parse.h"

namespace services {

    struct SotaSpot {
        char time[12];
        char summit[16];
        float freq;            // MHz
        char mode[12];
        char activator[16];
        char comment[64];
        bool is_qrp;
    };

    // SOTA spots from the SOTA cluster (cluster.sota.org.uk:7300, telnet). The connection
    // runs only while the xOTA SOTA tab is open; the cluster replays recent spots on login.
    class SotaManager {
    public:
        static void start();
        static void stop();
        // True once the background task has fully exited (stop() only requests the exit).
        static bool is_stopped();

        // Kept for the xOTA screen: starts the cluster connection if it isn't running.
        static void fetch_async() { start(); }

        static const SotaSpot* get_spots() { return spots; }
        static size_t get_spot_count() { return spot_count; }
        static bool is_fetching() { return fetching; }
        static bool is_dirty() { return dirty; }
        static void clear_dirty() { dirty = false; }
        static uint32_t get_last_fetch_time() { return last_fetch_time; }
        static void expire_timer() { last_fetch_time = 0; }

    private:
        static SotaSpot* spots;
        static size_t spot_count;
        static bool fetching;
        static bool dirty;
        static bool running;
        static uint32_t last_fetch_time;

        static void task_loop(void* param);
        static void store_spot(const sota_cluster::ParsedSpot& p);
    };

} // namespace services

#endif // SOTA_MANAGER_H
