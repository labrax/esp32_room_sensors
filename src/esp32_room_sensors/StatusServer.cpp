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
#include "WifiConfig.h"
#include "defines.h"
#include "wifi_pass.h"

// WPA2 needs 8+ characters. Fail the build rather than ship a weaker AP.
static_assert(sizeof(AP_PASSWORD) - 1 >= 8, "AP_PASSWORD must be at least 8 characters (WPA2)");
// softAP() and the status form both cap the SSID at 32 and the PSK at 63 bytes.
static_assert(sizeof(AP_SSID) - 1 <= 32, "AP_SSID must be at most 32 bytes");
static_assert(sizeof(AP_PASSWORD) - 1 <= 63, "AP_PASSWORD must be at most 63 bytes");

static const IPAddress AP_ADDRESS(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static WebServer status_server(80);
static SemaphoreHandle_t snapshot_lock = NULL;
static SensorSnapshot snapshot = {0.0f, 0.0f, 0.0f, false, 0};
static TaskHandle_t StatusServerTask = NULL;

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

static void fmt_float(char *dst, size_t len, bool valid, float value, const char *missing) {
  if (valid) {
    snprintf(dst, len, "%.1f", value);
  } else {
    snprintf(dst, len, "%s", missing);
  }
}

static void fmt_utc(char *dst, size_t len, bool valid, time_t now) {
  if (!valid) {
    snprintf(dst, len, "null");
    return;
  }
  struct tm t;
  gmtime_r(&now, &t);
  char iso[24];
  strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
  snprintf(dst, len, "\"%s\"", iso);
}

// Both escapers report failure instead of truncating, so callers can answer
// 500 rather than serve half a page.
static bool html_escape(char *dst, size_t len, const char *src) {
  size_t used = 0;
  for (const char *p = src; *p != '\0'; ++p) {
    const char *repl = NULL;
    switch (*p) {
      case '&': repl = "&amp;"; break;
      case '<': repl = "&lt;"; break;
      case '>': repl = "&gt;"; break;
      case '"': repl = "&quot;"; break;
      case '\'': repl = "&#39;"; break;
      default: break;
    }
    size_t need = repl != NULL ? strlen(repl) : 1;
    if (used + need >= len) {
      if (len > 0) {
        dst[used] = '\0';
      }
      return false;
    }
    memcpy(dst + used, repl != NULL ? repl : p, need);
    used += need;
  }
  dst[used] = '\0';
  return true;
}

static bool json_escape(char *dst, size_t len, const char *src) {
  static const char hexdigits[] = "0123456789abcdef";
  size_t used = 0;
  for (const unsigned char *p = (const unsigned char *)src; *p != '\0'; ++p) {
    char out[6];
    size_t need = 1;
    if (*p == '"' || *p == '\\') {
      out[0] = '\\';
      out[1] = (char)*p;
      need = 2;
    } else if (*p < 0x20) {
      out[0] = '\\';
      out[1] = 'u';
      out[2] = '0';
      out[3] = '0';
      out[4] = hexdigits[*p >> 4];
      out[5] = hexdigits[*p & 0x0f];
      need = 6;
    } else {
      out[0] = (char)*p;
    }
    if (used + need >= len) {
      if (len > 0) {
        dst[used] = '\0';
      }
      return false;
    }
    memcpy(dst + used, out, need);
    used += need;
  }
  dst[used] = '\0';
  return true;
}

// A restart is scheduled rather than run inside the handler, so the response
// reaches the client first.
static bool restart_pending = false;
static uint32_t restart_at_ms = 0;

static void handle_api_status(void) {
  SensorSnapshot s = snapshot_copy();
  time_t now = time(NULL);

  char temp[12], hum[12], pressure[12], utc[32], ssid[200] = "";
  fmt_float(temp, sizeof(temp), s.valid, s.temperature_c, "null");
  fmt_float(hum, sizeof(hum), s.valid, s.humidity_pct, "null");
  fmt_float(pressure, sizeof(pressure), s.valid, s.pressure_hpa, "null");
  fmt_utc(utc, sizeof(utc), time_valid, now);

  status_server.sendHeader("Cache-Control", "no-store");
  // 32 SSID bytes escape to at most 192 characters, so these buffers are the
  // only dynamic part that can fail; treat that as a server error.
  if (wifi_config_is_configured() &&
      !json_escape(ssid, sizeof(ssid), wifi_config_ssid())) {
    status_server.send(500, "text/plain", "status unavailable\n");
    return;
  }

  static char body[800]; // single status task; keeps the buffer off its stack
  int n = snprintf(body, sizeof(body),
                   "{\"uptime_s\":%lu,\"temperature_c\":%s,\"humidity_pct\":%s,"
                   "\"pressure_hpa\":%s,\"sensors_valid\":%s,\"sensor_age_ms\":%lu,"
                   "\"time_source\":\"%s\",\"time_valid\":%s,\"rtc_present\":%s,"
                   "\"time_utc\":%s,"
                   "\"sd_ok\":%s,\"recording\":%s,\"sta_connected\":%s,"
                   "\"sta_ip\":\"%s\",\"ap_ip\":\"%s\",\"ap_clients\":%u,"
                   "\"sta_ssid\":\"%s\",\"sta_provisioned\":%s}\n",
                   (unsigned long)(millis() / 1000UL), temp, hum, pressure,
                   s.valid ? "true" : "false",
                   s.valid ? (unsigned long)(millis() - s.updated_ms) : 0UL,
                   time_source_name(), time_valid ? "true" : "false",
                   rtc_present ? "true" : "false", utc,
                   sd_card == 1 ? "true" : "false", record_streams ? "true" : "false",
                   WiFi.status() == WL_CONNECTED ? "true" : "false",
                   WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str(),
                   (unsigned)WiFi.softAPgetStationNum(), ssid,
                   wifi_config_is_provisioned() ? "true" : "false");

  if (n < 0 || (size_t)n >= sizeof(body)) {
    status_server.send(500, "text/plain", "status unavailable\n");
    return;
  }
  status_server.send(200, "application/json", body);
}

static void handle_root(void) {
  SensorSnapshot s = snapshot_copy();
  time_t now = time(NULL);

  char temp[12], hum[12], pressure[12], age[16], clock[48];
  fmt_float(temp, sizeof(temp), s.valid, s.temperature_c, "-");
  fmt_float(hum, sizeof(hum), s.valid, s.humidity_pct, "-");
  fmt_float(pressure, sizeof(pressure), s.valid, s.pressure_hpa, "-");
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
    snprintf(clock, sizeof(clock), "%s (%s)", iso, time_source_name());
  } else if (time_source == TimeSource::Compile) {
    snprintf(clock, sizeof(clock), "compile (approximate)");
  } else {
    snprintf(clock, sizeof(clock), "unset");
  }

  const char *active_ssid = wifi_config_ssid();
  bool configured = wifi_config_is_configured();
  // 32 SSID bytes become at most 192 escaped characters; 200 always fits.
  char target[200];
  char ssid_value[200] = "";
  char conn[64];
  const char *source =
      !configured ? "not configured (use the form)"
                  : (wifi_config_is_provisioned() ? "saved on this device (NVS)"
                                                  : "wifi_pass.h fallback");

  if (!configured || !html_escape(target, sizeof(target), active_ssid)) {
    snprintf(target, sizeof(target), "not configured");
  } else {
    snprintf(ssid_value, sizeof(ssid_value), "%s", target);
  }
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(conn, sizeof(conn), "connected, IP %s",
             WiFi.localIP().toString().c_str());
  } else {
    snprintf(conn, sizeof(conn), "not connected");
  }

  static char page[5120]; // single status task; keeps the page off its stack
  int n = snprintf(
      page, sizeof(page),
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>esp32-room-sensors</title><style>"
      "*{box-sizing:border-box}"
      "body{margin:0;padding:1rem 1rem 2.5rem;font:16px/1.55 system-ui,-apple-system,"
      "\"Segoe UI\",sans-serif;color:#1b1f23;background:#f4f5f7}"
      "main{max-width:34rem;margin:0 auto}"
      ".bar{display:flex;align-items:flex-start;justify-content:space-between;gap:1rem;"
      "margin:0 0 1.25rem}"
      "h1{margin:0;font-size:1.3rem;letter-spacing:-.01em}"
      ".sub{margin:.2rem 0 0;color:#5f6b76;font-size:.85rem}"
      ".pill{flex:none;padding:.5rem .9rem;border:1px solid #d5d9de;border-radius:999px;"
      "background:#fff;color:#1b1f23;text-decoration:none;font-size:.9rem;font-weight:600}"
      "section{background:#fff;border:1px solid #e2e5e9;border-radius:12px;"
      "padding:1rem 1.05rem;margin:0 0 1rem}"
      "h2{margin:0 0 .7rem;font-size:.75rem;font-weight:700;letter-spacing:.08em;"
      "text-transform:uppercase;color:#5f6b76}"
      "dl{display:grid;grid-template-columns:auto 1fr;gap:.35rem 1rem;margin:0}"
      "dt{color:#5f6b76}dd{margin:0;font-variant-numeric:tabular-nums}"
      ".field{margin:0 0 .9rem}"
      "label{display:block;margin:0 0 .35rem;font-size:.9rem;font-weight:600}"
      "input{display:block;width:100%%;min-height:46px;padding:.6rem .7rem;font:inherit;"
      "color:inherit;background:#fff;border:1px solid #ccd2d8;border-radius:9px}"
      "input:focus{outline:2px solid #0f766e;outline-offset:1px;border-color:#0f766e}"
      ".hint{margin:.35rem 0 0;font-size:.82rem;color:#5f6b76}"
      "button{display:block;width:100%%;min-height:48px;padding:.7rem 1rem;font:inherit;"
      "font-weight:700;color:#fff;background:#0f766e;border:0;border-radius:9px;cursor:pointer}"
      "button:active{background:#0b5d56}"
      "button:focus-visible{outline:2px solid #1b1f23;outline-offset:2px}"
      ".note{margin:1rem 0 0;padding-top:.9rem;border-top:1px solid #e2e5e9;"
      "font-size:.85rem;color:#5f6b76}"
      ".foot{margin:1.25rem 0 0;text-align:center;font-size:.85rem}"
      "a{color:#0f766e}"
      "a:focus-visible{outline:2px solid #0f766e;outline-offset:2px}"
      "</style></head><body><main>"
      "<div class=\"bar\"><div><h1>esp32-room-sensors</h1>"
      "<p class=\"sub\">Status is a snapshot; refresh to update.</p></div>"
      "<a class=\"pill\" href=\"/\">Refresh</a></div>"
      "<section><h2>Readings</h2><dl>"
      "<dt>Temperature</dt><dd>%s &deg;C</dd>"
      "<dt>Humidity</dt><dd>%s %%</dd>"
      "<dt>Pressure</dt><dd>%s hPa</dd>"
      "<dt>Reading age</dt><dd>%s</dd></dl></section>"
      "<section><h2>Device</h2><dl>"
      "<dt>Time</dt><dd>%s</dd>"
      "<dt>RTC</dt><dd>%s</dd>"
      "<dt>SD card</dt><dd>%s</dd>"
      "<dt>Recording</dt><dd>%s</dd>"
      "<dt>Uptime</dt><dd>%lu s</dd></dl></section>"
      "<section><h2>WiFi network</h2><dl>"
      "<dt>Target</dt><dd>%s</dd>"
      "<dt>Source</dt><dd>%s</dd>"
      "<dt>Connection</dt><dd>%s</dd></dl></section>"
      "<section><h2>Change network</h2>"
      "<form method=\"post\" action=\"/api/wifi\">"
      "<div class=\"field\"><label for=\"ssid\">SSID</label>"
      "<input id=\"ssid\" name=\"ssid\" type=\"text\" value=\"%s\" required maxlength=\"32\""
      " autocapitalize=\"none\" autocorrect=\"off\" spellcheck=\"false\"></div>"
      "<div class=\"field\"><label for=\"password\">WPA2 password</label>"
      "<input id=\"password\" name=\"password\" type=\"password\" required minlength=\"8\""
      " maxlength=\"63\" autocomplete=\"new-password\" aria-describedby=\"pw-hint\">"
      "<p class=\"hint\" id=\"pw-hint\">8-63 printable ASCII characters. The saved password is never shown.</p></div>"
      "<button type=\"submit\">Save and restart</button></form>"
      "<p class=\"note\">Saving stores the network and restarts the device. "
      "The network is used for time sync when needed. The access point stays up. "
      "This page has no login; anyone who can reach it on the AP or the LAN can "
      "change the network.</p></section>"
      "<p class=\"foot\"><a href=\"/api/status\">/api/status</a></p>"
      "</main></body></html>\n",
      temp, hum, pressure, age, clock, rtc_present ? "present" : "not fitted",
      sd_card == 1 ? "ok" : "unavailable",
      record_streams ? "recording" : "idle", (unsigned long)(millis() / 1000UL),
      target, source, conn, ssid_value);

  status_server.sendHeader("Cache-Control", "no-store");
  if (n < 0 || (size_t)n >= sizeof(page)) {
    status_server.send(500, "text/plain", "status page too large\n");
    return;
  }
  status_server.send(200, "text/html", page);
}

