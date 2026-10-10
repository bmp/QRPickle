#include "sun_calc.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

namespace services {
    namespace sun {

        static constexpr double DEG = M_PI / 180.0;

        // South-west corner and size of a locator's square.
        static bool grid_box(const char* g, double& lat0, double& lon0, double& dlat, double& dlon) {
            const size_t n = g ? strlen(g) : 0;
            if (n != 4 && n != 6) return false;
            const int f0 = toupper((unsigned char)g[0]) - 'A', f1 = toupper((unsigned char)g[1]) - 'A';
            if (f0 < 0 || f0 > 17 || f1 < 0 || f1 > 17) return false;
            if (!isdigit((unsigned char)g[2]) || !isdigit((unsigned char)g[3])) return false;
            lon0 = -180.0 + f0 * 20.0 + (g[2] - '0') * 2.0;
            lat0 = -90.0 + f1 * 10.0 + (g[3] - '0') * 1.0;
            dlon = 2.0;
            dlat = 1.0;
            if (n == 6) {
                const int s0 = tolower((unsigned char)g[4]) - 'a', s1 = tolower((unsigned char)g[5]) - 'a';
                if (s0 < 0 || s0 > 23 || s1 < 0 || s1 > 23) return false;
                dlon = 2.0 / 24.0;
                dlat = 1.0 / 24.0;
                lon0 += s0 * dlon;
                lat0 += s1 * dlat;
            }
            return true;
        }

        bool grid_to_latlon(const char* grid, float& lat, float& lon) {
            double lat0, lon0, dlat, dlon;
            if (!grid_box(grid, lat0, lon0, dlat, dlon)) return false;
            lat = (float)(lat0 + dlat / 2);
            lon = (float)(lon0 + dlon / 2);
            return true;
        }

        bool latlon_in_grid(const char* grid, float lat, float lon) {
            double lat0, lon0, dlat, dlon;
            if (!grid_box(grid, lat0, lon0, dlat, dlon)) return false;
            return lat >= lat0 && lat < lat0 + dlat && lon >= lon0 && lon < lon0 + dlon;
        }

        bool station_location(bool latlon_set, float lat, float lon, const char* grid, float& out_lat, float& out_lon) {
            if (!latlon_set && grid_to_latlon(grid, out_lat, out_lon)) return true;
            out_lat = lat;
            out_lon = lon;
            return false;
        }

        float elevation_deg(float lat, float lon, uint32_t utc) {
            const double d = utc / 86400.0 - 10957.5;  // days since J2000.0 (2000-01-01 12:00 UTC)
            const double g = fmod(357.529 + 0.98560028 * d, 360.0) * DEG;  // mean anomaly
            const double q = fmod(280.459 + 0.98564736 * d, 360.0);  // mean longitude
            const double L = (q + 1.915 * sin(g) + 0.020 * sin(2 * g)) * DEG;  // ecliptic longitude
            const double e = (23.439 - 0.00000036 * d) * DEG;  // obliquity
            const double ra = atan2(cos(e) * sin(L), cos(L));
            const double dec = asin(sin(e) * sin(L));
            const double gmst_deg = fmod(280.46061837 + 360.98564736629 * d, 360.0);
            const double ha = gmst_deg * DEG + lon * DEG - ra;  // hour angle
            const double la = lat * DEG;
            return (float)(asin(sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha)) / DEG);
        }

        Light light_at(float lat, float lon, uint32_t utc) {
            const float el = elevation_deg(lat, lon, utc);
            if (el > GREY_DEG) return Light::DAY;
            if (el < -GREY_DEG) return Light::NIGHT;
            return Light::GREYLINE;
        }

        uint32_t next_sun_event(float lat, float lon, uint32_t from_utc, bool& rising) {
            constexpr float HORIZON = -0.833f;
            constexpr uint32_t STEP = 600;  // 10 min, then bisect to the second
            float prev = elevation_deg(lat, lon, from_utc) - HORIZON;
            for (uint32_t t = from_utc + STEP; t <= from_utc + 48 * 3600; t += STEP) {
                const float cur = elevation_deg(lat, lon, t) - HORIZON;
                if ((prev < 0) != (cur < 0)) {
                    rising = cur >= 0;
                    uint32_t lo = t - STEP, hi = t;
                    while (hi - lo > 1) {
                        const uint32_t mid = lo + (hi - lo) / 2;
                        if ((elevation_deg(lat, lon, mid) - HORIZON < 0) == (prev < 0)) lo = mid;
                        else hi = mid;
                    }
                    return hi;
                }
                prev = cur;
            }
            return 0;
        }

    }  // namespace sun
}  // namespace services
