#pragma once
#include <stdint.h>

// Breadcrumbs that survive a watchdog/panic reset (RTC no-init memory). Each task records
// its last step with mark(); after an abnormal reset, report_previous() prints them so the
// hang location is visible even when the panic handler prints nothing. (Review 3.14)
namespace crashlog {

    enum Slot : uint8_t { SLOT_LOOP, SLOT_APRS, SLOT_HAMALERT, SLOT_GH_OTA, SLOT_LED, SLOT_SOTA, SLOT_NET, SLOT_COUNT };

    void mark(Slot slot, uint16_t step);

    // Call right after Serial.begin() in setup().
    void report_previous();

}  // namespace crashlog