// Minimal result page for the POST and for internal errors. `message` is an
// HTML fragment: callers escape every value they put in it.
static void send_simple_page(int code, const char *title, const char *message) {
  static char page[1280]; // single status task
  int n = snprintf(page, sizeof(page),
                   "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                   "<title>%s</title><style>"
                   "*{box-sizing:border-box}"
                   "body{margin:0;padding:2rem 1rem;font:16px/1.55 system-ui,-apple-system,"
                   "\"Segoe UI\",sans-serif;color:#1b1f23;background:#f4f5f7}"
                   "main{max-width:34rem;margin:0 auto;background:#fff;border:1px solid #e2e5e9;"
                   "border-radius:12px;padding:1.2rem}"
                   "h1{margin:0 0 .6rem;font-size:1.2rem}"
                   "p{margin:0 0 1rem}"
                   "a{color:#0f766e}"
                   "</style></head><body><main><h1>%s</h1><p>%s</p>"
                   "<p><a href=\"/\">Back to status</a></p></main></body></html>\n",
                   title, title, message);
  status_server.sendHeader("Cache-Control", "no-store");
  if (n < 0 || (size_t)n >= sizeof(page)) {
    status_server.send(500, "text/plain", "response too large\n");
    return;
  }
  status_server.send(code, "text/html", page);
}

