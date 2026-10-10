#include "config_json.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

namespace config {

    namespace {
        enum class Kind : uint8_t { STR, SECRET, U8, I8, U16, BOOL, F32, TZ_HOURS, MACROS };
        struct Field { const char* key; const char* set_key; Kind kind; size_t offset; size_t size; };
        #define FIELD(key, kind, member)   { key, nullptr, Kind::kind, offsetof(Config, member), sizeof(Config::member) }
        #define SECRET(key, member)    { key, key "_set", Kind::SECRET, offsetof(Config, member), sizeof(Config::member) }

        // Keys are the ones the web console already uses.
        // clang-format off
        const Field FIELDS[] = {
            FIELD("callsign",     STR,      callsign),
            FIELD("grid",         STR,      grid),
            FIELD("ssid",         STR,      wifi_ssid),
            SECRET("password",          wifi_password),
            SECRET("apikey",            openweather_api_key),
            FIELD("lat",          F32,      lat),
            FIELD("lon",          F32,      lon),
            FIELD("offset",       TZ_HOURS, tz_offset_hh),
            FIELD("brightness",   U8,       brightness),
            FIELD("auto_bright",  BOOL,     auto_brightness),
            FIELD("theme_id",     U8,       theme_id),
            FIELD("timeout",      U8,       screen_timeout_min),
            FIELD("fc_slots",     U8,       forecast_slots),
            FIELD("dx_url_p",     STR,      dx_url_primary),
            FIELD("dx_port_p",    U16,      dx_port_primary),
            FIELD("dx_url_s",     STR,      dx_url_secondary),
            FIELD("dx_port_s",    U16,      dx_port_secondary),
            FIELD("solar_url",    STR,      solar_url),
            FIELD("aprs_en",      BOOL,     aprs_enabled),
            SECRET("aprs_pass",         aprs_passcode),
            FIELD("aprs_ssid",    I8,       aprs_ssid),
            FIELD("aprs_cmt",     STR,      aprs_comment),
            FIELD("aprs_icn",     STR,      aprs_icon),
            FIELD("aprs_macros",  MACROS,   aprs_macros),
            SECRET("hamalert_pass",     hamalert_password),
        };
        #undef FIELD
        #undef SECRET
        // clang-format on

        constexpr size_t MACRO_COUNT = sizeof(Config::aprs_macros) / sizeof(Config::aprs_macros[0]);
        constexpr size_t MACRO_SIZE = sizeof(Config::aprs_macros[0]);

        void copy_into(char* dst, size_t size, const char* src) {
            strncpy(dst, src, size - 1);
            dst[size - 1] = '\0';
        }

        long clamp_int(JsonVariantConst v, long lo, long hi) {
            long x = v.as<long>();
            return x < lo ? lo : (x > hi ? hi : x);
        }
    }

    void to_json(const Config& c, JsonObject out, Secrets secrets) {
        for (const Field& f : FIELDS) {
            const void* src = reinterpret_cast<const uint8_t*>(&c) + f.offset;
            // (const char*) casts make ArduinoJson copy the text instead of keeping a pointer.
            switch (f.kind) {
                case Kind::STR:  out[f.key] = (const char*)src; break;
                case Kind::SECRET:
                    if (secrets == Secrets::Include) {
                        out[f.key] = (const char*)src;
                    } else {
                        out[f.key] = "";
                        out[f.set_key] = static_cast<const char*>(src)[0] != '\0';
                    }
                    break;
                case Kind::U8:   out[f.key] = *static_cast<const uint8_t*>(src); break;
                case Kind::I8:   out[f.key] = *static_cast<const int8_t*>(src); break;
                case Kind::U16:  out[f.key] = *static_cast<const uint16_t*>(src); break;
                case Kind::BOOL: out[f.key] = *static_cast<const bool*>(src); break;
                case Kind::F32:  out[f.key] = *static_cast<const float*>(src); break;
                case Kind::TZ_HOURS: out[f.key] = *static_cast<const int8_t*>(src) / 2.0f; break;
                case Kind::MACROS: {
                    JsonArray arr = out[f.key].to<JsonArray>();
                    for (size_t i = 0; i < MACRO_COUNT; i++) arr.add((const char*)c.aprs_macros[i]);
                    break;
                }
            }
        }
    }

    void from_json(Config& c, JsonObjectConst in) {
        for (const Field& f : FIELDS) {
            JsonVariantConst v = in[f.key];
            void* dst = reinterpret_cast<uint8_t*>(&c) + f.offset;
            switch (f.kind) {
                case Kind::STR:
                    if (v.is<const char*>()) copy_into(static_cast<char*>(dst), f.size, v.as<const char*>());
                    break;
                case Kind::SECRET:
                    if (v.is<const char*>()) {
                        const char* s = v.as<const char*>();
                        if (s[0] != '\0' && strcmp(s, "unset") != 0) copy_into(static_cast<char*>(dst), f.size, s);
                    }
                    break;
                case Kind::U8:   if (v.is<long>()) *static_cast<uint8_t*>(dst)  = (uint8_t)clamp_int(v, 0, 255); break;
                case Kind::I8:   if (v.is<long>()) *static_cast<int8_t*>(dst)   = (int8_t)clamp_int(v, -128, 127); break;
                case Kind::U16:  if (v.is<long>()) *static_cast<uint16_t*>(dst) = (uint16_t)clamp_int(v, 0, 65535); break;
                case Kind::BOOL: if (v.is<bool>()) *static_cast<bool*>(dst) = v.as<bool>(); break;
                case Kind::F32:  if (v.is<float>()) *static_cast<float*>(dst) = v.as<float>(); break;
                case Kind::TZ_HOURS:
                    if (v.is<float>()) {
                        float half_hours = roundf(v.as<float>() * 2.0f);
                        if (half_hours >= -128.0f && half_hours <= 127.0f) *static_cast<int8_t*>(dst) = (int8_t)half_hours;
                    }
                    break;
                case Kind::MACROS: {
                    JsonArrayConst arr = v.as<JsonArrayConst>();
                    for (size_t i = 0; i < MACRO_COUNT && i < arr.size(); i++) {
                        if (arr[i].is<const char*>()) copy_into(c.aprs_macros[i], MACRO_SIZE, arr[i].as<const char*>());
                    }
                    break;
                }
            }
        }
    }

    void clear_secrets(Config& c) {
        for (const Field& f : FIELDS) {
            if (f.kind == Kind::SECRET) reinterpret_cast<char*>(&c)[f.offset] = '\0';
        }
    }

}  // namespace config
