#include "TimeFunctions.h"

#include <Wire.h>
#include <esp_system.h>
#include <string.h>
#include <time.h>

RTC_DS3231 rtc;
bool rtc_present = false;
TimeSource time_source = TimeSource::None;
bool time_valid = false;

const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* ntpServer = "pool.ntp.org";

static unsigned long last_rtc_read_ms = 0;

static bool rtc_time_in_range(const DateTime &now) {
  return now.year() >= RTC_MIN_YEAR && now.year() <= RTC_MAX_YEAR;
}

static void set_system_time(struct tm tm) {
  tm.tm_isdst = 0;
  time_t t = mktime(&tm);
  struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&tv, NULL);
}

static bool wifi_credentials_configured() {
  return strcmp(WIFI_SSID, "YOUR_SSID") != 0 && strlen(WIFI_SSID) > 0;
}

// HS-12: with an always-on AP the radio must stay up. Without it, HS-4's
// original policy applies: drop STA after the sync and turn WiFi off.
// Keep the decision in one place so the time logic stays policy-free.
static void apply_post_sync_wifi_policy() {
#if AP_ALWAYS_ON
  // AP+STA stays up; STA remains associated for future NTP use.
  Serial.println("[wifi] AP_ALWAYS_ON: keeping AP+STA up after the time sync.");
#else
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
#endif
}

// Compile-time fallback: ordered but approximate UTC. time_valid stays false.
static void set_compile_time() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int day = 1, year = 1970, hh = 0, mm = 0, ss = 0;

  if (sscanf(__DATE__, "%3s %d %d", mon, &day, &year) != 3) {
    return;
  }
  if (sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss) != 3) {
    return;
  }

  const char *p = strstr(months, mon);
  int month = p ? (int)(p - months) / 3 + 1 : 1;

  struct tm tm = {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hh;
  tm.tm_min = mm;
  tm.tm_sec = ss;
  tm.tm_isdst = 0;
  set_system_time(tm);
}

void updateTimeFromRTC() {
  DateTime now = rtc.now();
  struct tm tm = {};
  tm.tm_year = now.year() - 1900;
  tm.tm_mon = now.month() - 1;
  tm.tm_mday = now.day();
  tm.tm_hour = now.hour();
  tm.tm_min = now.minute();
  tm.tm_sec = now.second();
  tm.tm_isdst = 0;

  set_system_time(tm);
  Serial.printf("System time set from RTC: %04d%02d%02d,%02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
}

// Returns true when NTP set the system clock. Bounded: connect 15 s, sync 10 s.
static bool try_ntp() {
  if (!wifi_credentials_configured()) {
    Serial.println("WiFi credentials are placeholders; skipping NTP.");
    return false;
  }

  Serial.printf("Connecting to WiFi %s (max %lu s)...\n", ssid,
                WIFI_CONNECT_TIMEOUT_MS / 1000UL);
#if AP_ALWAYS_ON
  // Keep the softAP up while the STA associates; WiFi.mode() is a no-op when
  // the mode is already AP_STA.
  WiFi.mode(WIFI_AP_STA);
#else
  WiFi.mode(WIFI_STA);
#endif
  WiFi.setAutoReconnect(false);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  bool connected = WiFi.status() == WL_CONNECTED;
  bool synced = false;

  if (connected) {
    Serial.printf("WiFi connected in %lu ms; requesting NTP (max %lu s)...\n",
                  millis() - start, NTP_SYNC_TIMEOUT_MS / 1000UL);
    configTime(0, 0, ntpServer);
    struct tm tm = {};
    synced = getLocalTime(&tm, NTP_SYNC_TIMEOUT_MS);
    if (synced) {
      Serial.printf("NTP synced: %04d%02d%02d,%02d:%02d:%02d\n",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
      if (rtc_present) {
        rtc.adjust(DateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec));
        Serial.println("RTC adjusted from NTP.");
      }
    } else {
      Serial.println("NTP sync timed out.");
    }
  } else {
    Serial.printf("WiFi connect timed out after %lu ms.\n", millis() - start);
  }

  apply_post_sync_wifi_policy();
  return synced;
}

// The MPU6050 and the DS3231 both answer at 0x68. Read the MPU WHO_AM_I
// register first; if it answers with an MPU-family ID, the RTC probe must not
// touch the bus. Known IDs include 0x68 (MPU6050), 0x70/0x71/0x73
// (MPU6500/MPU9250), and clone-specific values such as 0x98. A DS3231 does not
// implement register 0x75; a read there returns 0x00 or 0xFF.
bool mpu6050_present_at_0x68() {
  Wire.beginTransmission(0x68);
  Wire.write(0x75); // WHO_AM_I
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(0x68, 1) != 1) {
    return false;
  }
  uint8_t who = Wire.read();
  return who != 0x00 && who != 0xFF;
}

