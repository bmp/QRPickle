#pragma once

// Pure APRS helpers (no Arduino dependencies) so they are unit-tested on the host.
namespace services {
namespace aprs {

    // Uncompressed position after the type char: "!DDMM.mmN/DDDMM.mmW>" (info[0] = '!','=','/','@'
    // already skipped by the caller so that info points at the type char). Rejects compressed
    // (base91) positions, malformed digits and out-of-range values (review 3.6).
    // Spaces (position ambiguity) are read as 0.
    bool parse_uncompressed_latlon(const char* info, float& lat, float& lon, char& table, char& symbol);

    // True if an APRS message addressee ("VU3GLJ-7 ", space padded) is `mycall` itself or
    // `mycall-SSID`, case-insensitively (review 3.10: prefix match accepted "VU3GLJX").
    bool addressed_to(const char* addressee, const char* mycall);

}  // namespace aprs
}  // namespace services
