#ifndef DEFINES_H
#define DEFINES_H

// The old WEBSERVER page is gone (HS-2 D3, requalified by HS-12). The status
// page lives in StatusServer.*.

// WiFi AP (HS-12). Credentials are AP_SSID/AP_PASSWORD in wifi_pass.h; see
// wifi_pass.example.h.
// 1 = AP up for the whole runtime (default, user request).
// 0 = no AP; the status page is then reachable on the STA address only.
// The AP radio heats the board next to the BME280. The bias is unmeasured on
// this bench (see docs/architecture.md 5.1); treat absolute temperature as
// indicative while the AP is on.
#ifndef AP_ALWAYS_ON
#define AP_ALWAYS_ON 1
#endif
#define AP_CHANNEL 1     // requested; in AP+STA the AP follows the STA channel
#define AP_MAX_CLIENTS 4 // ESP32 softAP limit

//status
#define statusLED 2
extern unsigned char record_streams; // store information if the mic / gps data should be recorded

//sensors
#define photoPin ADC1_CHANNEL_6
#define dustPin ADC1_CHANNEL_4
#define pirPin 35
#define SD_CARD_CS_PIN 5
#define dustLEDPin 33

//delays on data collection
#define SENSOR_TIC_SEC 300
#define SENSOR_PIR_TIC_SEC 1

//audio defines
#include <freertos/FreeRTOS.h>
#include <driver/i2s.h>
#define STREAMS_ENABLE_DISABLE_PIN 13
#define I2S_SCK 14 // ports
#define I2S_WS 15
#define I2S_SD 4
#define I2S_PORT I2S_NUM_0 //15; 14 (4)
#define I2S_SAMPLE_RATE (16000) //44100
#define I2S_SAMPLE_BITS (16) //32
#define I2S_CHANNEL_NUM (1) //only left
#define RECORD_TIME (120)
#define HEADER_SIZE 44
#define SAMPLES 1024
#define I2S_READ_LEN (I2S_SAMPLE_BITS * SAMPLES)
#define BLOCK_SIZE SAMPLES
#define OCTAVES 9

//accel define
#define ACCEL_FREQ 10 // Hz

#define SKIP_RTC true

#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)

// A-weighting curve from 31.5 Hz ... 8000 Hz
extern const double aweighting[9];
#endif