static void handle_api_wifi(void) {
  String ssid = status_server.arg("ssid");
  String password = status_server.arg("password");
  const char *error = NULL;

  if (!wifi_config_save(ssid.c_str(), password.c_str(), &error)) {
    char escaped[160];
    if (error == NULL || !html_escape(escaped, sizeof(escaped), error)) {
      snprintf(escaped, sizeof(escaped), "Credentials were rejected.");
    }
    send_simple_page(400, "WiFi not saved", escaped);
    return;
  }

  // Saved for the next boot; the restart below applies it. Never echo the
  // password, and never log it.
  char escaped_ssid[200];
  char message[384];
  if (!html_escape(escaped_ssid, sizeof(escaped_ssid), ssid.c_str())) {
    snprintf(escaped_ssid, sizeof(escaped_ssid), "the new network");
  }
  snprintf(message, sizeof(message),
           "Saved <b>%s</b>. Restarting to apply it; the device uses this network "
           "for time sync when needed. The access point stays up.",
           escaped_ssid);
  send_simple_page(200, "WiFi saved", message);

  Serial.printf("[wifi] saved \"%s\" from the AP form; restarting\n", ssid.c_str());
  restart_at_ms = millis() + 1800;
  restart_pending = true;
}

static void handle_not_found(void) {
  status_server.send(404, "text/plain", "not found\n");
}

