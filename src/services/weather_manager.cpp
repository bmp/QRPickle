#include "weather_manager.h"
#include "wifi_manager.h"
#include "../config/config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace services {
    namespace weather_manager {

        static CurrentWeather current_wx;
        static ForecastWeather forecast_wx;
        
        static unsigned long last_fetch_ms = 0;
        static bool initial_fetch_done = false;
        static uint32_t active_interval_ms = 7200000;  

        static void fetch_current(const config::Config& cfg) {
            if (strlen(cfg.openweather_api_key) == 0) return;
            char url[256];
            snprintf(url, sizeof(url), "http://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&appid=%s&units=metric", cfg.lat, cfg.lon, cfg.openweather_api_key);

            HTTPClient http;
            http.begin(url);
            if (http.GET() == 200) {
                // NEW: Strict Memory Filter
                JsonDocument filter;
                filter["main"]["temp"] = true;
                filter["main"]["humidity"] = true;
                filter["main"]["pressure"] = true;
                filter["wind"]["speed"] = true;
                filter["wind"]["deg"] = true;
                filter["clouds"]["all"] = true;
                filter["visibility"] = true;
                filter["sys"]["sunrise"] = true;
                filter["sys"]["sunset"] = true;
                filter["weather"][0]["description"] = true;
                filter["weather"][0]["icon"] = true;
                filter["name"] = true;

                JsonDocument doc;
                if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
                    current_wx.temp = doc["main"]["temp"] | 0.0f;
                    current_wx.humidity = doc["main"]["humidity"] | 0;
                    current_wx.pressure = doc["main"]["pressure"] | 0.0f;
                    current_wx.wind_speed = doc["wind"]["speed"] | 0.0f;
                    current_wx.wind_deg = doc["wind"]["deg"] | 0;
                    current_wx.clouds = doc["clouds"]["all"] | 0;
                    current_wx.visibility = doc["visibility"] | 0;
                    current_wx.sunrise = doc["sys"]["sunrise"] | 0;  
                    current_wx.sunset = doc["sys"]["sunset"] | 0;   
                    strlcpy(current_wx.description, doc["weather"][0]["description"] | "Unknown", sizeof(current_wx.description));
                    strlcpy(current_wx.icon, doc["weather"][0]["icon"] | "01d", sizeof(current_wx.icon));
                    strlcpy(current_wx.location, doc["name"] | "Local Area", sizeof(current_wx.location));
                    current_wx.valid = true;
                }
            }
            http.end();
        }

        static void fetch_forecast(const config::Config& cfg) {
            if (strlen(cfg.openweather_api_key) == 0) return;
            char url[256];
            snprintf(url, sizeof(url), "http://api.openweathermap.org/data/2.5/forecast?lat=%.4f&lon=%.4f&appid=%s&units=metric&cnt=8", cfg.lat, cfg.lon, cfg.openweather_api_key);

            HTTPClient http;
            http.begin(url);
            if (http.GET() == 200) {
                // NEW: Strict Memory Filter
                JsonDocument filter;
                filter["list"][0]["dt"] = true;
                filter["list"][0]["main"]["temp"] = true;
                filter["list"][0]["pop"] = true;
                filter["list"][0]["wind"]["speed"] = true;
                filter["list"][0]["weather"][0]["icon"] = true;

                JsonDocument doc;
                if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
                    JsonArray list = doc["list"].as<JsonArray>();
                    int idx = 0;
                    for (JsonObject item : list) {
                        if (idx >= 8) break;
                        forecast_wx.blocks[idx].dt = item["dt"] | 0;
                        forecast_wx.blocks[idx].temp = item["main"]["temp"] | 0.0f;
                        forecast_wx.blocks[idx].pop = (uint8_t)((item["pop"] | 0.0f) * 100.0f);
                        forecast_wx.blocks[idx].wind_speed = item["wind"]["speed"] | 0.0f;
                        strlcpy(forecast_wx.blocks[idx].icon, item["weather"][0]["icon"] | "01d", sizeof(forecast_wx.blocks[idx].icon));
                        idx++;
                    }
                    forecast_wx.valid = true;
                }
            }
            http.end();
        }

        void init() { current_wx.valid = false; forecast_wx.valid = false; }

        void update() {
            if (!wifi_manager_is_connected()) return;
            unsigned long now = millis();

            if (current_wx.valid && current_wx.sunrise > 0) {
                time_t sys_now = time(nullptr);
                long dist_sunrise = labs((long)(sys_now - current_wx.sunrise));
                long dist_sunset  = labs((long)(sys_now - current_wx.sunset));
                
                if (dist_sunrise <= 2700 || dist_sunset <= 2700) {  
                    active_interval_ms = 180000;  
                } else {
                    active_interval_ms = 7200000;  
                }
            }

            if (!initial_fetch_done || (now - last_fetch_ms >= active_interval_ms)) {
                last_fetch_ms = now;
                initial_fetch_done = true;
                const auto& cfg = config::get();
                fetch_current(cfg);
                fetch_forecast(cfg);
            }
        }

        void force_refresh() { last_fetch_ms = 0; initial_fetch_done = false; }
        const CurrentWeather& get_current() { return current_wx; }
        const ForecastWeather& get_forecast() { return forecast_wx; }
    }
}