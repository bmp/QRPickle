#pragma once
// Which board is fitted, from the screen's ID answers, touch-chip probes and the user's board setting.
// Host-safe (no Arduino): tested in test/test_boards. The device side (reading IDs, probing, NVS) is
// src/hw/board.cpp. Rules: docs/private/specs/2026-11-24-v0.3.2-board-support-design.md 5.4/5.5.
#include "board_def.h"

namespace hw {
    namespace rules {

        constexpr size_t MAX_BOARDS = 16;  // = BOARD_MAX in boards_gen.h (static_assert in board.cpp)

        // What the screen answered. A value of 0x000000 or 0xFFFFFF means no answer (data-out not
        // connected, or a chip that doesn't support the command).
        struct PanelIds {
            uint32_t rddid;  // 0x04, 24 bits after one dummy clock
            uint32_t rdid4_direct;  // 0xD3 read directly (ST7796)
            uint32_t rdid4_indexed;  // 0xD3 read through the 0xD9 index (ILI9341)
        };
        bool readable(const PanelIds& ids);

        // Touch probes; -1 = not run. i2c_addr: 0 = no chip answered, else the address that did.
        struct Probes {
            int16_t i2c_addr = -1;
            int8_t xpt_own = -1;
            int8_t xpt_shared = -1;
        };

        enum class Decision : uint8_t {
            Detected,  // auto, exactly one board matched
            Chosen,  // the user's choice (screen agrees, or can't tell)
            ChosenAnyway,  // the user's choice although the screen disagrees ("<id>!")
            Mismatch,  // the user's choice, the screen disagrees, not accepted yet: warn
            Upgrade,  // auto, settings from before v0.3.2, not sure: drive the screen as v0.3.1 did
            Ambiguous,  // auto, several boards matched (fresh install): safe mode
            Unknown,  // auto, no board matched (fresh install): safe mode
        };
        const char* decision_name(Decision d);  // for people: "kept from v0.3.1"
        const char* decision_code(Decision d);  // one word, for links and the issue form: "upgrade"

        struct Selection {
            int board = 0;  // index into the board list; always valid
            Decision decision = Decision::Unknown;
            uint64_t pins_off = 0;  // GPIOs never driven (bit n = GPIO n)
            bool ask_confirm = false;  // "Detected X: check the number on the back" (touch boards only)
            bool warn_mismatch = false;  // the choice contradicts the screen and wasn't accepted
            bool backlight_on = true;
            bool sensors_on = true;  // module I2C (BME280, v0.4 add-ons)
            bool touch_on = true;
            bool setting_unknown = false;  // the stored setting named a board that isn't in the list
            uint8_t n_candidates = 0;
            int8_t candidates[MAX_BOARDS] = {};
        };

        // Boards (complete ones only) whose recorded IDs match, narrowed by the probes that ran (a probe
        // that would rule out every candidate is ignored). Returns how many were written to out.
        size_t candidates(const BoardDef* boards, size_t n, const PanelIds& ids, const Probes& p, int8_t* out);

        // setting: "auto", "<id>" or "<id>!". upgrade: config::is_upgrade(). fallback: BOARD_FALLBACK.
        Selection select(const BoardDef* boards, size_t n, const uint64_t (*conflict)[MAX_BOARDS],
                         const PanelIds& ids, const Probes& p, const char* setting, bool upgrade, int fallback);

        // The short report link for the QR code: only facts the board knows (never user text or settings).
        //   https://github.com/bmp/QRPickle/issues/new?template=board_report.yml&fw=<fw>&board=<id>
        //   &ids=<rddid>-<rdid4>-<rdid4 indexed>&decision=<decision>&touch=<i2c>.<own>.<shared>
        // Returns the length (0 if buf is too small).
        size_t report_qr_url(char* buf, size_t len, const char* fw, const char* board_id, const PanelIds& ids,
                             Decision d, const Probes& p);

        // Touch point on the panel (raw panel pixels, landscape) -> UI coordinates (320x240). False when
        // the point lies outside the UI area of a bigger panel.
        bool map_touch(const BoardDef& b, int x, int y, int* ux, int* uy);

    }  // namespace rules
}  // namespace hw
