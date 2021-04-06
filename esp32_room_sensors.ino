#include <time.h>

#include "ServerFunctions.h"
#include "sensors.h"
#include "Mic.h" //mic: useful functions
#include "defines.h" //port definitions
#include "SDData.h"
#include "TimeFunctions.h"

//task handlers
TaskHandle_t DataTask;
TaskHandle_t DataPIRTask;
TaskHandle_t DataAudioTask;

void sensor_data_loop(void * pvParameters) {
  Serial.print("sensor_data_loop() running on core ");
  Serial.println(xPortGetCoreID());
  while(true) {
    delay(1000);
    // collect sensor data
    photoValue = adc1_get_raw(photoPin);
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;

    delay(1000);
    //dust
    digitalWrite(dustLEDPin, HIGH); // power on the LED
    delayMicroseconds(280);
    dustValue = adc1_get_raw(dustPin);
    delayMicroseconds(40);
    digitalWrite(dustLEDPin, LOW); // turn the LED off
    delayMicroseconds(9680);
  
    // output data
    time_to_file(&data_output);
    Serial.printf("%d,%d,%f,%f,%f\n", dustValue, photoValue, temperature, humidity, pressure);
    if(sd_card == 1) {
      write_n = data_output.printf("%d,%d,%f,%f,%f\n", dustValue, photoValue, temperature, humidity, pressure);
      if(!write_n) {
        sd_card = 0;
      }
      data_output.flush();
    }
    delay(SENSOR_TIC_SEC*1000 - 2010);
  }
}

void sensor_pir_data_loop(void * pvParameters) {
  static int npir = 0;
  Serial.print("sensor_pir_data_loop() running on core ");
  Serial.println(xPortGetCoreID());
  while(true) {
    npir = digitalRead(pirPin);

    if(npir != pirValue) {
      pirValue = npir;
      time_to_file(&data_pir_output);
      Serial.printf("%d\n", pirValue);
      if(sd_card == 1) {
        write_n = data_pir_output.printf("%d\n", pirValue);
        if(!write_n) {
          sd_card = 0;
        }
        data_output.flush();
      }
    }
    delay(SENSOR_PIR_TIC_SEC);
  }
}

void audio_loop(void * pvParameters) {
  Serial.print("audio_loop() running on core ");
  Serial.println(xPortGetCoreID());
  while(true) {
    //delay(1000);
    collect_audio_data();
    time_to_file(&data_audio_output);
    for (int i = 0; i < OCTAVES; i++) {
        //print_level(energy[i]);
        Serial.printf("%d,", (int) energy[i]);

        if(sd_card == 1) {
          write_n = data_audio_output.printf("%d,", (int) energy[i]);
          if(!write_n) {
            sd_card = 0;
          }
        }
    }
    Serial.printf("%.1f\n", loudness);
    if(sd_card == 1) {
      write_n = data_audio_output.printf("%.1f\n", loudness);
      if(!write_n) {
        sd_card = 0;
      }
      data_audio_output.flush();
    }
    //print("\n");
    //!audio
  }
}

void setup() {
  //status info
  pinMode(statusLED, OUTPUT);
  digitalWrite(statusLED, HIGH);
  
  Serial.begin(115200);

  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);

  rtc_prepare();
  Serial.printf("rtc = %d\n", rtc_start);

  #ifndef WEBSERVER
  time_to_file(NULL);
  Serial.println("disconnecting wifi");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
  #endif

  time_to_file(NULL);
  Serial.println("starting sensors");
  // sensor setup
  pinMode(dustLEDPin, OUTPUT);
  pinMode(pirPin, INPUT);
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_config_channel_atten(photoPin, ADC_ATTEN_DB_11);
  adc1_config_channel_atten(dustPin, ADC_ATTEN_DB_11);
  bme.begin(0x76);

  start_sd();
  start_audio();

  #ifdef WEBSERVER
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  time_to_file(NULL);
  Serial.println("HTTP server started");
  #endif

  time_to_file(NULL);
  Serial.println("starting threads");
  //separate thread runnings
  xTaskCreatePinnedToCore(
        audio_loop, /* Function to implement the task */
        "audio_loop", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &DataAudioTask,  /* Task handle. */
        0); /* Core where the task should run */

  xTaskCreatePinnedToCore(
        sensor_data_loop, /* Function to implement the task */
        "sensor_loop", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &DataTask,  /* Task handle. */
        1); /* Core where the task should run */

  xTaskCreatePinnedToCore(
        sensor_pir_data_loop, /* Function to implement the task */
        "sensor_pir_loop", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        1,  /* Priority of the task */ //higher priority than normal data
        &DataPIRTask,  /* Task handle. */
        1); /* Core where the task should run */

  //status info
  digitalWrite(statusLED, LOW); // power off the LED
}

void loop()
{
  delay(10000);
  #ifdef WEBSERVER
  server.handleClient();
  #endif

  if(sd_card == 0) {
    Serial.println("Invalid card status, restarting");
    digitalWrite(statusLED, HIGH); // power on the LED
    delay(10000);
    Serial.println("Invalid card status, restarting");
    ESP.restart();
  }
}
