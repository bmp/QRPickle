# Third-Party Notices

QRPickle's own code is MIT-licensed (see [`License`](License)). The firmware and documentation also contain the components below, each under its own licence.

## In the firmware

| Component | Licence | Copyright / source | Licence text |
|---|---|---|---|
| Atkinson Hyperlegible font (AtkinsonHyperlegible-Regular.ttf, converted to `src/ui/fonts/font_atkinson_*_raw.c`) | SIL OFL 1.1 | © 2020 Braille Institute of America, Inc. | [`assets/fonts/OFL-AtkinsonHyperlegible.txt`](assets/fonts/OFL-AtkinsonHyperlegible.txt) |
| JetBrains Mono font (JetBrainsMono-Bold.ttf, converted to `src/ui/fonts/font_jetbrains_*_raw.c`) | SIL OFL 1.1 | © 2020 The JetBrains Mono Project Authors | [`assets/fonts/OFL-JetBrainsMono.txt`](assets/fonts/OFL-JetBrainsMono.txt) |
| Montserrat font (LVGL built-in `lv_font_montserrat_*`) | SIL OFL 1.1 | © 2011 The Montserrat Project Authors | [`assets/fonts/OFL-LVGL-builtin.txt`](assets/fonts/OFL-LVGL-builtin.txt) |
| Font Awesome 5 Free symbols (in the LVGL built-in fonts) | SIL OFL 1.1 | © Fonticons, Inc. | [`assets/fonts/OFL-LVGL-builtin.txt`](assets/fonts/OFL-LVGL-builtin.txt) |
| Font Awesome 7 Free icons "globe" and "microchip" (`assets/img/icon_*`), recoloured | CC BY 4.0 | © Fonticons, Inc. | [creativecommons.org/licenses/by/4.0](https://creativecommons.org/licenses/by/4.0/) |
| Weather condition icons (`assets/img/[0-9]*`) | No published icon licence; attributed | © OpenWeather, [openweathermap.org](https://openweathermap.org/weather-conditions) | [`assets/img/SOURCES.md`](assets/img/SOURCES.md) |
| International amateur radio symbol (splash screen; web console favicon `data/www/favicon.svg`) | Public domain | Denelson83, [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:International_amateur_radio_symbol.svg) | [`assets/img/SOURCES.md`](assets/img/SOURCES.md) |
| Bangalore Amateur Radio Club (VU2ARC) logo (splash screen) | Not MIT; used with the club's permission | © Bangalore Amateur Radio Club, [barc.in](https://www.barc.in/) | [`assets/img/SOURCES.md`](assets/img/SOURCES.md) |
| LVGL 9.6 | MIT | © LVGL Kft, [lvgl/lvgl](https://github.com/lvgl/lvgl) | in the library |
| TFT_eSPI 2.5 | MIT (per library.json) | Bodmer, [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | in the library |
| ArduinoJson 7 | MIT | © Benoît Blanchon, [arduinojson.org](https://arduinojson.org) | in the library |
| ESPAsyncWebServer 3.12, AsyncTCP 3.5 | LGPL-3.0 | [ESP32Async](https://github.com/ESP32Async) | in the library; QRPickle's full source is public, so the firmware can be relinked with modified versions |
| Adafruit BME280 Library | BSD | © Adafruit Industries | in the library |
| Adafruit Unified Sensor | Apache-2.0 | © Adafruit Industries | in the library |
| Adafruit BusIO | MIT | © Adafruit Industries | in the library |
| XPT2046_Touchscreen | MIT-style | © 2015 Paul Stoffregen | in the library |
| Arduino-ESP32 core 2.0.17 (platform `espressif32 @ 6.13.0`) | LGPL-2.1 | © Espressif Systems and contributors | [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) |
| ESP-IDF 4.4 (incl. FreeRTOS, lwIP, mbedTLS) | Apache-2.0 (FreeRTOS: MIT, lwIP: BSD) | © Espressif Systems and the respective authors | [espressif/esp-idf](https://github.com/espressif/esp-idf) |

The libraries are fetched by PlatformIO at the versions pinned in `platformio.ini`; their licence files come with them. Image assets and their sources are listed in [`assets/img/SOURCES.md`](assets/img/SOURCES.md).

`scripts/check_licenses.py` runs on every build and in CI. It fails when a library, font, platform version, image or file with a foreign copyright line is added without being recorded here (or in the image manifests).

## In the documentation only

| Component | Licence | Copyright / source |
|---|---|---|
| CYD pinout image (`docs/pics/third-party/`) | CC BY-NC-ND 4.0 (not MIT) | © Renzo Mischianti, [mischianti.org](https://mischianti.org/esp32-2432s028-cheap-yellow-display-high-resolution-pinout-datasheet-schema-and-specs/). Unmodified; see [`docs/pics/third-party/README.md`](docs/pics/third-party/README.md). |

## Web installer page

The GitHub Pages installer loads [ESP Web Tools](https://github.com/esphome/esp-web-tools) (Apache-2.0) from a CDN; it is not bundled.
