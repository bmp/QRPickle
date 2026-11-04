# Image sources

Every file in `assets/img/` (and its converted `data/img/*.bin`) must be listed here with its source and licence. `scripts/check_licenses.py` fails the build otherwise and warns while an entry is still UNCONFIRMED.

| File(s) | What | Source | Licence |
|---|---|---|---|
| 01d_36x36.png, 01n_36x36.png, 02d_36x36.png, 02n_36x36.png, 03d_36x36.png, 03n_36x36.png, 04d_36x36.png, 04n_36x36.png, 09d_36x36.png, 09n_36x36.png, 10d_36x36.png, 10n_36x36.png, 11d_36x36.png, 11n_36x36.png, 13d_36x36.png, 13n_36x36.png, 50d_36x36.png, 50n_36x36.png | Weather condition icons (OpenWeatherMap icon codes) | OpenWeather's condition icons ([openweathermap.org/weather-conditions](https://openweathermap.org/weather-conditions)), resized to 36x36 (one 4 KB LittleFS block each) | No icon-specific licence published; attributed to OpenWeather (its free-plan terms require attribution). Credited in About. |
| icon_web_20x20.png | Globe icon (web console) | [Font Awesome 7 Free](https://fontawesome.com) "globe" (solid) by Fonticons, Inc.; recoloured #1E2E4F and padded | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Credited in About. |
| icon_sensor_20x20.png | Sensor icon | [Font Awesome 7 Free](https://fontawesome.com) "microchip" (solid) by Fonticons, Inc.; recoloured #1E2E4F and padded | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Credited in About. |
| logo_splash_left_50x50.png | Bangalore Amateur Radio Club (VU2ARC) logo (splash screen); also compiled in as `src/ui/img/logo_splash_left_50x50.c` | [barc.in](https://www.barc.in/) | © Bangalore Amateur Radio Club; used with the club's permission (confirmed to the owner, 2026-10-10). Not covered by QRPickle's MIT licence. Credited in About. |
| logo_splash_right_30x60.png | International amateur radio symbol (splash screen); also compiled in as `src/ui/img/logo_splash_right_30x60.c` | [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:International_amateur_radio_symbol.svg), by Denelson83 (2007), scaled to 30x60 | Public domain (released by the author) |

Web console files outside `assets/img/`:
- `data/www/favicon.svg`: the International amateur radio symbol from [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:International_amateur_radio_symbol.svg) by Denelson83 (public domain); editor metadata removed and a square viewBox added, drawing unchanged.

