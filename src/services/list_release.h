#pragma once

// When a spot list (POTA, SOTA, DX; 3.4-3.8 KB each) may be freed. They used to stay allocated
// once their screen had been opened (~11 KB). Free only when the list exists, no task writes it
// (fetch, cluster socket, dial), no screen shows it, and screen cycling won't come straight back to
// it (re-fetching every round would load the servers). Host-safe (test/test_parsers).
namespace services {
    inline bool may_release_list(bool allocated, bool writer_active, bool shown, bool cycle_keeps) {
        return allocated && !writer_active && !shown && !cycle_keeps;
    }
}  // namespace services
