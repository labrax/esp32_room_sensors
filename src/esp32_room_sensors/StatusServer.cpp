#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "StatusServer.h"
#include "SDData.h"
#include "TimeFunctions.h"
#include "defines.h"
#include "wifi_pass.h"

// WPA2 needs 8+ characters. Fail the build rather than ship a weaker AP.
static_assert(sizeof(AP_PASSWORD) - 1 >= 8, "AP_PASSWORD must be at least 8 characters (WPA2)");

static const IPAddress AP_ADDRESS(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static WebServer status_server(80);
static SemaphoreHandle_t snapshot_lock = NULL;
static SensorSnapshot snapshot = {0.0f, 0.0f, 0.0f, false, 0};
static TaskHandle_t StatusServerTask = NULL;

// HS-4 owns the real time-source API. Until it lands, infer from the epoch:
// anything after 2021-01-01 means RTC or NTP set the clock.
static const time_t TIME_VALID_AFTER = 1609459200;

static SensorSnapshot snapshot_copy(void) {
  SensorSnapshot out = {0.0f, 0.0f, 0.0f, false, 0};
  if (snapshot_lock == NULL) {
    return out;
  }
  if (xSemaphoreTake(snapshot_lock, pdMS_TO_TICKS(5)) != pdTRUE) {
    return out; // report "no reading" rather than block the web task
  }
  out = snapshot;
  xSemaphoreGive(snapshot_lock);
  return out;
}

void status_publish_sensors(float temperature_c, float humidity_pct, float pressure_hpa) {
  if (snapshot_lock == NULL) {
    return;
  }
  if (xSemaphoreTake(snapshot_lock, pdMS_TO_TICKS(5)) != pdTRUE) {
    return;
  }
  snapshot.temperature_c = temperature_c;
  snapshot.humidity_pct = humidity_pct;
  snapshot.pressure_hpa = pressure_hpa;
  snapshot.valid = !(isnan(temperature_c) || isnan(humidity_pct) || isnan(pressure_hpa));
  snapshot.updated_ms = millis();
  xSemaphoreGive(snapshot_lock);
}

static const char *time_source_name(bool time_valid) {
  if (!time_valid) {
    return "unset";
  }
  if (rtc_start == 1) {
    return "rtc";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return "ntp";
  }
  return "unknown";
}

static void fmt_float(char *dst, size_t len, bool valid, float value, const char *missing) {
  if (valid) {
    snprintf(dst, len, "%.1f", value);
  } else {
    snprintf(dst, len, "%s", missing);
  }
}

static void fmt_utc(char *dst, size_t len, bool time_valid, time_t now) {
  if (!time_valid) {
    snprintf(dst, len, "null");
    return;
  }
  struct tm t;
  gmtime_r(&now, &t);
  char iso[24];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
  snprintf(dst, len, "\"%s\"", iso);
}

static void handle_api_status(void) {
  SensorSnapshot s = snapshot_copy();
  time_t now = time(NULL);
  bool time_valid = now > TIME_VALID_AFTER;

  char temp[12], hum[12], pres[12], utc[32];
  fmt_float(temp, sizeof(temp), s.valid, s.temperature_c, "null");
  fmt_float(hum, sizeof(hum), s.valid, s.humidity_pct, "null");
  fmt_float(pres, sizeof(pres), s.valid, s.pressure_hpa, "null");
  fmt_utc(utc, sizeof(utc), time_valid, now);

  char body[512];
  snprintf(body, sizeof(body),
           "{\"uptime_s\":%lu,\"temperature_c\":%s,\"humidity_pct\":%s,"
           "\"pressure_hpa\":%s,\"sensors_valid\":%s,\"sensor_age_ms\":%lu,"
           "\"time_source\":\"%s\",\"time_valid\":%s,\"time_utc\":%s,"
           "\"sd_ok\":%s,\"recording\":%s,\"sta_connected\":%s,"
           "\"sta_ip\":\"%s\",\"ap_ip\":\"%s\",\"ap_clients\":%u}\n",
           (unsigned long)(millis() / 1000UL), temp, hum, pres,
           s.valid ? "true" : "false",
           s.valid ? (unsigned long)(millis() - s.updated_ms) : 0UL,
           time_source_name(time_valid), time_valid ? "true" : "false", utc,
           sd_card == 1 ? "true" : "false", record_streams ? "true" : "false",
           WiFi.status() == WL_CONNECTED ? "true" : "false",
           WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str(),
           (unsigned)WiFi.softAPgetStationNum());

  status_server.sendHeader("Cache-Control", "no-store");
  status_server.send(200, "application/json", body);
}

static void handle_root(void) {
  SensorSnapshot s = snapshot_copy();
  time_t now = time(NULL);
  bool time_valid = now > TIME_VALID_AFTER;

  char temp[12], hum[12], pres[12], age[16], clock[48];
  fmt_float(temp, sizeof(temp), s.valid, s.temperature_c, "-");
  fmt_float(hum, sizeof(hum), s.valid, s.humidity_pct, "-");
  fmt_float(pres, sizeof(pres), s.valid, s.pressure_hpa, "-");
  if (s.valid) {
    snprintf(age, sizeof(age), "%lu s", (unsigned long)((millis() - s.updated_ms) / 1000UL));
  } else {
    snprintf(age, sizeof(age), "-");
  }
  if (time_valid) {
    struct tm t;
    gmtime_r(&now, &t);
    char iso[24];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
    snprintf(clock, sizeof(clock), "%s (%s)", iso, time_source_name(true));
  } else {
    snprintf(clock, sizeof(clock), "unset");
  }

  char page[1200];
  snprintf(page, sizeof(page),
           "<!doctype html><html><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<meta http-equiv=\"refresh\" content=\"5\">"
           "<title>esp32-room-sensors</title>"
           "<style>body{font:16px/1.5 system-ui,sans-serif;margin:2rem;max-width:30rem}"
           "td{padding:.15rem 1rem .15rem 0}td:first-child{color:#555}</style>"
           "</head><body><h1>esp32-room-sensors</h1><table>"
           "<tr><td>Temperature</td><td>%s &deg;C</td></tr>"
           "<tr><td>Humidity</td><td>%s %%</td></tr>"
           "<tr><td>Pressure</td><td>%s hPa</td></tr>"
           "<tr><td>Reading age</td><td>%s</td></tr>"
           "<tr><td>Time</td><td>%s</td></tr>"
           "<tr><td>SD card</td><td>%s</td></tr>"
           "<tr><td>Recording</td><td>%s</td></tr>"
           "<tr><td>Uptime</td><td>%lu s</td></tr>"
           "</table><p><a href=\"/api/status\">/api/status</a></p></body></html>\n",
           temp, hum, pres, age, clock, sd_card == 1 ? "ok" : "unavailable",
           record_streams ? "recording" : "idle", (unsigned long)(millis() / 1000UL));

  status_server.sendHeader("Cache-Control", "no-store");
  status_server.send(200, "text/html", page);
}

static void handle_not_found(void) {
  status_server.send(404, "text/plain", "not found\n");
}

static void ap_start(void) {
  WiFi.mode(WIFI_AP_STA);
#if AP_ALWAYS_ON
  WiFi.softAPConfig(AP_ADDRESS, AP_ADDRESS, AP_MASK);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS)) {
    Serial.printf("[ap] \"%s\" up, channel %d, http://%s/\n", AP_SSID, AP_CHANNEL,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[ap] softAP() failed; status page on the STA address only");
  }
#else
  Serial.println("[ap] disabled (AP_ALWAYS_ON=0); status page on the STA address only");
#endif
}

// Own task: WebServer::handleClient() can block for seconds on a half-open
// client. On its own task that only stalls the status page, never the loggers.
static void status_server_loop(void *pvParameters) {
  (void)pvParameters;
  Serial.printf("[web] status task on core %d\n", xPortGetCoreID());
  for (;;) {
    status_server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void status_server_begin(void) {
  snapshot_lock = xSemaphoreCreateMutex();
  if (snapshot_lock == NULL) {
    Serial.println("[web] snapshot mutex not created; sensors will report null");
  }

  ap_start();

  status_server.on("/", handle_root);
  status_server.on("/api/status", handle_api_status);
  status_server.onNotFound(handle_not_found);
  status_server.begin();
  Serial.println("[web] http server on port 80");

  if (xTaskCreatePinnedToCore(status_server_loop, "status_server", 8192, NULL, 1,
                              &StatusServerTask, 1) != pdPASS) {
    StatusServerTask = NULL;
    Serial.println("[web] ERROR: status_server task not created");
  }
}
