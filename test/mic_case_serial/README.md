# HS-3 serial WAV recorder

Standalone ESP32 + INMP441 recorder for the HS-3 case test. It streams raw
16-bit PCM over the USB serial link instead of writing to an SD card.

- **Format:** 32 kHz, 16-bit, mono, 10 s per run.
- **Baud:** 921600 (64 kB/s audio; 921600 8N1 carries about 92 kB/s).
- **Host:** `tools/serial_wav.py` (requires `pyserial`).
- **No SD card** is used.

## Wiring

Match `src/esp32_room_sensors/defines.h` and the project board:

| Signal | ESP32 |
|---|---|
| INMP441 VDD | 3.3 V |
| INMP441 GND | GND |
| INMP441 L/R | board-dependent; the project board ties it high → the sketch records the right channel. If your module ties L/R low, change `ONLY_RIGHT` to `ONLY_LEFT`. |
| I2S SCK / BCLK | GPIO14 |
| I2S WS / LRCLK | GPIO15 |
| I2S SD / DOUT | GPIO4 |
| Record button | GPIO13 -> 3V3 (active high) |
| Status LED | GPIO2 |

## Flash

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 test/mic_case_serial
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 test/mic_case_serial
```

The sketch prints `HSA_READY` every 2 s while idle. The host sends `S` to start
a run; the device replies with `HSA1` + a 12-byte header + 640,000 bytes of PCM.
The LED is on while streaming.

## Capture

```sh
python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/hs3_serial/A1.wav
```

For three repeats without changing the setup:

```sh
python3 tools/serial_wav.py --port /dev/ttyUSB0 --out /tmp/opencode/hs3_serial/A --count 3
```

The script waits for `HSA_READY`, sends `S`, waits for the 1 s pre-roll after
the header, and prints `Recording ... start playback now`. Start the sweep when
that line appears. Each run is 10 s; files are `A_000.wav`, `A_001.wav`, ...

If the port was already open or the board did not reset, the script can still
send `S`; the sketch is waiting. If no header arrives, reset the board and run
the script again.

## Caveats

- The serial link must stay connected for the whole run.
- Bootloader/boot text is at 115200, so the host may see garbage before
  `HSA_READY`; the script scans for the marker.
- The device prints nothing during the PCM stream.
- UART is fine for this relative A/B test; SD is more robust for long logs.
- The sketch is compile-verified; check the I2S channel, `>>16` scaling, and
  button polarity on the real board during the first run.
