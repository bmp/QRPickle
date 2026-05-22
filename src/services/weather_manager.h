#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace services {
    namespace weather_manager {

        struct CurrentWeather {
            bool valid = false;
            float temp = 0.0f;
            uint8_t humidity = 0;
            float pressure = 0.0f;
            float wind_speed = 0.0f; 
            uint16_t wind_deg = 0;
            uint8_t clouds = 0;      
            uint16_t visibility = 0; 
            char description[32] = "";
            char icon[4] = "";       
            char location[32] = "";
            uint32_t sunrise = 0;
            uint32_t sunset = 0;
        };

        struct ForecastBlock {
            uint32_t dt = 0;         
            float temp = 0.0f;
            uint8_t pop = 0;         
            float wind_speed = 0.0f; // FIXED: Added wind speed extraction for Forecast tab
            char icon[4] = "";
        };

        struct ForecastWeather {
            bool valid = false;
            ForecastBlock blocks[8]; 
        };

        void init();
        void update();
        void force_refresh();
        const CurrentWeather& get_current();
        const ForecastWeather& get_forecast();
    }
}