#include "TimeFunctions.h"

RTC_DS3231 rtc;
int rtc_start = 0;
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
struct tm a0, a1;
struct tm * current_tm=&a0;

void waitWifiLoop() {
  Serial.print("Waiting for WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
}

void updateTime() {
  struct tm timeinfo;
  Serial.print("Obtaining time");
  while(!getLocalTime(&timeinfo)) {
    delay(500);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print(".");
  }
  Serial.println(" TIME SYNCED");
  printLocalTime();
}

void updateTimeFromRTC() {
  DateTime now = rtc.now();
  struct tm tm;
  tm.tm_year = now.year() - 1900;
  tm.tm_mon = now.month() - 1;
  tm.tm_mday = now.day();
  tm.tm_hour = now.hour();
  tm.tm_min = now.minute();
  tm.tm_sec = now.second();

  time_t t = mktime(&tm);
  printf("Setting time: %s", asctime(&tm));
  struct timeval new_time = { .tv_sec = t };
  settimeofday(&new_time, NULL);
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.print("ERR Obtaining time");
    return;
  }
  Serial.print(&timeinfo, "%Y/%m/%d,%H:%M:%S");
}

void rtc_prepare() {
  if (! rtc.begin()) {
    rtc_start = 0;
    Serial.println("Couldn't find RTC");
  } else {
    rtc_start = 1;
  }

  if(rtc_start == 1) {
    DateTime now = rtc.now();
    Serial.printf("RTC time is: %04d%02d%02d,%02d:%02d:%02d\n", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    if(now.year() < 2023) {
      Serial.println("Updating time\n");
      waitWifiLoop();
      updateTime();
      
      struct tm t;
      if(!getLocalTime(&t)) {
        Serial.println("getLocalTime error - cannot update RTC time\n");
      } else {
        rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
      }
    } else {
      updateTimeFromRTC();
    }
  } else { //rtc_start == 0
    Serial.println("RTC clock not working, obtaining online time");
    waitWifiLoop();
    updateTime();
  }
}

void tick_time_update() {
  if(current_tm == &a0) {
    getLocalTime(&a1);
    current_tm = &a1;
  } else {
    getLocalTime(&a0);
    current_tm = &a0;
  }
}
