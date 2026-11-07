#pragma once
#include <ArduinoJson.h>
#include "freshness.h"

// One freshness record per data source (freshness.h). Tasks call ok()/fail() where they log
// their result today; /api/status reports them ("fresh") and screens show the age.
namespace services {
    namespace fresh {

        enum Source : uint8_t { WEATHER,
                                SOLAR,
                                FEED,
                                POTA,
                                SOTA,
                                DX,
                                HAMALERT,
                                APRS,
                                OTA_CHECK,
                                COUNT };

        void ok(Source s);  // any task
        void fail(Source s);  // any task
        Freshness get(Source s);
        // Seconds since the last success, or UINT32_MAX when there was none.
        uint32_t age_s(Source s);
        FreshState state(Source s);
        // {"weather": {"age_s": 12, "fails": 0, "state": "FRESH"}, ...}
        void to_json(JsonObject out);

    }  // namespace fresh
}  // namespace services
