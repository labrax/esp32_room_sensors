#ifndef SENSORS_H
#define SENSORS_H
#include <Wire.h>
#include <Adafruit_Sensor.h> //BME280
#include <Adafruit_BME280.h> //BME280
#include <driver/adc.h> //photoresistor; dust sensor

// sensors
extern Adafruit_BME280 bme;

// stored sensor data
extern int photoValue, pirValue, dustValue;
extern float temperature, humidity, pressure;
#endif
