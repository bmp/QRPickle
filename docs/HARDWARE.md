# Hardware and Wiring

QRPickle runs on the **ESP32-2432S028R "Cheap Yellow Display" (CYD)**, the 2.8" ILI9341 version with a resistive touch screen. Nothing needs soldering. The only optional extra is a **BME280** sensor for indoor temperature, humidity and pressure.

For the full board pinout, see Renzo Mischianti's **[ESP32-2432S028 (Cheap Yellow Display): high-resolution pinout, datasheet, schema and specs](https://mischianti.org/esp32-2432s028-cheap-yellow-display-high-resolution-pinout-datasheet-schema-and-specs/)** (mischianti.org, CC BY-NC-ND).

> **Not the 3.2" board.** The ESP32-2432S032 ("CYD v2") uses a different display and pin map, so QRPickle doesn't support it.

## The board

The reference CYD (checked with `esptool flash_id` and the boot log):

| Item | Value |
|---|---|
| Chip | ESP32-D0WD-V3 (revision v3.1), dual core, 240 MHz, WiFi + Bluetooth |
| Flash | 4 MB |
| PSRAM | none |
| Display | 2.8" ILI9341, 320 x 240 |
| Touch | XPT2046, resistive |

## Pins QRPickle uses

These come from `src/hw/User_Setup.h`, `src/hw/touch.cpp`, `src/hw/sensor.cpp`, `src/hw/led_rgb.cpp` and `src/services/display_manager.cpp`.

| Function | GPIO | Notes |
|---|---|---|
| Display SPI | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2 | Reset is wired to 3.3V on the board |
| Display backlight | 21 | PWM; also appears on connector P3, so don't use it there |
| Touch SPI | MOSI 32, MISO 39, CLK 25, CS 33 | Separate SPI bus |
| Light sensor (LDR) | 34 (ADC1 channel 6) | Auto-brightness |
| RGB status LED | R 4, G 16, B 17 | Active low; colours: [LEDColours.md](LEDColours.md) |
| **BME280 (I2C)** | **SDA 27, SCL 22** | Optional, on connector **CN1** |

The micro-SD slot and the speaker connector are not used.

## Optional BME280 sensor

The dashboard and weather screen show indoor temperature, humidity and pressure from a BME280 connected to **CN1**, the small 4-pin socket on the board's edge. Without a sensor, QRPickle shows `--` for these values.

### Wiring

| CN1 pin | CYD signal | BME280 pin |
|---|---|---|
| 1 | GND | GND |
| 2 | GPIO22 | SCL |
| 3 | GPIO27 | SDA |
| 4 | 3.3V | VIN / VCC |

```
   CYD connector CN1                    BME280 module
  +-----+--------+                     +-----------+
  |  1  |  GND   |---------------------| GND       |
  |  2  | GPIO22 |---------------------| SCL       |
  |  3  | GPIO27 |---------------------| SDA       |
  |  4  |  3.3V  |---------------------| VIN / VCC |
  +-----+--------+                     +-----------+
```

Check the labels printed next to CN1 on your board before connecting. Wire colours vary between cables, so match by the printed labels, not by colour.

### Notes

- **Use 3.3V.** CN1 provides 3.3V. Most breakout modules accept it on VIN.
- **Address:** QRPickle tries both I2C addresses, `0x76` and `0x77`, so either SDO setting works.
- **6-pin modules** (with CSB and SDO): connect CSB to 3.3V to select I2C mode. SDO selects the address (GND = 0x76, 3.3V = 0x77).
- **BMP280 is not a BME280.** Many cheap modules sold as "BME/BMP280" carry a BMP280, which has no humidity sensor. QRPickle doesn't detect a BMP280 and shows the sensor as offline.
- **Detection happens at boot.** Power-cycle the CYD after connecting the sensor.

### Checking it works

- On the device, the dashboard shows the indoor values instead of `--`.
- In the web console, **System Info** shows the temperature, and `/api/status` reports `"sensor_online": true`.

## Power

Power the CYD from a 5V USB supply through the micro-USB socket. The firmware limits WiFi transmit power to avoid brown-outs on the board's 3.3V regulator. If the board resets during WiFi activity, try a better cable or supply first.
