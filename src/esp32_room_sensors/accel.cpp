
#include "accel.h"

MPU6050 mpu (Wire);
bool mpu_present = false;

void tick_accel() {
  //mpu.Execute();
  mpu.UpdateRawAccel();
  mpu.UpdateRawGyro();

  struct accel_data a;
  a.time_recorded = current_time();
  a.rawAccX = mpu.GetRawAccX();
  a.rawAccY = mpu.GetRawAccY();
  a.rawAccZ = mpu.GetRawAccZ();
  a.rawGyroX = mpu.GetRawGyroX();
  a.rawGyroY = mpu.GetRawGyroY();
  a.rawGyroZ = mpu.GetRawGyroZ();

  data_accel_output.write((const uint8_t *) &a, sizeof(struct accel_data));
  data_accel_output.flush();
}

void loop_accel() {
  while(record_streams == 1) {
    tick_accel();
    vTaskDelay((1000/ACCEL_FREQ)/portTICK_PERIOD_MS);
  }
  close_accel_file();
}

// One-shot read for the status page when nothing is being recorded. Not
// reentrant: Execute() must not run from two tasks at once.
bool read_accel_g(float *x_g, float *y_g, float *z_g) {
  if (!mpu_present) {
    return false;
  }
  mpu.Execute();
  *x_g = mpu.GetAccX();
  *y_g = mpu.GetAccY();
  *z_g = mpu.GetAccZ();
  return true;
}
