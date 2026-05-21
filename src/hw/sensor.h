#ifndef SENSOR_H
#define SENSOR_H

// Initializes the physical I2C bus and verifies connection to the BME280 sensor chip
void sensor_init();

// Thread-safe getters to pull the latest environmental data snapshots
float sensor_get_temp();
float sensor_get_humidity();
float sensor_get_pressure();

#endif // SENSOR_H
