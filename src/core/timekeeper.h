#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <time.h>

// Initializes internal hardware clocks and configures core NTP server arrays
void timekeeper_init();

// Non-blocking engine task polled inside the main loop to process clock increments
void timekeeper_update();

// Thread-safe text query getters to pull live formatted time strings into the UI
const char * timekeeper_get_utc_string();
const char * timekeeper_get_local_string();

// Local time (UTC offset applied) as numbers; false before NTP. Format it with
// units::fmt_clock() so the 12-hour setting applies; don't cut the local string apart.
bool timekeeper_get_local_tm(struct tm* out);

// Returns true if the native lwIP engine has achieved a valid atomic lock with an NTP server
bool timekeeper_is_synced();

#endif // TIMEKEEPER_H
