#include "sota_cluster_parse.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace services {
namespace sota_cluster {

    static void copy_str(char* dst, size_t n, const char* src) {
        strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    }

    bool is_summit_ref(const char* s) {
        size_t len = strlen(s);
        if (len < 8 || len > 15) return false;
        const char* slash = strchr(s, '/');
        if (!slash || slash == s) return false;
        for (const char* p = s; p < slash; p++) if (!isalnum((unsigned char)*p)) return false;
        const char* dash = strchr(slash, '-');
        if (!dash || dash - slash - 1 < 2 || dash - slash - 1 > 3) return false;
        for (const char* p = slash + 1; p < dash; p++) if (!isalnum((unsigned char)*p)) return false;
        if (strlen(dash + 1) != 3) return false;
        for (const char* p = dash + 1; *p; p++) if (!isdigit((unsigned char)*p)) return false;
        return true;
    }

    const char* mode_for_freq(float mhz) {
        static const float FT8[] = {1.840f, 3.573f, 7.074f, 10.136f, 14.074f, 18.100f, 21.074f, 24.915f, 28.074f, 50.313f};
        for (float f : FT8) if (mhz > f - 0.003f && mhz < f + 0.003f) return "FT8";

        static const float CW[][2] = {
            {1.800f, 1.840f}, {3.500f, 3.600f}, {7.000f, 7.070f}, {10.100f, 10.150f}, {14.000f, 14.070f},
            {18.068f, 18.095f}, {21.000f, 21.070f}, {24.890f, 24.915f}, {28.000f, 28.070f},
            {50.000f, 50.100f}, {144.000f, 144.150f}};
        for (const auto& r : CW) if (mhz >= r[0] && mhz < r[1]) return "CW";

        if ((mhz >= 144.5f && mhz < 148.0f) || mhz >= 430.0f) return "FM";
        return "SSB";
    }

    // A mode word anywhere in the comment beats any guess.
    static const char* mode_from_comment(const char* comment) {
        static const char* MODES[] = {"CW", "SSB", "FM", "AM", "FT8", "FT4", "DATA"};
        char buf[48];
        copy_str(buf, sizeof(buf), comment);
        char* save = nullptr;
        for (char* w = strtok_r(buf, " ,;", &save); w; w = strtok_r(nullptr, " ,;", &save)) {
            for (const char* m : MODES) if (strcasecmp(w, m) == 0) return m;
        }
        return nullptr;
    }

    bool parse_line(const char* line, ParsedSpot& out) {
        memset(&out, 0, sizeof(out));
        if (strncmp(line, "DX de ", 6) != 0) return false;

        char buf[192];
        copy_str(buf, sizeof(buf), line + 6);
        char* colon = strchr(buf, ':');
        if (!colon) return false;
        *colon = '\0';  // spotter may be glued to the frequency ("KG7LBY-#:14325.0")
        char* save = nullptr;
        char* spotter = strtok_r(buf, " ", &save);
        if (!spotter) return false;
        copy_str(out.spotter, sizeof(out.spotter), spotter);

        char* tok[16];
        int n = 0;
        save = nullptr;
        for (char* t = strtok_r(colon + 1, " \t\r\n", &save); t && n < 16; t = strtok_r(nullptr, " \t\r\n", &save)) {
            tok[n++] = t;
        }
        if (n < 3) return false;

        char* end = nullptr;
        float khz = strtof(tok[0], &end);
        if (end == tok[0] || *end != '\0' || khz <= 0.0f) return false;
        out.freq_mhz = khz / 1000.0f;
        copy_str(out.activator, sizeof(out.activator), tok[1]);

        int stop = n;
        const char* last = tok[n - 1];
        if (strlen(last) == 5 && last[4] == 'Z' && isdigit((unsigned char)last[0]) && isdigit((unsigned char)last[1]) &&
            isdigit((unsigned char)last[2]) && isdigit((unsigned char)last[3])) {
            out.time[0] = last[0]; out.time[1] = last[1]; out.time[2] = ':';
            out.time[3] = last[2]; out.time[4] = last[3]; out.time[5] = '\0';
            stop = n - 1;
        }

        int ref = -1;
        for (int i = 2; i < stop; i++) {
            if (is_summit_ref(tok[i])) { ref = i; break; }
        }
        if (ref < 0) return false;
        copy_str(out.summit, sizeof(out.summit), tok[ref]);

        for (int i = ref + 1; i < stop; i++) {
            size_t used = strlen(out.comment);
            if (used + strlen(tok[i]) + 2 > sizeof(out.comment)) break;
            if (used) strcat(out.comment, " ");
            strcat(out.comment, tok[i]);
        }

        const char* mode = mode_from_comment(out.comment);
        if (!mode && strncasecmp(out.spotter, "RBNHOLE", 7) == 0) mode = "CW";
        if (!mode) mode = mode_for_freq(out.freq_mhz);
        copy_str(out.mode, sizeof(out.mode), mode);
        return true;
    }

}  // namespace sota_cluster
}  // namespace services
