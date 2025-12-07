#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_LSM303_U.h>
#include <Adafruit_Sensor.h>

// Declare the global sensor objects (defined in LevelCompact.ino)
extern Adafruit_LSM303_Accel_Unified accel;
extern Adafruit_LSM303_Mag_Unified mag;

#endif  // SENSORS_H