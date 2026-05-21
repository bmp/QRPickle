#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

// Initializes internal hardware clocks and configures core NTP server arrays
void timekeeper_init();

// Non-blocking engine task polled inside the main loop to process clock increments
void timekeeper_update();

// Thread-safe text query getters to pull live formatted time strings into the UI
const char * timekeeper_get_utc_string();
const char * timekeeper_get_local_string();

#endif // TIMEKEEPER_H