static void ap_start(void) {
#if AP_ALWAYS_ON
  // Start in AP+STA so the AP is up before init_time(); try_ntp() keeps this
  // mode when it associates the STA for NTP.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_ADDRESS, AP_ADDRESS, AP_MASK);
  if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS)) {
    Serial.printf("[ap] \"%s\" up, channel %d, http://%s/\n", AP_SSID, AP_CHANNEL,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[ap] softAP() failed; no AP, so the status page and form are unavailable");
  }
#else
  Serial.println("[ap] disabled (AP_ALWAYS_ON=0); no AP, so the status page and form are unavailable");
#endif
}

// Own task: WebServer::handleClient() can block for seconds on a half-open
// client. On its own task that only stalls the status page, never the loggers.
static void status_server_loop(void *pvParameters) {
  (void)pvParameters;
  Serial.printf("[web] status task on core %d\n", xPortGetCoreID());
  for (;;) {
    status_server.handleClient();
    // Deferred so the saved/restart response is sent before the link drops.
    if (restart_pending && (int32_t)(millis() - restart_at_ms) >= 0) {
      Serial.println("[wifi] restarting to apply the saved network");
      ESP.restart();
    }
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
  status_server.on("/api/wifi", HTTP_POST, handle_api_wifi);
  status_server.onNotFound(handle_not_found);
  status_server.begin();
  Serial.println("[web] http server on port 80");

  if (xTaskCreatePinnedToCore(status_server_loop, "status_server", 8192, NULL, 1,
                              &StatusServerTask, 1) != pdPASS) {
    StatusServerTask = NULL;
    Serial.println("[web] ERROR: status_server task not created");
  }
}
