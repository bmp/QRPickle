#pragma once

// Parser for SOTA cluster (cluster.sota.org.uk:7300) spot lines. No Arduino
// dependencies, so it is unit-tested on the host (test/test_parsers).
namespace services {
namespace sota_cluster {

    struct ParsedSpot {
        char  spotter[16];
        char  activator[16];
        char  summit[16];    // e.g. "W1/DI-009"
        char  time[6];       // "HH:MM" (UTC), empty if absent
        char  comment[48];   // free text between summit and time, may be empty
        char  mode[8];       // from comment, else RBN => CW, else band-plan guess
        float freq_mhz;
    };

    // "DX de K1GC:      14054.0  K1GC         W1/DI-009                      1704Z"
    bool parse_line(const char* line, ParsedSpot& out);

    // ASSOC/RR-NNN, e.g. "W0C/SR-046", "G/LD-001"
    bool is_summit_ref(const char* s);

    // Best-effort guess; cluster lines carry no mode field.
    const char* mode_for_freq(float mhz);

}  // namespace sota_cluster
}  // namespace services
