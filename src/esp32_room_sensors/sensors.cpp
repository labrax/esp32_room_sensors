#include "sensors.h"

Adafruit_BME280 bme;

int photoValue = 0, pirValue = 0, dustValue = 0;
float temperature, humidity, pressure;
