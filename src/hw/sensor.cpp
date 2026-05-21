#include "sensor.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Standard physical hardware pin mappings for the external CN1 port on the CYD board
#define I2C_SDA 27
#define I2C_SCL 22

static Adafruit_BME280 bme;
static bool sensor_online = false;

void sensor_init() {
    Wire.begin(I2C_SDA, I2C_SCL);

    // Attempt connection on either standard default I2C slave address addresses (0x76 or 0x77)
    if (bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire)) {
        sensor_online = true;
    }
}

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
