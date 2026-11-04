# ESP32 room sensors

Firmware and hardware for a WiFi-connected ESP32 room-sensor node.

Sensors: BME280 (temperature, humidity, pressure), photoresistor, dust sensor,
PIR, MPU-6050 accelerometer, and INMP441 I2S microphone. Data is logged to an
SD card; a DS3231 RTC provides timestamps; an optional web server exposes the
readings.

## Repository layout

| Path | Contents |
|---|---|
| `src/esp32_room_sensors/` | Arduino sketch and firmware sources |
| `case/` | Parametric OpenSCAD case, STLs, previews; see [`case/README.md`](case/README.md) |
| `pcb/` | Gerber/drill files and source archive; see [`pcb/README.md`](pcb/README.md) |
| `tools/` | Gerber preview/raster helpers |

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

## Hardware

- **Case:** [`case/README.md`](case/README.md) covers dimensions, printing,
  assembly, sensor openings, and the PIR variants.
- **PCB:** [`pcb/README.md`](pcb/README.md) covers the Gerber/drill files and
  the EasyEDA order note.
- The case was fitted to the PCB outline, mounting holes, and component
  positions. No PCB layout changes were made.
