#pragma once
#include <stddef.h>

// HamAlert telnet spot lines ("DX de <spotter>: <freq> <call> <rest>"). Host-safe: unit-tested in
// test/test_parsers. Replaces an sscanf call (scanf adds ~10 KB to the firmware).
namespace services {
    namespace hamalert {

        struct SpotFields {
            char spotter[16];
            char freq[16];
            char call[16];
            char rest[80];  // comment and time, e.g. "FT8 -12 dB 1712Z"; may be empty
        };

        // Splits a spot line. Returns how many fields were read, like the sscanf it replaces
        // ("DX de %15[^:]: %15s %15s %79[^\r\n]"): 3 or 4 for a spot, fewer otherwise.
        int split_spot(const char* line, SpotFields& out);

    }  // namespace hamalert
}  // namespace services
