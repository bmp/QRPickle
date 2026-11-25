#pragma once
// The board this firmware runs on, chosen once at boot (before any board pin is driven) from the
// screen's ID answers, touch probes and the "board" setting. Rules: board_rules.h; list: boards_gen.h.
#include <ArduinoJson.h>
#include "board_def.h"
#include "board_rules.h"

namespace hw {
    namespace board {

        // Reads the screen IDs (and probes touch chips when several boards are possible), applies the
        // rules and logs one "[BOARD]" line. Call right after config::load(); sets "board" to the
        // fallback on an upgrade from v0.3.1 so the confirmation is asked only once.
        void select_at_boot();

        const BoardDef& active();
        const rules::Selection& selection();
        const rules::PanelIds& ids();
        const rules::Probes& probes();

        // true when a GPIO may be driven (not in selection().pins_off)
        bool pin_allowed(int8_t pin);

        // Detection result for /api/board, /api/status and Set up over USB (no secrets).
        void to_json(JsonObject out);

        // The report link for the QR code (short: automatic facts only). Returns the length written.
        size_t qr_url(char* buf, size_t len);

    }  // namespace board
}  // namespace hw
