# ESP32 room sensors

Firmware and hardware for a WiFi-connected ESP32 room-sensor node.

Sensors: BME280 (temperature, humidity, pressure), photoresistor, dust sensor,
PIR, MPU-6050 accelerometer, and INMP441 I2S microphone. Data is logged to an
SD card when one is fitted; a DS3231 RTC provides timestamps; a local
access-point status page and JSON API expose live readings, including on a
board without a card.

## Repository layout

| Path | Contents |
|---|---|
| `src/esp32_room_sensors/` | Arduino sketch and firmware sources |
| `case/` | Parametric OpenSCAD case, STLs, previews; see [`case/README.md`](case/README.md) |
| `pcb/` | Gerber/drill files and source archive; see [`pcb/README.md`](pcb/README.md) |
| `tools/` | Gerber preview/raster, microphone comparison, sweep generator, serial WAV capture |
| `test/mic_case/` | Standalone INMP441 + SD WAV recorder for the HS-3 bench test |
| `test/mic_case_serial/` | Standalone INMP441 serial WAV recorder for HS-3 (no SD) |

## Firmware

Open `src/esp32_room_sensors/esp32_room_sensors.ino` in the Arduino IDE with
ESP32 board support installed. Keep the sketch folder named
`esp32_room_sensors` so the Arduino IDE accepts the `.ino` file.

Libraries: Adafruit BME280 (with Adafruit Unified Sensor), TinyMPU6050,
arduinoFFT, RTClib, SD, and the ESP32 core's WiFi, WebServer, FreeRTOS, and
I2S drivers.

Copy `src/esp32_room_sensors/wifi_pass.example.h` to
`src/esp32_room_sensors/wifi_pass.h` and fill in the network credentials.
`wifi_pass.h` is ignored by git.

At boot the board starts the `esp32-room-sensors` access point (WPA2, password
from `AP_PASSWORD`). Join it, open `http://192.168.4.1/`, and use the WiFi form
to save the router SSID and password. Saved credentials live in NVS and override
the `WIFI_SSID`/`WIFI_PASSWORD` fallback on the next boot; the device restarts
after saving and the AP stays up. The page has no login, so anyone who can reach
it on the AP or the LAN can change the network. `GET /api/status` is the JSON view;
`POST /api/wifi` is the form endpoint.

An SD card is optional. Without one the board boots and stays up; the CSV,
session, audio, and accelerometer files are skipped. The AP page and
`/api/status` report live BME280 readings plus the dust raw ADC, the light raw
ADC, the PIR input level, and MPU6050 acceleration. Audio recording still
requires an SD card.

## Hardware

- **Case:** [`case/README.md`](case/README.md) covers dimensions, printing,
  assembly, sensor openings, and the PIR variants.
- **Microphone bench test:** [`docs/mic_case_test.md`](docs/mic_case_test.md)
  is the HS-3 protocol for measuring the INMP441 with and without the lid.
  The standalone recorder is in [`test/mic_case/`](test/mic_case/README.md).
- **PCB:** [`pcb/README.md`](pcb/README.md) covers the Gerber/drill files and
  the EasyEDA order note.
- The case was fitted to the PCB outline, mounting holes, and component
  positions. No PCB layout changes were made.
