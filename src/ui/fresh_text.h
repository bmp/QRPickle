#pragma once
#include <stdio.h>
#include "../services/fresh_registry.h"

// "updated 5 min ago" for a data source (services/fresh_registry.h), for the screens that show
// how old their data is. Returns true when the data is stale or failing (show it in the warning
// colour). Main loop only, like every UI module.
namespace ui {
    inline bool fresh_text(char* out, size_t n, services::fresh::Source s) {
        using services::FreshState;
        const uint32_t age = services::fresh::age_s(s);
        const FreshState st = services::fresh::state(s);
        if (age == UINT32_MAX) {
            snprintf(out, n, st == FreshState::FAILING ? "no data, retrying" : "no data yet");
            return st == FreshState::FAILING;
        }
        char a[16];
        services::age_text(a, sizeof(a), age);
        snprintf(out, n, "updated %s", a);
        return st == FreshState::STALE || st == FreshState::FAILING;
    }
}  // namespace ui
