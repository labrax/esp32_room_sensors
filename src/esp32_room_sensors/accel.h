
#ifndef _ACCEL_H_
#define _ACCEL_H_

#include "defines.h"
#include "SDData.h"
#include "TimeFunctions.h"
#include <Wire.h>
#include <TinyMPU6050.h>

extern MPU6050 mpu;
extern bool mpu_present;

struct accel_data {
  long time_recorded;
  int16_t rawAccX, rawAccY, rawAccZ, rawGyroX, rawGyroY, rawGyroZ;
};

void tick_accel();
void loop_accel();

// Reads one sample and returns the acceleration in g. False when no MPU6050 is
// fitted. Call from a single task only: Execute() shares the I2C bus.
bool read_accel_g(float *x_g, float *y_g, float *z_g);

#endif
