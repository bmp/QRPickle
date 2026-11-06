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

}  // namespace mem
