#include "hamalert_parse.h"
#include <ctype.h>
#include <string.h>

namespace services {
    namespace hamalert {

        static const char* skip_space(const char* p) {
            while (*p && isspace((unsigned char)*p)) p++;
            return p;
        }

        // Copies up to cap-1 characters while accept(c) and returns where it stopped (like a scanf
        // field width: a longer field continues into the next one); nullptr if nothing matched.
        template <typename Accept>
        static const char* take(const char* p, char* out, size_t cap, Accept accept) {
            size_t n = 0;
            while (*p && accept(*p) && n < cap - 1) out[n++] = *p++;
            out[n] = '\0';
            return n == 0 ? nullptr : p;
        }

        int split_spot(const char* line, SpotFields& out) {
            memset(&out, 0, sizeof(out));
            if (!line) return 0;
            const char* p = skip_space(line);
            if (strncmp(p, "DX", 2) != 0) return 0;
            p = skip_space(p + 2);
            if (strncmp(p, "de", 2) != 0) return 0;
            p = skip_space(p + 2);

            p = take(p, out.spotter, sizeof(out.spotter), [](char c) { return c != ':'; });
            if (!p || *p != ':') return p ? 1 : 0;
            p = skip_space(p + 1);
            auto not_space = [](char c) { return !isspace((unsigned char)c); };
            p = take(p, out.freq, sizeof(out.freq), not_space);
            if (!p) return 1;
            p = skip_space(p);
            p = take(p, out.call, sizeof(out.call), not_space);
            if (!p) return 2;
            p = skip_space(p);
            p = take(p, out.rest, sizeof(out.rest), [](char c) { return c != '\r' && c != '\n'; });
            return p ? 4 : 3;
        }

    }  // namespace hamalert
}  // namespace services
