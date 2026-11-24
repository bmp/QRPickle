#include "board.h"
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "boards_gen.h"
#include "../config/config.h"
#include "../core/metadata.h"

static_assert(hw::rules::MAX_BOARDS == hw::BOARD_MAX, "board_rules.h and boards_gen.h disagree on the list size");

namespace hw {
    namespace board {

        namespace {
            rules::Selection sel;
            rules::PanelIds panel{0, 0, 0};
            rules::Probes probe;

            // Screen bus: identical on every listed board (scripts/check_boards.py enforces it).
            const BoardDef& any() { return BOARDS[0]; }

            uint8_t xfer(SPIClass& spi, uint8_t v) { return spi.transfer(v); }

            void screen_cmd(SPIClass& spi, uint8_t c) {
                digitalWrite(any().dc, LOW);
                xfer(spi, c);
                digitalWrite(any().dc, HIGH);
            }

            // Command, then four bytes back (the caller drops the dummy bit or byte).
            uint32_t screen_read32(SPIClass& spi, uint8_t c) {
                spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
                digitalWrite(any().cs, LOW);
                screen_cmd(spi, c);
                uint32_t v = 0;
                for (int i = 0; i < 4; i++) v = (v << 8) | xfer(spi, 0x00);
                digitalWrite(any().cs, HIGH);
                spi.endTransaction();
                return v;
            }

            // ILI9341: register c byte i through the 0xD9 index (as TFT_eSPI's readcommand8).
            uint8_t screen_read_indexed(SPIClass& spi, uint8_t c, uint8_t i) {
                spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
                digitalWrite(any().cs, LOW);
                screen_cmd(spi, 0xD9);
                xfer(spi, 0x10 + i);
                screen_cmd(spi, c);
                uint8_t v = xfer(spi, 0x00);
                digitalWrite(any().cs, HIGH);
                spi.endTransaction();
                return v;
            }

            void read_screen_ids() {
                const BoardDef& b = any();
                pinMode(b.cs, OUTPUT);
                pinMode(b.dc, OUTPUT);
                digitalWrite(b.cs, HIGH);
                SPIClass spi(VSPI);
                spi.begin(b.sclk, b.miso, b.mosi, -1);
                spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));  // software reset
                digitalWrite(b.cs, LOW);
                screen_cmd(spi, 0x01);
                digitalWrite(b.cs, HIGH);
                spi.endTransaction();
                delay(150);
                panel.rddid = (screen_read32(spi, 0x04) >> 7) & 0xFFFFFF;  // one dummy clock, 24 bits
                panel.rdid4_direct = screen_read32(spi, 0xD3) & 0xFFFFFF;  // one dummy byte, 24 bits
                panel.rdid4_indexed = (uint32_t)screen_read_indexed(spi, 0xD3, 1) << 16 |
                                      (uint32_t)screen_read_indexed(spi, 0xD3, 2) << 8 | screen_read_indexed(spi, 0xD3, 3);
                spi.end();
            }

            uint64_t chip_outputs_of(const int8_t* cand, size_t n) {
                uint64_t m = 0;
                for (size_t i = 0; i < n; i++) m |= BOARDS[cand[i]].chip_outputs;
                return m;
            }

            bool safe(int8_t pin, uint64_t chips) { return pin >= 0 && !(chips & (1ull << pin)); }

            // XPT2046 internal temperature (TEMP0, 12 bit): a chip that is there answers a plausible
            // value twice in a row; an empty bus reads 0, 4095 or noise.
            bool xpt_answers(SPIClass& spi, int8_t cs) {
                int v[2];
                for (int& x : v) {
                    spi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
                    digitalWrite(cs, LOW);
                    spi.transfer(0x84 | 0x03);  // start, TEMP0, 12-bit, single-ended, keep the ADC on
                    uint16_t r = spi.transfer16(0);
                    digitalWrite(cs, HIGH);
                    spi.endTransaction();
                    x = r >> 3;
                    delay(2);
                }
                return v[0] > 200 && v[0] < 3800 && abs(v[0] - v[1]) < 150;
            }

