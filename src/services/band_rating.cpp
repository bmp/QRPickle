#include "band_rating.h"
#include <math.h>

namespace services {
    namespace bands {

        const Band BANDS[BAND_COUNT] = {
            {"160m", 1.85f, 0},
            {"80m", 3.6f, 0},
            {"60m", 5.35f, 0},
            {"40m", 7.1f, 1},
            {"30m", 10.12f, 1},
            {"20m", 14.1f, 2},
            {"17m", 18.1f, 2},
            {"15m", 21.2f, 3},
            {"12m", 24.9f, 3},
            {"10m", 28.5f, 3},
            {"6m", 50.1f, 4},
        };
        const char* const GROUP_NAMES[GROUP_COUNT] = {"160-60m", "40-30m", "20-17m", "15-10m", "6m"};

        const Model MODEL = {
            18.0f,  // muf_day_base: daytime MUF 18 MHz at solar minimum ...
            0.14f,  // muf_day_per_ssn: ... ~39 MHz at R = 150
            10.5f,  // muf_night_base: night MUF 10.5 MHz at solar minimum ...
            0.08f,  // muf_night_per_ssn: ... ~22.5 MHz at R = 150
            0.85f,  // fot_ratio: the classic 85 % of the MUF
            4.0f,  // luf_base: daytime LUF 4 MHz at SFI 70 ...
            0.02f,  // luf_per_sfi: ... 6.6 MHz at SFI 200
            1.5f,  // luf_fair_ratio
            7.0f,  // low_band_mhz
            50.0f,  // six_m_mhz
        };

        float effective_ssn(int16_t sfi) {
            if (sfi <= 64) return 0.0f;
            const float a = 0.00089f, b = 0.728f, c = 63.7f - sfi;
            return (-b + sqrtf(b * b - 4 * a * c)) / (2 * a);
        }

        float storm_factor(int16_t k, int16_t a) {
            float fk = 1.0f;
            if (k >= 7) fk = 0.65f;
            else if (k == 6) fk = 0.75f;
            else if (k == 5) fk = 0.85f;
            else if (k == 4) fk = 0.95f;
            float fa = 1.0f;
            if (a >= 50) fa = 0.75f;
            else if (a >= 30) fa = 0.85f;
            else if (a >= 20) fa = 0.95f;
            return fk < fa ? fk : fa;
        }

        float muf_mhz(const Inputs& in, bool night) {
            const float r = effective_ssn(in.sfi);
            const float muf = night ? MODEL.muf_night_base + MODEL.muf_night_per_ssn * r
                                    : MODEL.muf_day_base + MODEL.muf_day_per_ssn * r;
            return muf * storm_factor(in.k_index, in.a_index);
        }

        float luf_mhz(int16_t sfi) {
            const float luf = MODEL.luf_base + MODEL.luf_per_sfi * (sfi - 70);
            return luf < 3.0f ? 3.0f : luf;
        }

        bool es_season(float lat, uint8_t month) {
            if (lat >= 0) return month >= 5 && month <= 8;
            return month >= 11 || month <= 2;
        }

        static Rating worse(Rating a, Rating b) { return a < b ? a : b; }

        Rating rate_band(int band, const Inputs& in, bool night) {
            if (band < 0 || band >= BAND_COUNT || in.sfi <= 0 || in.k_index < 0) return Rating::UNKNOWN;
            const float f = BANDS[band].mhz;
            const float muf = muf_mhz(in, night);

            // Skywave: comfortably below the MUF (at or under the FOT) is good, up to the MUF fair.
            Rating r = f <= MODEL.fot_ratio * muf ? Rating::GOOD : (f <= muf ? Rating::FAIR : Rating::POOR);

            if (f >= MODEL.six_m_mhz) {
                if (r != Rating::POOR) return r;  // F2 opening (solar maximum)
                return !night && es_season(in.lat, in.month) ? Rating::ES_POSSIBLE : Rating::CLOSED;
            }
            if (!night) {  // daytime D-layer absorption
                const float luf = luf_mhz(in.sfi);
                if (f < luf) r = worse(r, Rating::POOR);
                else if (f < MODEL.luf_fair_ratio * luf) r = worse(r, Rating::FAIR);
            }
            if (f < MODEL.low_band_mhz) {  // low bands: geomagnetic noise
                if (in.k_index >= 5) r = worse(r, Rating::POOR);
                else if (in.k_index >= 3) r = worse(r, Rating::FAIR);
            }
            return r;
        }

        Rating rate_group(int group, const Inputs& in, bool night) {
            Rating best = Rating::UNKNOWN;
            bool any = false;
            for (int i = 0; i < BAND_COUNT; i++) {
                if (BANDS[i].group != group) continue;
                const Rating r = rate_band(i, in, night);
                if (!any) best = r;
                else if (r == Rating::GOOD || (r == Rating::FAIR && best != Rating::GOOD) ||
                         (r == Rating::ES_POSSIBLE && best == Rating::CLOSED))
                    best = r;
                any = true;
            }
            return best;
        }

    }  // namespace bands
}  // namespace services
