# HS-3 mic case test recorder

Standalone ESP32 sketch for the HS-3 case test: records the INMP441 to a
10 s mono WAV on the SD card. No WiFi, no BME/MPU, no `arduinoFFT`, and no
main-firmware sources. Pins match `../../src/esp32_room_sensors/defines.h`.

- Sample rate: **32 kHz**, 16-bit mono PCM
- I2S: master RX, 32-bit slots, left channel (INMP441 L/R tied to GND)
- Files: `/mic_000.wav`, `/mic_001.wav`, ... (existing names are skipped)
- One recording starts 3 s after boot; the button starts another

## Wiring

| INMP441 | ESP32 | Note |
|---|---|---|
| VDD | 3V3 | **3.3 V only** |
| GND | GND | |
| L/R | GND | selects the left channel |
| SCK | GPIO14 | I2S bit clock |
| WS | GPIO15 | I2S word select |
| SD | GPIO4 | I2S data from the mic |

| microSD module | ESP32 | Note |
|---|---|---|
| CS | GPIO5 | `SD_CARD_CS_PIN` |
| SCK | GPIO18 | default SPI SCK |
| MISO | GPIO19 | default SPI MISO |
| MOSI | GPIO23 | default SPI MOSI |
| VCC | 3V3 or 5 V | follow the module; logic is 3.3 V |
| GND | GND | |

| Control | ESP32 | Note |
|---|---|---|
| Record button | GPIO13 -> 3V3 | active high, internal pulldown; the PCB KEY1 also works |
| Status LED | GPIO2 | on-board LED, active high |

Use short wires for I2S. Keep the mic module away from the SD card and the
ESP32 antenna. The INMP441 port must face the lid hole and stay unobstructed.

## SD card

- FAT32, 32 GB or smaller is the safe choice.
- Insert the card before powering up.
- At 32 kHz / 16-bit, each 10 s file is 640,044 bytes (about 640 KB).

## Flash

From the repository root, with ESP32 core `2.0.17` installed:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 test/mic_case
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 test/mic_case
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Change `/dev/ttyUSB0` to your port. The sketch also works from the Arduino
IDE if the folder is named `mic_case`.

## Recording procedure

1. Insert the SD card and power the board.
2. Wait for `SD ready` and `First recording starts in 3 s...`. The first file
   records automatically for 10 s.
3. Wait for `Saved /mic_000.wav: ...`. Copy or note the file name.
4. Change the case configuration, wait 2 s for the assembly to settle, then
   press the button once. The next file records for 10 s.
5. Do not move the board, mic, speaker, or cable between runs of a set.
6. Power cycle only between configuration sets. A power cycle starts a new
   automatic recording, so plan the file numbers.

Serial output per run:

```
Recording /mic_003.wav (320000 samples, 10.0 s)
  3%
  ...
Saved /mic_003.wav: peak -18.2 dBFS, RMS -37.5 dBFS
```

A peak near `0.0 dBFS` means clipping; reduce the speaker level and repeat
the whole set. A peak below about `-50 dBFS` is too quiet to compare.

## File naming

The sketch uses the first unused name in `/mic_000.wav` ... `/mic_999.wav`;
it never overwrites an existing file. Rename or copy files off the card after
each set and label them as described in `../../docs/mic_case_test.md`
(for example `B2_sweep.wav`).

## Notes

- The INMP441 has a fixed sensitivity; there is no gain control in this sketch.
  Keep the speaker level identical for every run.
- The 24-bit mic data is shifted right by 16 bits into the 16-bit WAV. The
  comparison script also accepts 32-bit mono WAVs.
- The level printed on serial is for a sanity check only; use
  `tools/mic_compare.py` for the actual comparison.
- Records until the card fills or 1000 file names are used.
