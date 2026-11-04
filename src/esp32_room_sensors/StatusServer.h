#ifndef STATUS_SERVER_H
#define STATUS_SERVER_H

#include <stdint.h>

// Immutable copy of the last BME280 reading. The sensor task publishes it after
// each read; the HTTP handlers copy it under a mutex. The web task never
// touches I2C (the I2C lock from HS-5 is not in this branch).
struct SensorSnapshot {
  float temperature_c;
  float humidity_pct;
  float pressure_hpa;
  bool valid;
  uint32_t updated_ms; // millis() when published
};

// Starts the AP (per AP_ALWAYS_ON), the HTTP server on port 80, and the
// dedicated status task. Call once from setup(), after WiFi.begin().
void status_server_begin(void);

// Publishes a new snapshot. Non-blocking; drops the sample if the lock is busy.
void status_publish_sensors(float temperature_c, float humidity_pct, float pressure_hpa);

#endif
