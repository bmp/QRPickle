#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The space feed (tools/space_feed/build_feed.py) in compact records, and the logic around passes
// that doesn't need the SGP4 library. Host-safe: unit-tested in test/test_parsers.
namespace services {
    namespace space {

        // Category bits (config::sat_categories) and the feed's letters.
        enum Category : uint8_t { CAT_FM = 1,
                                  CAT_LINEAR = 2,
                                  CAT_SSTV = 4,
                                  CAT_VOICE = 8,
                                  CAT_APRS = 16 };
        constexpr uint8_t CAT_DEFAULT = CAT_FM | CAT_LINEAR | CAT_SSTV | CAT_VOICE;
        uint8_t categories_from_letters(const char* letters);  // "fsv" -> CAT_FM | CAT_SSTV | CAT_VOICE

        constexpr int MAX_SATS = 40, MAX_TX = 4, MAX_EVENTS = 8, MAX_PASSES = 48;
        constexpr uint32_t ISS = 25544;

        struct Tx {
            uint32_t down_hz, up_hz;  // 0 = none
            char mode[8];
            uint8_t category;  // CAT_*
        };
        struct Sat {
            uint32_t norad;
            char name[16];
            uint8_t categories;  // CAT_* bits
            char tle1[70], tle2[70];
            Tx tx[MAX_TX];
            uint8_t tx_count;
        };
        // What stays in RAM per satellite; the orbits and frequencies stay in the cached feed file.
        struct SatInfo {
            uint32_t norad;
            char name[16];
            uint8_t categories;
        };
        enum EventKind : uint8_t { EVENT_CONTACT,
                                   EVENT_SSTV };
        struct Event {
            EventKind kind;
            uint32_t start, end;  // UTC
            char title[48];
            char detail[72];
            uint32_t freq_hz;  // 0 = not stated
            char mode[12];
            bool audible_here;  // contact: the ISS is above the horizon at contact time (set on device)
        };
        // Flags of a pass.
        enum PassFlag : uint8_t { PASS_SSTV_EVENT = 1,
                                  PASS_CONTACT = 2 };
        struct Pass {
            uint8_t sat;  // index into the satellite list
            uint32_t aos, tca, los;  // UTC: rise, highest point, set
            uint8_t max_el;  // degrees
            uint16_t az_aos, az_los;  // degrees
            uint8_t flags;  // PASS_*
        };

        // One element of the feed's "sats" / "events" arrays. False if it isn't usable.
        bool parse_sat(JsonObjectConst o, Sat& out);
        bool parse_event(JsonObjectConst o, Event& out);

        // Adds p to a list sorted by rise time that keeps the earliest cap passes; returns the new size.
        size_t insert_pass(Pass* list, size_t n, size_t cap, const Pass& p);

        // Marks ISS passes that fall in an SSTV event window or carry a school contact.
        void mark_event_passes(Pass* passes, size_t n, const SatInfo* sats, const Event* events, size_t n_events);

        // LED pass alert (docs/LEDColours.md): before an event pass within lead_s, during it, or none.
        enum class Alert : uint8_t { NONE,
                                     SOON,
                                     NOW };
        Alert alert_state(const Pass* passes, size_t n, uint32_t now, uint32_t lead_s);

        // Finds "key" in a JSON text read one character at a time: the quoted key, then optional
        // whitespace, ':' and, unless open is ':', optional whitespace and open (e.g. '['). feed()
        // returns true on the character that completes it, so a stream stands just after it (like
        // Stream::find("\"sats\":[") did, which missed a hand-made feed with "sats": [).
        struct KeyScan {
            KeyScan(const char* k, char o) : key(k), len(strlen(k)), open(o) {}
            bool feed(char c) {
                const bool space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
                if (state == 1) {  // after the key: wait for ':'
                    if (space) return false;
                    if (c == ':') {
                        if (open == ':') return done();
                        state = 2;
                        return false;
                    }
                } else if (state == 2) {  // after ':': wait for open
                    if (space) return false;
                    if (c == open) return done();
                }
                state = 0;
                match(c);
                return false;
            }

        private:
            const char* key;
            size_t len;
            char open;
            size_t i = 0;  // characters of "key" (with its quotes) matched so far
            uint8_t state = 0;  // 0 key, 1 colon, 2 open
            bool done() {
                state = 0;
                i = 0;
                return true;
            }
            void match(char c) {
                const char want = (i == 0 || i == len + 1) ? '"' : key[i - 1];
                if (c == want) {
                    if (++i == len + 2) {
                        state = 1;
                        i = 0;
                    }
                } else {
                    i = c == '"' ? 1 : 0;  // a quote may start the key again
                }
            }
        };

        // "NW" for an azimuth in degrees (16-point compass).
        const char* compass(uint16_t az);

    }  // namespace space
}  // namespace services
