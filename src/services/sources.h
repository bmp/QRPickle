#pragma once
#include <stdint.h>

// Every background data source and job, one row each: docs/DATA_FLOW.md's table is generated from
// this list (scripts/gen_data_flow.py) and scripts/check_data_flow.py fails the build when the doc
// and this file disagree. Change a refresh, retry, task or stack here, then regenerate the doc.
// The code uses the numbers below (job_interval_ms(), job_stack()), so they can't drift either.
//
// Fields: id, name, interval_ms (normal refresh; data counts as stale after 2x, fresh_registry.h;
// 0 = on request), task stack bytes (0 = runs on the main loop), connection, refresh, retry, task,
// kept in, screens. One X(...) row per line (the generator reads lines).
// clang-format off
#define QRP_JOBS(X) \
    X(WEATHER,   "weather",     7200000UL,  0,     "OpenWeather, HTTPS", "every 2 h; every 3 min within 45 min of sunrise or sunset", "5 min after a failure; 30 s if the network is busy", "main loop", "RAM (current, forecast)", "Weather, Dashboard") \
    X(SOLAR,     "solar",       10800000UL, 6144,  "hamqsl.com or own URL, HTTPS", "15 min after each 3-hourly update, at most hourly; not in the first minute after boot", "15 min; next tick if the network is busy", "solar_fetch", "RAM, copy kept over restarts", "Band conditions, Dashboard") \
    X(FEED,      "feed",        21600000UL, 6144,  "GitHub (raw), HTTPS", "4 min after boot, then every 6 h", "15 min; next tick if the network is busy", "space_fetch", "flash file /space.json", "Satellites") \
    X(POTA,      "pota",        300000UL,   6144,  "api.pota.app, HTTPS", "on an xOTA visit: opened by hand if older than 30 s, by cycling if older than 5 min", "next visit; waits while the network is busy", "pota_fetch", "RAM list (30), freed when no screen shows it", "xOTA (POTA tab)") \
    X(SOTA,      "sota",        3600000UL,  4096,  "SOTA cluster, telnet", "live while the SOTA tab is open; a cycling visit reconnects if older than 10 min", "30 s; silence 4 h; TCP keepalive", "sota_cluster", "RAM list (30), freed when no screen shows it", "xOTA (SOTA tab)") \
    X(DX,        "dx",          900000UL,   4096,  "DX cluster, telnet", "live while the Spots page is open or DX is cycled; read on the main loop", "30 s while the Spots page is open; silence 15 min; TCP keepalive", "dx_dial", "RAM list (50), freed when no screen shows it", "Spots") \
    X(HAMALERT,  "hamalert",    3600000UL,  4096,  "hamalert.org, telnet", "always connected", "30 s; silence 4 h; TCP keepalive", "hamalert_task", "RAM (10 alerts)", "HamAlert") \
    X(APRS,      "aprs",        900000UL,   4608,  "APRS-IS, TCP", "always connected when switched on", "5 s; silence 3 min; TCP keepalive", "aprs_task", "RAM (30 stations, 10 messages)", "APRS screens") \
    X(OTA_CHECK, "ota_check",   86400000UL, 9216,  "GitHub Pages, HTTPS (certificate checked)", "2 min after boot, then every 10 min until one succeeds; a manual check at once", "30 s if the network is busy", "gh_ota_check", "RAM (version, notes)", "Cloud OTA") \
    X(PASSES,    "passes",      1800000UL,  6144,  "none (SGP4 from the feed)", "every 30 min, and when the feed or the location changes", "next tick if the network is busy", "space_passes", "RAM", "Satellites, Dashboard") \
    X(OTA_INSTALL, "ota_install", 0UL,      12288, "GitHub Pages, HTTPS (certificate checked)", "on request (Cloud OTA screen or web console)", "none (the trial boot rolls back a bad image)", "ota_flash_worker", "the other app slot", "Cloud OTA")
// clang-format on

namespace services {
    namespace jobs {

        enum Job : uint8_t {
#define QRP_JOB_ID(id, ...) id,
            QRP_JOBS(QRP_JOB_ID)
#undef QRP_JOB_ID
                COUNT
        };

        struct Info {
            const char* name;
            uint32_t interval_ms;
            uint32_t stack;
        };

        constexpr Info INFO[COUNT] = {
#define QRP_JOB_INFO(id, name, interval, stack, ...) {name, interval, stack},
            QRP_JOBS(QRP_JOB_INFO)
#undef QRP_JOB_INFO
        };

        constexpr const char* job_name(Job j) { return INFO[j].name; }
        constexpr uint32_t job_interval_ms(Job j) { return INFO[j].interval_ms; }
        constexpr uint32_t job_stack(Job j) { return INFO[j].stack; }

    }  // namespace jobs
}  // namespace services
