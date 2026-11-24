#include "board_rules.h"
#include <stdio.h>
#include <string.h>

namespace hw {
    namespace rules {

        namespace {
            bool answered(uint32_t v) { return v != 0 && v != 0xFFFFFF && v != NO_ID; }

            bool id_matches(const BoardDef& b, const PanelIds& ids) {
                if (b.rddid != NO_ID && answered(ids.rddid) && b.rddid == ids.rddid) return true;
                if (b.rdid4 != NO_ID && (b.rdid4 == ids.rdid4_direct || b.rdid4 == ids.rdid4_indexed)) return true;
                return false;
            }

            bool probes_agree(const BoardDef& b, const Probes& p) {
                if (p.i2c_addr >= 0 && p.i2c_addr != (int16_t)b.t_addr) return false;
                if (p.xpt_own >= 0 && (p.xpt_own == 1) != (b.touch == TouchType::XPT2046Own)) return false;
                if (p.xpt_shared >= 0 && (p.xpt_shared == 1) != (b.touch == TouchType::XPT2046Shared)) return false;
                return true;
            }

            uint64_t bit(int8_t pin) { return pin >= 0 && pin < 64 ? (1ull << pin) : 0; }

            // The board named by a setting ("<id>" or "<id>!"), or -1.
            int find(const BoardDef* boards, size_t n, const char* setting, bool* anyway) {
                *anyway = false;
                if (!setting || !*setting || strcmp(setting, "auto") == 0) return -1;
                size_t len = strlen(setting);
                if (setting[len - 1] == '!') {
                    *anyway = true;
                    len--;
                }
                for (size_t i = 0; i < n; i++)
                    if (boards[i].complete && strlen(boards[i].id) == len && strncmp(boards[i].id, setting, len) == 0)
                        return (int)i;
                return -2;  // named a board that isn't (or no longer is) in the list
            }
        }  // namespace

        bool readable(const PanelIds& ids) {
            return answered(ids.rddid) || answered(ids.rdid4_direct) || answered(ids.rdid4_indexed);
        }

        const char* decision_name(Decision d) {
            switch (d) {
                case Decision::Detected: return "detected";
                case Decision::Chosen: return "chosen";
                case Decision::ChosenAnyway: return "chosen anyway";
                case Decision::Mismatch: return "mismatch";
                case Decision::Upgrade: return "kept from v0.3.1";
                case Decision::Ambiguous: return "ambiguous";
                default: return "unknown";
            }
        }

        size_t candidates(const BoardDef* boards, size_t n, const PanelIds& ids, const Probes& p, int8_t* out) {
            int8_t by_id[MAX_BOARDS];
            size_t m = 0;
            for (size_t i = 0; i < n && i < MAX_BOARDS; i++)
                if (boards[i].complete && id_matches(boards[i], ids)) by_id[m++] = (int8_t)i;
            size_t k = 0;
            for (size_t i = 0; i < m; i++)
                if (probes_agree(boards[by_id[i]], p)) out[k++] = by_id[i];
            if (k == 0) {  // a probe that rules out every candidate is not trusted
                memcpy(out, by_id, m);
                k = m;
            }
            return k;
        }

        Selection select(const BoardDef* boards, size_t n, const uint64_t (*conflict)[MAX_BOARDS],
                         const PanelIds& ids, const Probes& p, const char* setting, bool upgrade, int fallback) {
            Selection s;
            s.n_candidates = (uint8_t)candidates(boards, n, ids, p, s.candidates);
            bool anyway = false;
            int chosen = find(boards, n, setting, &anyway);
            if (chosen == -2) {
                s.setting_unknown = true;
                chosen = -1;
            }

            if (chosen >= 0) {
                bool in_candidates = false;
                for (size_t i = 0; i < s.n_candidates; i++) in_candidates |= s.candidates[i] == chosen;
                s.board = chosen;
                if (readable(ids) && s.n_candidates > 0 && !in_candidates) {
                    s.decision = anyway ? Decision::ChosenAnyway : Decision::Mismatch;
                    s.warn_mismatch = !anyway;
                    for (size_t i = 0; i < s.n_candidates; i++) s.pins_off |= conflict[s.candidates[i]][chosen];
                } else {
                    s.decision = Decision::Chosen;
                }
            } else if (s.n_candidates == 1) {
                s.decision = Decision::Detected;
                s.board = s.candidates[0];
            } else if (upgrade) {
                s.decision = Decision::Upgrade;  // v0.3.1 drove these pins on this board already
                s.board = fallback;
            } else {
                // Fresh install and not sure: drive only what every possible board agrees on.
                s.decision = s.n_candidates ? Decision::Ambiguous : Decision::Unknown;
                s.board = s.n_candidates ? s.candidates[0] : fallback;
                const BoardDef& b = boards[s.board];
                for (size_t i = 0; i < n; i++) {
                    bool in_pool = s.n_candidates == 0 ? boards[i].complete : false;
                    for (size_t c = 0; c < s.n_candidates; c++) in_pool |= s.candidates[c] == (int8_t)i;
                    if (!in_pool) continue;
                    s.pins_off |= conflict[i][s.board];
                    if (boards[i].bl_pin != b.bl_pin || boards[i].bl_active_high != b.bl_active_high)
                        s.backlight_on = false;
                }
                s.sensors_on = false;
                s.touch_on = false;
            }

            const BoardDef& b = boards[s.board];
            if (s.pins_off & bit(b.bl_pin)) s.backlight_on = false;
            uint64_t touch_pins = bit(b.t_cs) | bit(b.t_sclk) | bit(b.t_mosi) | bit(b.t_rst);
            if (b.touch == TouchType::GT911) touch_pins |= bit(b.t_irq);
            if (b.touch == TouchType::None || (s.pins_off & touch_pins)) s.touch_on = false;
            if (b.i2c_sda == NO_PIN || b.i2c_scl == NO_PIN || (s.pins_off & (bit(b.i2c_sda) | bit(b.i2c_scl))))
                s.sensors_on = false;
            s.ask_confirm = (s.decision == Decision::Detected || s.decision == Decision::Upgrade) && s.touch_on;
            return s;
        }

        const char* decision_code(Decision d) {
            switch (d) {
                case Decision::Detected: return "detected";
                case Decision::Chosen: return "chosen";
                case Decision::ChosenAnyway: return "anyway";
                case Decision::Mismatch: return "mismatch";
                case Decision::Upgrade: return "upgrade";
                case Decision::Ambiguous: return "ambiguous";
                default: return "unknown";
            }
        }

        size_t report_qr_url(char* buf, size_t len, const char* fw, const char* board_id, const PanelIds& ids,
                             Decision d, const Probes& p) {
            int n = snprintf(buf, len,
                             "https://github.com/bmp/QRPickle/issues/new?template=board_report.yml&fw=%s&board=%s"
                             "&ids=%06lX-%06lX-%06lX&decision=%s&touch=%d.%d.%d",
                             fw, board_id, (unsigned long)(ids.rddid & 0xFFFFFF),
                             (unsigned long)(ids.rdid4_direct & 0xFFFFFF), (unsigned long)(ids.rdid4_indexed & 0xFFFFFF),
                             decision_code(d), p.i2c_addr, p.xpt_own, p.xpt_shared);
            return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
        }

        bool map_touch(const BoardDef& b, int x, int y, int* ux, int* uy) {
            int u = x - b.ui_x, v = y - b.ui_y;
            if (u < 0 || v < 0 || u >= 320 || v >= 240) return false;
            *ux = u;
            *uy = v;
            return true;
        }

    }  // namespace rules
}  // namespace hw
