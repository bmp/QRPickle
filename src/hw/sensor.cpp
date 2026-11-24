#include "sensor.h"
#include "board.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

static Adafruit_BME280 bme;
static bool sensor_online = false;

void sensor_init() {
    // The module connector's I2C pins come from the board (CN1 on the 2.8" boards: IO27/IO22). Boards
    // without a mapped connector, or a board in doubt, leave the sensor off.
    const hw::BoardDef& b = hw::board::active();
    if (!hw::board::selection().sensors_on || b.i2c_sda < 0 || b.i2c_scl < 0) {
        Serial.println("[Sensor] Module connector not mapped for this board; BME280 off.");
        return;
    }
    Wire.begin(b.i2c_sda, b.i2c_scl);

    // Attempt connection on either standard default I2C slave address addresses (0x76 or 0x77)
    if (bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire)) {
        sensor_online = true;
    }
}

bool sensor_is_online() { return sensor_online; }

float sensor_get_temp() {
    return sensor_online ? bme.readTemperature() : 0.0f;
}

float sensor_get_humidity() {
    return sensor_online ? bme.readHumidity() : 0.0f;
}

float sensor_get_pressure() {
    // Converts raw pascals directly to standard hectopascals (hPa)
    return sensor_online ? (bme.readPressure() / 100.0f) : 0.0f;
}
