#ifndef STATUS_SERVER_H
#define STATUS_SERVER_H

#include <stdint.h>

// Immutable copy of the last sensor readings. The sensor tasks publish after
// each read; the HTTP handlers copy it under a mutex. The web task never
// touches I2C (the I2C lock from HS-5 is not in this branch).
struct SensorSnapshot {
  float temperature_c;
  float humidity_pct;
  float pressure_hpa;
  bool valid;
  uint32_t updated_ms; // millis() when published
  int dust_raw;
  int photo_raw;
  bool air_valid;
  int pir; // 0=no motion, 1=motion
  bool pir_valid;
  uint32_t pir_updated_ms;
  float accel_x_g;
  float accel_y_g;
  float accel_z_g;
  bool accel_valid;
  uint32_t accel_updated_ms;
};

// Starts the AP (per AP_ALWAYS_ON), the HTTP server on port 80, and the
// dedicated status task. Call once from setup(), before init_time(), so an
// AP-only boot still gets an AP before the bounded NTP attempt.
void status_server_begin(void);

// Publishes a new snapshot. Non-blocking; drops the sample if the lock is busy.
void status_publish_sensors(float temperature_c, float humidity_pct, float pressure_hpa,
                            int dust_raw, int photo_raw);

// Publishes the latest PIR level (0/1).
void status_publish_pir(int pir);

// Publishes the latest accelerometer reading in g.
void status_publish_accel(float x_g, float y_g, float z_g);

#endif
