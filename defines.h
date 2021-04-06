#ifndef DEFINES_H
#define DEFINES_H

#define SEALEVELPRESSURE_HPA (1013.25)  // not used
//#define WEBSERVER

//status
#define statusLED 2

//sensors
#define photoPin ADC1_CHANNEL_6
#define dustPin ADC1_CHANNEL_4
#define pirPin 35
#define SD_CARD_CS_PIN 5
#define dustLEDPin 33

//sensor control
#define SENSOR_TIC_SEC 60
#define SENSOR_PIR_TIC_SEC 1

//audio defines
#include <FreeRTOS.h>
#include <driver/i2s.h>
#define I2S_PORT I2S_NUM_0 //15; 14 (4)
#define SAMPLES 1024
#define BLOCK_SIZE SAMPLES
#define OCTAVES 9

// A-weighting curve from 31.5 Hz ... 8000 Hz
extern const double aweighting[9];
#endif
