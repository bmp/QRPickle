#pragma once

// Try a host's addresses in order until one connects (v0.3.0). A host such as
// raw.githubusercontent.com resolves to several addresses, and one of them can be unreachable from
// a given network (2026-11-07: 185.199.109.133 timed out from the owner's ISP while the other three
// answered); using only the first address failed whenever DNS put that one first. try_one(i)
// connects to address i and returns true on success. Returns the index that connected, or -1.
// Host-safe (test/test_parsers).
namespace services {
    constexpr int MAX_HOST_ADDRS = 4;

    template <typename F>
    int connect_any(int n, F try_one) {
        for (int i = 0; i < n; i++)
            if (try_one(i)) return i;
        return -1;
    }
}  // namespace services
