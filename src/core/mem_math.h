#pragma once
#include <stdint.h>

// Heap figures (host-safe; test/test_parsers).
namespace mem {

    // Fragmentation: the share of free memory that is NOT in the largest block (0 = one block).
    // 0 when nothing is free; a largest block sampled above free (two calls apart) counts as 0.
    inline uint8_t frag_pct(uint32_t free_b, uint32_t largest) {
        if (!free_b || largest >= free_b) return 0;
        return (uint8_t)((uint64_t)(free_b - largest) * 100 / free_b);  // rounded down
    }

    // Whether the web server may answer a request: below these limits building any response can fail,
    // and a failed `new` aborts the device (no exceptions; crash decoded 2026-11-06). The request is
    // closed instead; the browser asks again.
    constexpr uint32_t WEB_MIN_FREE = 12288, WEB_MIN_BLOCK = 6144;
    inline bool web_can_serve(uint32_t free_b, uint32_t largest) {
        return free_b >= WEB_MIN_FREE && largest >= WEB_MIN_BLOCK;
    }

}  // namespace mem
