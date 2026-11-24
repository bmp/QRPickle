#pragma once
// GT911 capacitive touch over I2C (ESP32-2432S032C, ESP32-3248S035C). Board status "mapped": written
// from the GT911 datasheet and the boards' pin data, not yet run on such a board.
#include <stdint.h>
#include "board_def.h"

namespace hw {
    namespace gt911 {
        // Resets the chip into address 0x5D (INT low while RST rises) on its own I2C controller (Wire1)
        // and checks it answers. False: no touch.
        bool begin(const BoardDef& b);
        // The first touch point in panel pixels (landscape). False when nothing is pressed.
        bool read(int16_t& px, int16_t& py);
    }  // namespace gt911
}  // namespace hw
