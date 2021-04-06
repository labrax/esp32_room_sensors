#ifndef TIMEFUNC_H
#define TIMEFUNC_H
#include <WiFi.h>
#include <RTClib.h> //RTC: DS3231 (raspberry pi-like adapter)
#include "wifi_pass.h"

extern RTC_DS3231 rtc;

extern int rtc_start; //start success
extern const char* ssid; //wifi settings
extern const char* password;
extern const char* ntpServer; //ntp settings
extern const long  gmtOffset_sec;
extern const int   daylightOffset_sec;
extern struct tm a0, a1; //current collected time
extern struct tm * current_tm;

void waitWifiLoop();
void updateTime();
void updateTimeFromRTC();
void printLocalTime();
void rtc_prepare();
void tick_time_update();

#endif
