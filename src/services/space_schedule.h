#pragma once
#include <stdint.h>

namespace services {

    // Retry after a failed feed download: soon at first (a failure at boot is usually the TLS heap
    // during the boot rush), then every 15 minutes. failures counts consecutive failures (0 = 1).
    constexpr uint32_t space_retry_ms(uint8_t failures) {
        return failures <= 1 ? 2 * 60000u : failures == 2 ? 5 * 60000u
                                                          : 15 * 60000u;
    }

}  // namespace services
