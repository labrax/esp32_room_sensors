
#ifndef _ACCEL_H_
#define _ACCEL_H_

#include "defines.h"
#include "SDData.h"
#include "TimeFunctions.h"
#include <Wire.h>
#include <TinyMPU6050.h>

extern MPU6050 mpu;

struct accel_data {
  long time_recorded;
  int16_t rawAccX, rawAccY, rawAccZ, rawGyroX, rawGyroY, rawGyroZ;
};

void tick_accel();
void loop_accel();

#endif