            // Only when several boards are possible, and only on pins no candidate's chips drive.
            void run_probes() {
                const uint64_t chips = chip_outputs_of(sel.candidates, sel.n_candidates);
                const BoardDef* i2c_board = nullptr;
                const BoardDef* own = nullptr;
                const BoardDef* shared = nullptr;
                for (size_t i = 0; i < sel.n_candidates; i++) {
                    const BoardDef& b = BOARDS[sel.candidates[i]];
                    if (b.t_addr && !i2c_board) i2c_board = &b;
                    if (b.touch == TouchType::XPT2046Own && !own) own = &b;
                    if (b.touch == TouchType::XPT2046Shared && !shared) shared = &b;
                }
                if (i2c_board && safe(i2c_board->t_sda, chips) && safe(i2c_board->t_scl, chips)) {
                    Wire.begin(i2c_board->t_sda, i2c_board->t_scl, 100000);
                    probe.i2c_addr = 0;
                    for (uint8_t a : {(uint8_t)0x5D, (uint8_t)0x14, (uint8_t)0x15}) {
                        Wire.beginTransmission(a);
                        if (Wire.endTransmission() == 0) {
                            // GT911 answers at 0x5D or 0x14 depending on its reset: same chip.
                            probe.i2c_addr = (a == 0x14) ? 0x5D : a;
                            break;
                        }
                    }
                    Wire.end();
                }
                if (own && safe(own->t_sclk, chips) && safe(own->t_mosi, chips) && safe(own->t_cs, chips)) {
                    pinMode(own->t_cs, OUTPUT);
                    digitalWrite(own->t_cs, HIGH);
                    SPIClass spi(HSPI);
                    spi.begin(own->t_sclk, own->t_miso, own->t_mosi, -1);
                    probe.xpt_own = xpt_answers(spi, own->t_cs) ? 1 : 0;
                    spi.end();
                    pinMode(own->t_cs, INPUT);
                }
                if (shared && safe(shared->t_cs, chips)) {
                    const BoardDef& s = *shared;
                    pinMode(s.t_cs, OUTPUT);
                    digitalWrite(s.t_cs, HIGH);
                    pinMode(s.cs, OUTPUT);
                    digitalWrite(s.cs, HIGH);  // keep the screen deselected
                    SPIClass spi(VSPI);
                    spi.begin(s.sclk, s.miso, s.mosi, -1);
                    probe.xpt_shared = xpt_answers(spi, s.t_cs) ? 1 : 0;
                    spi.end();
                    pinMode(s.t_cs, INPUT);
                }
            }

            rules::Selection decide() {
                const config::Config& c = config::get();
                return rules::select(BOARDS, BOARD_COUNT, BOARD_CONFLICT, panel, probe, c.board, config::is_upgrade(),
                                     BOARD_FALLBACK);
            }
        }  // namespace

        void select_at_boot() {
            read_screen_ids();
            sel = decide();
            const bool undecided = sel.decision == rules::Decision::Ambiguous || sel.decision == rules::Decision::Upgrade;
            if (sel.n_candidates > 1 && undecided) {
                run_probes();
                sel = decide();
            }
            if (sel.decision == rules::Decision::Upgrade) {
                // Ask once: from now on the setting names the board v0.3.1 drove.
                strlcpy(config::mutable_get().board, BOARDS[sel.board].id, sizeof(config::Config::board));
                config::save();
            }
            Serial.printf("[BOARD] %s (%s) rddid %06lX rdid4 %06lX/%06lX, probes i2c %d xpt %d/%d, setting %s%s\n",
                          BOARDS[sel.board].id, rules::decision_name(sel.decision), (unsigned long)panel.rddid,
                          (unsigned long)panel.rdid4_direct, (unsigned long)panel.rdid4_indexed, probe.i2c_addr,
                          probe.xpt_own, probe.xpt_shared, config::get().board,
                          sel.setting_unknown ? " (unknown: ignored)" : "");
            if (sel.pins_off)
                Serial.printf("[BOARD] pins kept off: 0x%llX%s%s%s\n", (unsigned long long)sel.pins_off,
                              sel.backlight_on ? "" : ", backlight", sel.touch_on ? "" : ", touch",
                              sel.sensors_on ? "" : ", sensors");
        }

        const BoardDef& active() { return BOARDS[sel.board]; }
        const rules::Selection& selection() { return sel; }
        const rules::PanelIds& ids() { return panel; }
        const rules::Probes& probes() { return probe; }
        bool pin_allowed(int8_t pin) { return pin >= 0 && !(sel.pins_off & (1ull << pin)); }

        void to_json(JsonObject out) {
            const BoardDef& b = active();
            char hex[8];
            out["id"] = b.id;
            out["model"] = b.model;
            out["name"] = b.name;
            out["status"] = b.status == BoardStatus::Tested ? "tested" : b.status == BoardStatus::Reported ? "reported" : "mapped";
            out["decision"] = rules::decision_name(sel.decision);
            out["setting"] = config::get().board;
            out["setting_unknown"] = sel.setting_unknown;
            out["warn_mismatch"] = sel.warn_mismatch;
            out["readable"] = rules::readable(panel);
            snprintf(hex, sizeof(hex), "%06lX", (unsigned long)panel.rddid);
            out["rddid"] = hex;
            snprintf(hex, sizeof(hex), "%06lX", (unsigned long)panel.rdid4_direct);
            out["rdid4"] = hex;
            snprintf(hex, sizeof(hex), "%06lX", (unsigned long)panel.rdid4_indexed);
            out["rdid4_indexed"] = hex;
            JsonObject p = out["probes"].to<JsonObject>();
            p["i2c"] = probe.i2c_addr;
            p["xpt_own"] = probe.xpt_own;
            p["xpt_shared"] = probe.xpt_shared;
            JsonArray cand = out["candidates"].to<JsonArray>();
            for (size_t i = 0; i < sel.n_candidates; i++) cand.add(BOARDS[sel.candidates[i]].id);
            out["backlight_on"] = sel.backlight_on;
            out["touch_on"] = sel.touch_on;
            out["sensors_on"] = sel.sensors_on;
            out["touch_note"] = b.touch_note;
            out["fw"] = meta::FW_VERSION;
        }

        size_t qr_url(char* buf, size_t len) {
            return rules::report_qr_url(buf, len, meta::FW_VERSION, active().id, panel, sel.decision, probe);
        }

    }  // namespace board
}  // namespace hw
