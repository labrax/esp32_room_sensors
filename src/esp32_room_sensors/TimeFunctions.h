#ifndef TIMEFUNC_H
#define TIMEFUNC_H
#include <WiFi.h>
#include <RTClib.h> //RTC: DS3231 (raspberry pi-like adapter)
#include "wifi_pass.h"
#include "defines.h"

enum class TimeSource {
  None,
  RTC,
  NTP,
  Compile
};

extern RTC_DS3231 rtc;
extern bool rtc_present;
extern TimeSource time_source;
extern bool time_valid;

// Boot sequence: probe RTC, fall back to bounded NTP, then compile time.
void init_time();
// Re-read the RTC when it is the active source (called periodically).
void service_time();
void updateTimeFromRTC();

// UTC formatting helpers. `format_timestamp` -> "YYYYMMDD,HH:MM:SS".
void format_timestamp(char *buf, size_t len);
// `format_filename_timestamp` -> "YYYYMMDD_HHMMSS".
void format_filename_timestamp(char *buf, size_t len);
// `format_boot_id` -> "YYYYMMDDTHHMMSSZ-xxxxxx".
void format_boot_id(char *buf, size_t len);

const char *time_source_name();
const char *reset_reason_name();

long current_time();

#endif