void init_time() {
  setenv("TZ", "UTC0", 1);
  tzset();

  bool rtc_lost_power = false;
  if (mpu6050_present_at_0x68()) {
    rtc_present = false;
    Serial.println("MPU6050 at 0x68; RTC probe skipped (I2C address conflict).");
  } else {
    rtc_present = rtc.begin();
    if (!rtc_present) {
      Serial.println("RTC not found.");
    } else {
      rtc_lost_power = rtc.lostPower();
      DateTime now = rtc.now();
      Serial.printf("RTC time: %04d%02d%02d,%02d:%02d:%02d%s\n",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second(),
                    rtc_lost_power ? " (lost power)" : "");
    }
  }

  if (rtc_present && !rtc_lost_power && rtc_time_in_range(rtc.now())) {
    updateTimeFromRTC();
    time_source = TimeSource::RTC;
    time_valid = true;
    last_rtc_read_ms = millis();
#if AP_ALWAYS_ON
    Serial.println("Time source: rtc (AP stays up; no NTP needed).");
#else
    Serial.println("Time source: rtc (WiFi stays off).");
#endif
    return;
  }

  if (try_ntp()) {
    time_source = TimeSource::NTP;
    time_valid = true;
    Serial.println("Time source: ntp.");
    return;
  }

  set_compile_time();
  time_source = TimeSource::Compile;
  time_valid = false;
  Serial.println("Time source: compile (approximate, time_valid=false).");
}

void service_time() {
  if (time_source != TimeSource::RTC || !rtc_present) {
    return;
  }
  if ((millis() - last_rtc_read_ms) < RTC_REREAD_INTERVAL_MS) {
    return;
  }
  last_rtc_read_ms = millis();
  if (rtc_time_in_range(rtc.now())) {
    updateTimeFromRTC();
  }
}

static void format_utc(char *buf, size_t len, const char *fmt) {
  time_t now = time(NULL);
  struct tm tm = {};
  localtime_r(&now, &tm);
  strftime(buf, len, fmt, &tm);
}

void format_timestamp(char *buf, size_t len) {
  format_utc(buf, len, "%Y%m%d,%H:%M:%S");
}

void format_filename_timestamp(char *buf, size_t len) {
  format_utc(buf, len, "%Y%m%d_%H%M%S");
}

void format_boot_id(char *buf, size_t len) {
  char ts[24];
  format_utc(ts, sizeof(ts), "%Y%m%dT%H%M%SZ");
  snprintf(buf, len, "%s-%06X", ts, (unsigned int)(ESP.getEfuseMac() & 0xFFFFFF));
}

const char *time_source_name() {
  switch (time_source) {
    case TimeSource::RTC: return "rtc";
    case TimeSource::NTP: return "ntp";
    case TimeSource::Compile: return "compile";
    default: return "none";
  }
}

const char *reset_reason_name() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    default: return "unknown";
  }
}

long current_time() {
  time_t now;
  time(&now);
  return (long) now;
}
