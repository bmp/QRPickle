#ifndef SENSOR_H
#define SENSOR_H

// Initializes the physical I2C bus and verifies connection to the BME280 sensor chip
void sensor_init();

// False when no BME280 answered at boot; the getters then return 0.0 (review 5.1).
bool sensor_is_online();

// Thread-safe getters to pull the latest environmental data snapshots
float sensor_get_temp();
float sensor_get_humidity();
float sensor_get_pressure();

#endif // SENSOR_H
