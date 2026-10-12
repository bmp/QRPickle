#include "space_parse.h"
#include <string.h>

namespace services {
    namespace space {

        static uint8_t letter_category(char c) {
            switch (c) {
                case 'f': return CAT_FM;
                case 'l': return CAT_LINEAR;
                case 's': return CAT_SSTV;
                case 'v': return CAT_VOICE;
                case 'a': return CAT_APRS;
                default: return 0;
            }
        }

        uint8_t categories_from_letters(const char* letters) {
            uint8_t bits = 0;
            for (const char* p = letters; p && *p; p++) bits |= letter_category(*p);
            return bits;
        }

        static void copy(char* dst, size_t size, const char* src) {
            strncpy(dst, src ? src : "", size - 1);
            dst[size - 1] = '\0';
        }

        bool parse_sat(JsonObjectConst o, Sat& out) {
            memset(&out, 0, sizeof(out));
            out.norad = o["id"] | 0u;
            const char* t1 = o["t1"] | "";
            const char* t2 = o["t2"] | "";
            if (!out.norad || strlen(t1) != 69 || strlen(t2) != 69 || t1[0] != '1' || t2[0] != '2') return false;
            copy(out.name, sizeof(out.name), o["n"] | "?");
            copy(out.tle1, sizeof(out.tle1), t1);
            copy(out.tle2, sizeof(out.tle2), t2);
            out.categories = categories_from_letters(o["c"] | "");
            for (JsonObjectConst t : o["tx"].as<JsonArrayConst>()) {
                if (out.tx_count >= MAX_TX) break;
                Tx& x = out.tx[out.tx_count];
                x.down_hz = t["d"] | 0u;
                x.up_hz = t["u"] | 0u;
                copy(x.mode, sizeof(x.mode), t["m"] | "");
                x.category = letter_category((t["c"] | " ")[0]);
                if (x.down_hz) out.tx_count++;
            }
            return true;
        }

        bool parse_event(JsonObjectConst o, Event& out) {
            memset(&out, 0, sizeof(out));
            const char* k = o["k"] | "";
            if (strcmp(k, "contact") == 0) out.kind = EVENT_CONTACT;
            else if (strcmp(k, "sstv") == 0) out.kind = EVENT_SSTV;
            else return false;
            out.start = o["s"] | 0u;
            out.end = o["e"] | 0u;
            if (!out.start || out.end < out.start) return false;
            copy(out.title, sizeof(out.title), o["t"] | "");
            copy(out.detail, sizeof(out.detail), o["x"] | "");
            out.freq_hz = o["f"] | 0u;
            copy(out.mode, sizeof(out.mode), o["m"] | "");
            return true;
        }

        size_t insert_pass(Pass* list, size_t n, size_t cap, const Pass& p) {
            if (!cap || (n == cap && p.aos >= list[n - 1].aos)) return n;
            size_t i = n < cap ? n : cap - 1;  // when full the latest pass drops out
            for (; i > 0 && list[i - 1].aos > p.aos; i--) list[i] = list[i - 1];
            list[i] = p;
            return n < cap ? n + 1 : n;
        }

        void mark_event_passes(Pass* passes, size_t n, const SatInfo* sats, const Event* events, size_t n_events) {
            for (size_t i = 0; i < n; i++) {
                Pass& p = passes[i];
                p.flags = 0;
                if (sats[p.sat].norad != ISS) continue;
                for (size_t e = 0; e < n_events; e++) {
                    const Event& ev = events[e];
                    const bool overlaps = p.aos < ev.end && p.los > ev.start;
                    if (!overlaps) continue;
                    if (ev.kind == EVENT_SSTV) p.flags |= PASS_SSTV_EVENT;
                    if (ev.kind == EVENT_CONTACT) p.flags |= PASS_CONTACT;
                }
            }
        }

        Alert alert_state(const Pass* passes, size_t n, uint32_t now, uint32_t lead_s) {
            if (!lead_s) return Alert::NONE;
            Alert best = Alert::NONE;
            for (size_t i = 0; i < n; i++) {
                const Pass& p = passes[i];
                if (!p.flags) continue;
                if (now >= p.aos && now < p.los) return Alert::NOW;
                if (p.aos > now && p.aos - now <= lead_s) best = Alert::SOON;
            }
            return best;
        }

        const char* compass(uint16_t az) {
            static const char* const DIRS[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                                 "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
            return DIRS[((az % 360) * 16 + 180) / 360 % 16];
        }

    }  // namespace space
}  // namespace services
