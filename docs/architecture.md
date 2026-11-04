# Architecture decision record: runtime, time, build, and repo layout

- **Status:** Proposed — pending review on HS-2
- **Date:** 2026-11-04
- **Baseline:** `main` at `94ade22` (`labrax/esp32_room_sensors`)
- **Scope:** firmware runtime and concurrency, boot/time, input, `WEBSERVER`,
  build/CI, repo/assets, case/docs
- **Not in scope:** implementing the fixes; PCB or case geometry changes;
  rewriting git history

This record resolves the findings in HS-2. Each finding is marked **confirmed**,
**corrected**, or **deferred**. Each architecture area has a chosen option, the
rejected alternatives, and the deferrals. The backlog maps every approved item
to a child ticket.

> **Update 2026-11-04:** HS-12 added a WiFi AP mode at boot. The "WiFi off
> after boot" part of D2 and the "delete `WEBSERVER`" decision in D3 were
> qualified by HS-12 and are implemented in section 5.1. The rest of this record
> stands.

No firmware, hardware, or case changes are made in HS-2. This document is the
only repository change.

---

## 1. Decisions at a glance

| Area | Decision |
|---|---|
| Runtime | Keep the five FreeRTOS tasks. Add one SD mutex and one I2C mutex. Use atomics for flags. Each task owns its files and opens/closes them. No storage queue yet. |
| Time | Probe the DS3231 at runtime. Bound WiFi to 15 s and NTP to 10 s. Fall back to compile time + uptime. Store UTC. Write a per-boot `session.json` with the time source. |
| Web | Delete `WEBSERVER`. Re-add a read-only status endpoint only if it is wanted, after the runtime refactor. |
| Input | Momentary KEY1, active-low, 30 ms debounce, edge-triggered toggle. A session state machine acknowledges start/stop. LED shows idle/recording/error. |
| Build | Arduino CLI + `sketch.yaml`. ESP32 core `2.0.17`. Exact library versions. GitHub Actions for firmware, case, tools, and guards. |
| Repo | Binary STLs in-repo (about 3.8 MB). Drop the duplicate `lid_501*` exports. MIT `LICENSE`. Document `tools/`. |
| Case/docs | Add `case/export.sh`. Document every `part=` value. Correct the `front_ext` screw wording. Document the MIC1 gap and options. Keep the side vents. |

---

## 2. Findings: disposition and evidence

Line references are against `94ade22`. The review's evidence was re-checked
against the source; corrections are called out.

| Sev | Area | Disposition | Evidence and correction |
|---|---|---|---|
| High | Boot/time | **Confirmed** | `defines.h:42` hardcodes `SKIP_RTC true`. `TimeFunctions.cpp:13-20` (`waitWifiLoop`) and `:22-32` (`updateTime`) loop without a bound. `rtc_prepare` calls them at `:72-73` when the RTC is skipped and at `:86-87` when the year is not 2021. `README.md:7` claims the DS3231 provides timestamps. With no time set, `time_to_file` formats the zeroed `struct tm` as `19000100,00:00:00` (`TimeFunctions.cpp:91-99`, `SDData.cpp:118-126`). |
| High | SD/I2C races | **Confirmed** | Audio (core 0) writes `data_audio_raw_output` (`Mic.cpp:89`), while core 1 writes `data_output` (`.ino:42`), `data_pir_output` (`.ino:62`), and `data_accel_output` (`accel.cpp:20`). `sd_card` and `write_n` are plain globals (`SDData.cpp:3,10`); `record_streams` is a plain `unsigned char` (`defines.cpp:3`); `a0/a1/current_tm` are shared (`TimeFunctions.cpp:10-11,91-99`). `bme` and `mpu` share `Wire` with no lock (`.ino:187-188`, `sensors.cpp`, `accel.cpp`). |
| High | Stream switch | **Confirmed** | `.ino:74-102`: while idle, the loop blinks for 1 s, then a HIGH starts; while recording, the same HIGH stops. Holding the input HIGH toggles about once per second. Presses during the 2 s blink are not sampled. There is no debounce. The pin is configured as plain `INPUT` (`.ino:192`), so an open switch floats. |
| High | `WEBSERVER` | **Confirmed** | `ServerFunctions.cpp:10` uses undeclared `altitude`; `:10` uses undefined `SEALEVELPRESSURE_HPA`; `:18` defines `SendHTML` after its use at `:11`; `ServerFunctions.h` declares neither handler. `.ino:196-197` references the handlers. `.ino:258-260` calls `server.handleClient()` once per 10 s. The `.ino` includes `ServerFunctions.h` before `defines.h`, so uncommenting `WEBSERVER` in `defines.h` does not define it for the header. |
| Med | Stacks | **Confirmed, comment corrected** | ESP-IDF `xTaskCreate*` takes the stack depth in **bytes**, not words (the `.ino` comments at `:209,218,227,236,245` say "words"). The sensor task gets 1900 B while calling `Serial.printf("%f")` and `File.printf("%f")`; the PIR task gets 1500 B while calling `printf`. The high-water probes are commented out (`.ino:48,69`). `uxTaskGetStackHighWaterMark` also reports bytes in ESP-IDF. |
| Med | Error handling | **Confirmed** | `bme.begin(0x76)` and `mpu.Initialize()` are unchecked (`.ino:187-188`). A mount failure only prints (`SDData.cpp:14-16`). File-open failures set `sd_card=0` for the CSV files but not for audio/accel (`SDData.cpp:90-106`). `i2s_read` uses `portMAX_DELAY` (`Mic.cpp:85`); the audio write result is ignored (`Mic.cpp:89`); accel flushes every sample (`accel.cpp:20-21`). `loop()` restarts the board every 10 s whenever `sd_card == 0` (`.ino:262-268`), producing a reboot loop. |
| Med | RTC logic | **Confirmed** | `updateTimeFromRTC` calls `mktime` on an uninitialized `struct tm`; `tm_isdst` is never set (`TimeFunctions.cpp:36-44`). The validity test accepts only 2021 (`:70`). `getLocalTime` returns are ignored in the double buffer (`:91-99`). A valid RTC with any other year falls into the unbounded WiFi/NTP path. |
| Med | Build repro | **Confirmed** | `sensors.h:6` includes the legacy `<driver/adc.h>`; `defines.h:23` includes `<driver/i2s.h>`; `.ino:176` calls `btStop()`. All require ESP32 core 2.0.x. `Mic.h:6` includes `arduinoFFT.h` while the FFT code is commented out (`Mic.cpp:99-119`). There is no `sketch.yaml`/`platformio.ini` and no CI. `TinyMPU6050` is in the Arduino library index at 0.5.3, so it can be pinned in `sketch.yaml`. |
| Low | Case docs (`part=`) | **Confirmed** | `esp32_case.scad:6` lists only `base, lid, lid_pir, plunger, assembly, assembly_pir`, but the output block at `:389-419` also supports `base_501, lid_501, pir501_cover, assembly_501, none`. `case/README.md` never names `part=`; `:74-75` only mentions `front_ext`. |
| Low | Case docs (`front_ext`) | **Confirmed — wording only** | `esp32_case.scad:66` sets `lid_screw_y = front_ext > 0 ? [-15, 45] : [15, 45]`. The front lid-screw pair and its base boss (both use this list) move 30 mm forward in the extended version. `case/README.md:63` says "the four screw bosses stay" and `:72` says "Screws: unchanged ... original boss positions"; both are wrong for the front pair. The boss exists; the review's "no boss" claim was the error. |
| Low | Repo/assets | **Confirmed, duplicate corrected** | The STLs are ASCII (`file` reports "ASCII text"); the total is about 12.9 MB. `esp32_case_lid_501.stl` and `esp32_case_lid.stl` have the same facet count and bounding box but different bytes (different triangulation/export), and the SCAD maps `lid_501` to the same `lid(false)` module. They are functionally duplicate, not byte-identical; keeping both invites drift. Same for `_ext`. There is no `LICENSE`, no CI, and no `tools/README.md`; `gerber_raster.py` hardcodes `/tmp/opencode` output. |
| — | MIC1 | **Confirmed, scope narrowed** | The INMP441 port is top-facing and aligned with the 4 mm lid hole. The open item is the roughly 16 mm cavity between the port and the lid, not the hole purpose. HS-3 measures the acoustic effect and decides between leave-as-is, gasket/tube, vents, and mesh. HS-2 records the options and does not pre-empt the measurement. |

---

## 3. Runtime and concurrency

### 3.1 Task layout

Keep five tasks. Do not add a storage queue yet.

| Task | Core | Priority | Stack (bytes) | Owns | Period |
|---|---:|---:|---:|---|---|
| `audio_loop` | 0 | 3 | 8192 | I2S driver, audio WAV file | continuous while recording |
| `sensor_loop` | 1 | 2 | 8192 | BME280, photo/dust ADC, main CSV | 300 s |
| `sensor_pir_loop` | 1 | 2 | 8192 | PIR, PIR CSV | 1 s poll |
| `input_loop` | 1 | 3 | 4096 | KEY1, LED, session state machine | 10 ms |
| `sensor_accel_loop` | 1 | 1 | 4096 | MPU6050, accel binary file | 10 Hz |
| Arduino `loop()` | 1 | 1 | 8192 | health line, SD retry, watchdog feed | 1 s |

Rationale:

- Audio must preempt on core 0 to drain the I2S DMA. Priority 3 is enough; with
  the HS-12 AP the WiFi task stays up, but it is light and the radio work is
  mostly on the WiFi core.
- Input gets priority 3 so a 30 ms debounce is not starved by sensor work.
- Sensor and PIR stay separate: a BME/dust sample takes about 2 s, and merging
  PIR into it would delay a 1 s poll.
- Stacks are in bytes. 8192 B is the starting point for any task that calls
  `printf` or `File.printf`; measure with `uxTaskGetStackHighWaterMark` and keep
  at least 25 % free. The current 1900 B/1500 B values are unsafe.

### 3.2 Ownership and locking

- **SD:** one `SemaphoreHandle_t sd_mutex` created before the tasks. All access
  goes through a `Storage` module (`storage_open`, `storage_write`,
  `storage_flush`, `storage_close`). The module checks an atomic `sd_ok` under
  the mutex and returns a result. No task calls `SD.open` or `File::write`
  directly.
- **I2C:** one `i2c_mutex`. All BME280 and MPU6050 transactions take it. Use an
  RAII guard so early returns cannot leak it. Call `Wire.begin(21, 22, 100000)`
  and `Wire.setTimeOut(50)` explicitly; probe BME280 at `0x76` then `0x77`.
- **No nested locks.** A writer never holds the SD mutex while taking the I2C
  mutex, and vice versa. The sensor task reads I2C first, releases, then writes
  SD. Document this in the module header.
- **Flags:** `std::atomic<SessionState>` for the session, `std::atomic<bool>` for
  `sd_ok` and stop requests. Remove `record_streams`, `sd_card`, and `write_n`
  as plain globals. `std::atomic` is available in the core 2.0.x toolchain.
- **Time:** remove `a0`, `a1`, and `current_tm`. `format_timestamp()` uses a
  task-local `struct tm` and `localtime_r`/`strftime`; it has no shared state.
- **Sensor values:** make them task-local unless a status endpoint needs a
  snapshot. If one is added later, publish an immutable `SensorSnapshot` under a
  short mutex, not the raw globals.

### 3.3 Session state machine

Replace the boolean with an explicit state:

```
IDLE -> STARTING -> RECORDING -> STOPPING -> IDLE
             \-> ERROR -> IDLE
```

- The input task only requests a transition. The audio and accel tasks open,
  write, flush, and close their own files and acknowledge.
- A press during `STARTING`/`STOPPING` is ignored.
- If any owner fails to open a file, it closes anything it opened, signals
  `ERROR`, and the session returns to `IDLE`. No half-open session, no repeated
  file churn.
- Stop is a request; the owners finish the current block, flush, and close. The
  state returns to `IDLE` only after both acknowledge.

### 3.4 Audio path

- 16 kHz, 16-bit mono PCM, written as a WAV file. The current
  `i2s_adc_data_scale` converts to 8-bit and is removed.
- Read 1024 samples (2048 B) per block with a 500 ms timeout, not
  `portMAX_DELAY`. Five consecutive timeouts stop the session with an error.
- Flush every 5 s or 256 KB, whichever comes first; flush and close on stop.
  This bounds loss on a power cut without flushing every block.
- Verify the INMP441 channel and bit alignment on hardware. The current comment
  in `Mic.cpp:35` says the left/right selection is uncertain; the implementation
  must not rely on that comment.
- Keep `use_apll=false` unless a bench test shows unacceptable clock drift. APLL
  at 16 kHz is not validated here.
- Write the WAV header at open and patch the sizes on close. A power cut leaves
  a header that `tools/wav_fix.py` (or `sox`) can repair; document the command.

### 3.5 Accel path

- 10 Hz. Write a 16-byte header (`ACL1`, version 1, record size, sample rate,
  start epoch) and packed 18-byte records: `uint32_t epoch_s`, `uint16_t ms`,
  six `int16_t` axes. This removes the host-endian, padding-dependent raw
  `struct accel_data` and adds 1 ms resolution.
- Flush every 5 s, not every sample.
- If a write fails, stop the session and report the error.

### 3.6 Error policy

- Check every init, open, write, flush, and close result. Log a compact error
  with the task name and error code.
- A failed BME280 or MPU6050 is marked unavailable; retry every 60 s. A failed
  sensor does not block boot or the other streams.
- A failed SD mount is not a reboot. Keep the sensors running, show the error
  LED, retry the mount every 30 s, and allow a new session when a card appears.
- A write error during a session stops that session; it does not restart the
  board.
- Check `xTaskCreatePinnedToCore` returns. A failed task creation is a boot
  error: log it, show the error LED, and halt in a safe state.
- Enable the task watchdog for the audio, input, and sensor tasks with a 10 s
  timeout. Feed after each successful loop iteration. Bounded I2C and I2S waits
  make the feeds meaningful.

### 3.7 Rejected alternatives

- **One storage task with a queue.** It would isolate SD latency, but it adds
  RAM, a second buffering layer, and a new failure mode. The audio rate is only
  32 KB/s and writes are 2 KB blocks; a mutex is sufficient. Revisit if the
  bench shows audio underruns or SD p99 latency above 100 ms.
- **One task per sensor without locks.** It preserves the current structure but
  does not fix the races; rejected.
- **Per-file mutexes.** They do not protect the shared SD/VFS and SPI bus;
  rejected.
- **Move everything to core 0 or core 1.** No measured reason; the current
  split keeps audio away from the CSV work. Keep it.

### 3.8 Deferrals

- FFT/octave-band loudness. The current code is commented out; remove the dead
  code and the `arduinoFFT` dependency. Re-add as a separate feature if wanted.
- A storage queue, if measurements justify it.
- OTA, deep sleep, and battery optimization. The deployment assumption is
  mains-powered.

---

## 4. Boot and time

### 4.1 Boot sequence

1. `Serial.begin(115200)`, 1 s settle, print the boot banner: firmware version,
   reset reason, chip, flash size, free heap.
2. Set `TZ=UTC0` and `tzset()`.
3. `Wire.begin(21, 22, 100000)`, `Wire.setTimeOut(50)`.
4. Time init (section 4.2). This is the only network step at boot.
5. Mount SD with up to three attempts. Write `session.json` if mounted.
6. Probe BME280 and MPU6050; mark unavailable on failure; do not block boot.
7. Install the I2S driver; mark audio unavailable on failure.
8. Create the tasks; check every return.
9. Enter idle: LED heartbeat, health line every 60 s.

Worst case is about 30 s with a 25 s network budget. No path blocks forever.

### 4.2 Time sources

| Priority | Source | Condition | `time_source` |
|---|---|---|---|
| 1 | DS3231 | `rtc.begin()` succeeds, `!rtc.lostPower()`, year in 2024-2100 | `rtc` |
| 2 | NTP | WiFi connects within 15 s and `getLocalTime` succeeds within 10 s | `ntp` |
| 3 | Compile time | No RTC and no NTP | `compile` |

- **RTC:** read it at boot and every hour; `settimeofday` from a zeroed
  `struct tm` with `tm_isdst = 0`. If `lostPower()` is true or the year is
  outside the range, treat it as invalid and try NTP. If NTP succeeds and an RTC
  is present, adjust the RTC.
- **NTP:** call `configTime(0, 0, "pool.ntp.org")` only after a bounded connect.
  Use `getLocalTime(&tm, 10000)`. With `AP_ALWAYS_ON=0`, then
  `WiFi.mode(WIFI_OFF)`; with the HS-12 AP, keep `WIFI_AP_STA` up (section 5.1).
  No `btStop` here. Skip WiFi entirely if the credentials are still the example
  placeholders.
- **Compile fallback:** parse `__DATE__`/`__TIME__` as UTC and set the system
  clock. This gives ordered, plausible timestamps for an offline node, but they
  are approximate. `session.json` records `time_valid=false` so downstream
  knows.
- **Timezone:** store UTC everywhere. `format_timestamp` writes
  `YYYYMMDD,HH:MM:SS`. Document that consumers convert to local time. No DST
  logic in the firmware.

### 4.3 Session metadata

Write one `/<timestamp>_session.json` per boot when the SD is mounted:

```json
{
  "fw_version": "0.2.0",
  "git_sha": "unknown",
  "boot_id": "20261104T194500Z-1a2b3c",
  "reset_reason": "power_on",
  "time_source": "rtc",
  "time_valid": true,
  "sd_total_mb": 7620,
  "sd_free_mb": 7412,
  "sensors": { "bme280": true, "mpu6050": true, "mic": true, "sd": true },
  "config": { "sensor_period_s": 300, "pir_period_s": 1, "audio_hz": 16000, "accel_hz": 10 }
}
```

The file is hand-formatted with `snprintf`; no JSON dependency. It is the
authoritative record of whether the timestamps in that boot are trustworthy.

### 4.4 Rejected alternatives

- **Keep `SKIP_RTC`.** It makes the README false and forces a network dependency.
  Rejected; the runtime probe handles a missing RTC.
- **Block until WiFi/NTP succeeds.** Rejected; an offline node must log.
- **Use local time with DST.** Rejected; UTC avoids a timezone database and
  ambiguity. Convert at analysis time.
- **Use 1970 + uptime as the fallback.** Rejected; compile time is more useful
  and still clearly marked invalid.

### 4.5 Deferrals

- Periodic NTP resync when the radio is otherwise off. RTC drift is small; an
  ESP32 without an RTC drifts about 1-2 s/day. Revisit if timestamps matter more
  than power.
- GPS time, deep sleep, and battery budgeting.

---

## 5. `WEBSERVER`

**Decision: delete it.**

- The branch does not compile, serves only BME280 values, reads the I2C bus from
  the Arduino loop task without a lock, and polls once per 10 s.
- The primary product is an SD logger; live data is read from the card.
- Removing it also removes the `altitude`/`SEALEVELPRESSURE_HPA` problem, the
  include-order trap, and the `btStop()` compatibility dependency.

Deferred: a read-only `GET /api/readings` endpoint, served from a published
`SensorSnapshot`, STA-only on the local network, no auth (the device has no AP
mode). It is only worth adding after the runtime refactor and only if the user
wants live readings. Do not restore the old HTML page.

Rejected: fixing the current branch in place. It would need handler declarations,
an altitude constant, a lock, its own task or a non-blocking loop, and a test;
that is more work than deleting it and re-adding a smaller endpoint later.

### 5.1 HS-12: WiFi AP mode at boot

**Decision: always-on AP in `WIFI_AP_STA`, with a small status page.** This
qualifies D2 ("WiFi off after boot") and D3 ("no AP mode"); the old `WEBSERVER`
page stays deleted.

- **Policy:** `AP_ALWAYS_ON 1` (`defines.h`). The AP is up from boot for the
  whole runtime, because the user asked for a phone or laptop to reach the board
  without a router. Rejected: a 10 min provisioning window, a button-triggered
  window, and fallback-AP-only. A window saves heat and power, but needs a
  trigger the case does not have and makes the page unavailable when it matters.
- **Radio:** `WiFi.mode(WIFI_AP_STA)`; `softAPConfig(192.168.4.1/24)`;
  `softAP(AP_SSID, AP_PASSWORD, 1, 0, 4)`. WPA2-PSK, channel 1, four clients.
  In AP+STA the hardware forces the AP onto the STA channel once the router
  associates, so channel 1 is a request, not a guarantee. STA stays up for NTP;
  nothing turns WiFi off after the time sync.
- **Credentials:** `AP_SSID` (default `esp32-room-sensors`) and `AP_PASSWORD` in
  `wifi_pass.h`; `wifi_pass.example.h` documents them. `wifi_pass.h` is
  git-ignored and has no default, so the password must be set. `StatusServer.cpp`
  fails the build if `AP_PASSWORD` is shorter than 8 characters.
- **Endpoints:** `GET /` (HTML table, 5 s meta refresh) and `GET /api/status`
  (JSON: `uptime_s`, `temperature_c`, `humidity_pct`, `pressure_hpa`,
  `sensors_valid`, `sensor_age_ms`, `time_source`, `time_valid`, `rtc_present`,
  `time_utc`, `sd_ok`, `recording`, `sta_connected`, `sta_ip`, `ap_ip`,
  `ap_clients`). Unknown paths return 404. No auth, no captive portal, no file
  listing.
- **Ownership:** the sensor task publishes a `SensorSnapshot` under a mutex
  after each BME280 read; the handlers copy it and never touch I2C. A dedicated
  `status_server` task (core 1, priority 1, 8192 B) polls
  `server.handleClient()` every 10 ms, so a slow HTTP client cannot stall the
  loggers. The `loop()` delay and the SD restart path are unchanged.
- **Boot order:** the AP starts in `setup()` before `init_time()`, so an
  AP-only boot still has an AP while the bounded STA/NTP attempt runs.
  `init_time()` uses `WIFI_AP_STA` and the HS-12 post-sync policy leaves the
  radio on; AP-only time comes from the HS-4 RTC/compile fallback. The time and
  status modules share the HS-4 `time_source`/`time_valid`/`rtc_present` API.
- **Security:** WPA2-PSK, link-local only, no HTTP auth. Anyone who knows the
  PSK is on the same L2 network as the board. Acceptable while the page is
  read-only and the device is not exposed to the internet.
- **Thermal:** the AP radio adds heat next to the BME280. The bias is
  **unmeasured** (the board was reserved during HS-12). Measure AP on vs off for
  1 h on the bench; if the offset exceeds about 0.5 °C, set `AP_ALWAYS_ON 0` and
  open a follow-up for a boot window. Until then, absolute temperature is
  indicative while the AP is on.
- **Open:** HS-6 owns recording state; HS-8 owns the SD/`loop()` rework. This
  module only reads `sd_card`, `record_streams`, and the HS-4 time globals.

---

## 6. Input and switch

**Decision: momentary toggle.**

- The fitted hardware is KEY1, a 12 mm tactile switch with an optional printed
  plunger. Use `INPUT_PULLUP` and wire the switch to ground; active-low.
- Debounce for 30 ms (sample every 10 ms, require three stable samples).
  Trigger on the press edge, not the level.
- One press starts or stops the session. Holding the button does not repeat.
  A press during `STARTING`/`STOPPING` is ignored.
- If a latching switch is fitted instead, the edge-trigger still toggles on each
  change; document that the supported hardware is momentary. A level-based
  "closed = recording" mode is deferred until someone actually fits a latch.
- LED: idle = short flash every 2 s; recording = solid on; error = three fast
  blinks. The LED is the only local state indicator.
- Start failure: close partial files, show the error pattern, return to idle.
  Stop failure: show the error pattern, keep the state machine from re-entering
  the session until the owner acknowledges.

Open hardware check: confirm KEY1 is wired to GPIO13 and closes to ground. The
current code reads GPIO13 as a plain input and treats HIGH as active; the new
firmware assumes the opposite polarity with a pull-up.

---

## 7. Build, toolchain, and CI

**Decision: Arduino CLI + `sketch.yaml`.** Keep the Arduino sketch layout; do
not introduce PlatformIO for this size of project.

`src/esp32_room_sensors/sketch.yaml`:

```yaml
profiles:
  esp32:
    fqbn: esp32:esp32:esp32
    platforms:
      - platform: esp32:esp32 (2.0.17)
        platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
    libraries:
      - Adafruit BME280 Library (2.3.0)
      - Adafruit Unified Sensor (1.1.15)
      - Adafruit BusIO (1.17.4)
      - RTClib (2.1.4)
      - TinyMPU6050 (0.5.3)
default_profile: esp32
```

- **Core:** ESP32 Arduino `2.0.17`, the last 2.0.x. The firmware uses legacy
  `driver/i2s.h`, `adc1_get_raw`, and `btStop`; core 3.x removes or changes
  them. A 3.x migration is a separate project after the runtime refactor.
- **Libraries:** exact versions above. `TinyMPU6050` is in the Arduino index at
  0.5.3 (verified). `Adafruit BusIO` is the BME280/RTClib dependency.
- **Local build:** `arduino-cli compile --profile esp32 src/esp32_room_sensors`.
  Copy `wifi_pass.example.h` to `wifi_pass.h` first. `wifi_pass.h` stays
  ignored.
- **CI (GitHub Actions), staged so every merge is green:**
  - `firmware`: install Arduino CLI, cache the core and libraries, install the
    pinned core, copy the example credentials, compile the profile (HS-9).
  - `guards`: `git ls-files -ci --exclude-standard` must be empty; run
    `codespell` with a small project ignore list (HS-9).
  - `case`: install OpenSCAD, run `case/export.sh --check` into a temp dir,
    assert every part reports `Simple: yes` and its bounding box matches the
    manifest. Added with HS-10 when the script exists.
  - `tools`: `python3 -m py_compile tools/*.py`, install
    `tools/requirements.txt`, run the Gerber rasterizer, assert a non-blank PNG.
    Added with HS-11 when the requirements and the `--out` path exist.
- **Debug build:** `--build-property compiler.cpp.extra_flags="-DLOG_LEVEL=3"`.
  `sketch.yaml` profiles do not support arbitrary build properties; the CI job
  passes the flag on the command line.

Rejected alternatives:

- **PlatformIO.** A good fit if native unit tests become important, but it adds
  a second build system and a `platformio.ini` for a small sketch. Revisit if
  the test surface grows.
- **Arduino IDE only.** Cannot pin the core/libraries or run CI; rejected.
- **ESP32 core 3.x now.** It would force the I2S/ADC migration before the
  runtime fixes and before the hardware is bench-validated. Deferred.

Deferrals:

- `clang-format` enforcement. Run a single formatting pass after the runtime
  refactor; a check now would produce a large, unrelated diff.
- Native unit tests for pure functions. Add when a testable module exists.
- Release automation and binary artifacts. Not needed for a personal repo.

---

## 8. Repository and assets

**Decision: binary STLs in the repository.**

- Export with `openscad --export-format binstl`. The current ASCII set is about
  12.9 MB; binary is about 3.8 MB (verified on the plunger: 197 KB to 57 KB).
- Keep the STLs in-repo rather than Releases/LFS. A clone should be printable
  without extra steps; 3.8 MB is acceptable.
- `case/export.sh` is the source of truth for the mapping. CI runs it in check
  mode.
- Drop `esp32_case_lid_501.stl` and `esp32_case_lid_501_ext.stl`. The SCAD maps
  `lid_501` to the same `lid(false)` module, so the 501 variants use the
  standard lid. Keep the `part="lid_501"` alias in the SCAD and document the
  mapping; do not keep a separately exported copy that can drift.
- Add an MIT `LICENSE`. It covers the firmware, tools, case, and PCB files. If a
  copyleft hardware license is preferred, replace it before publishing; that is
  a user decision, not a technical one.
- Add `tools/README.md` and `tools/requirements.txt` (Pillow). Give
  `gerber_raster.py` an `--out` argument with a default under `tools/out/`;
  ignore generated outputs.
- Keep the `.gitignore` entries for `wifi_pass.h`, `build/`, `.pio/`,
  `.vscode/`, and `__pycache__/`; add `tools/out/` and `case/out/`.
- CI guards tracked-but-ignored files and codespell. Do not add
  `clang-format --dry-run` yet.

Rejected alternatives:

- **LFS or Releases-only.** Extra clone/setup friction for a small asset set;
  rejected for now.
- **Keep ASCII.** It is 3.4x larger for no benefit; rejected.
- **Keep both lid_501 files.** They are functionally duplicate and already
  differ in triangulation; rejected.

Deferrals:

- A release workflow that attaches STLs and firmware binaries.
- A case gasket/tube model; see section 9.

---

## 9. Case and documentation

### 9.1 Export reproducibility

- Add `case/export.sh` with a table mapping each output file to
  `part`/`front_ext`, a `--out` directory, and a `--check` mode that greps
  `Simple: yes` and compares the bounding box against a small manifest.
- Update the `part` comment in `esp32_case.scad` to list every value the output
  block accepts.
- Add a README table with the exact command for every STL. The current README
  names only `front_ext`; a reader cannot reproduce the set.

### 9.2 `front_ext` correction

The SCAD moves the front lid-screw pair with the extension:

```scad
lid_screw_y = front_ext > 0 ? [-15, 45] : [15, 45];
```

The PCB and the four board-mount bosses stay at their original coordinates. The
front lid-screw pair and its base boss move 30 mm forward; the rear pair stays.
The README must say this instead of "the four screw bosses stay" and "Screws:
unchanged ... original boss positions". This is a wording fix, not a geometry
change.

### 9.3 MIC1 acoustic note

The INMP441 port faces up and is aligned with the 4 mm lid hole. There is about
16 mm of air between the port and the lid (measure the actual module/header
height). That cavity can color the response and create a resonance; the case is
not acoustically transparent.

Document these options in `case/README.md`:

1. **Gasket/cup (recommended if acoustic data matters).** A soft foam ring
   around the port and a short, wide cup to the lid hole. Avoid a narrow
   4 mm x 16 mm tube: a quarter-wave resonance near 5.4 kHz colors the band.
2. **Dust mesh.** An acoustic mesh over the 4 mm hole keeps dust out; measure
   the high-frequency attenuation.
3. **Leave as-is.** Acceptable for voice/level monitoring; calibrate in situ.
4. **Side vents.** Keep the existing VACG side vents for airflow. HS-3 tests
   their acoustic effect if a prototype is available; do not remove them for
   the microphone.

HS-3 is the measurement ticket: baseline (no lid), lid with the hole open, hole
taped, gasket/tube, mesh, and vents if available. HS-2 records the options and
the gap; the final recommendation follows HS-3's evidence. Do not model a new
printed part in HS-2 or HS-10.

### 9.4 README alignment

The current README says the DS3231 provides timestamps and that an optional web
server exposes readings. Both are false against the current code and will be
false against the target until the child tickets land. HS-11 updates the README
after HS-4 through HS-10: RTC optional, `WEBSERVER` removed, Arduino CLI build,
data formats, `docs/` layout, CI.

---

## 10. Data formats and pin map (appendix)

### 10.1 Pin map

| Signal | GPIO | Notes |
|---|---:|---|
| Status LED | 2 | active high |
| Photo (LDR) | 34 | `ADC1_CHANNEL_6` |
| Dust (GP2Y) | 36 | `ADC1_CHANNEL_4` |
| PIR | 35 | input-only; HC-SR501 output |
| SD card CS | 5 | VSPI |
| Dust LED | 33 | GP2Y LED drive |
| KEY1 / button | 13 | active-low with `INPUT_PULLUP` (assumed) |
| I2S SCK | 14 | INMP441 |
| I2S WS | 15 | INMP441 |
| I2S SD | 4 | INMP441 data |
| I2C SDA | 21 | BME280 0x76/0x77, MPU6050 0x68 |
| I2C SCL | 22 | 100 kHz, 50 ms timeout |
| SPI SCK/MISO/MOSI | 18/19/23 | default VSPI |

Verify the pin map against the PCB before changing wiring. The code is the
source of truth for the firmware; the README should mirror it.

### 10.2 CSV files

Main data, one row per 300 s:

```
date,time,dust,photo,temp,humidity,pressure
20261104,19:45:00,123,456,21.50,45.20,1013.25
```

PIR, one row per transition:

```
date,time,pir
20261104,19:45:01,1
```

- `date` is `YYYYMMDD`, `time` is `HH:MM:SS`, both UTC.
- On a BME280 failure, leave the temperature/humidity/pressure fields empty
  (trailing commas); do not write `nan` or a stale value.
- Keep the column order for compatibility. Add new columns only at the end and
  document them.

### 10.3 Audio

- WAV, 16 kHz, 16-bit signed PCM, mono.
- 44-byte header; sizes patched on close. A power cut may leave the sizes zero;
  `tools/wav_fix.py` or `sox` can repair them.
- Filename: `YYYYMMDD_HHMMSS_audio.wav`, with a numeric suffix on collision.
- Flush every 5 s or 256 KB; close on stop.

### 10.4 Accel

- Header: `ACL1`, version 1, record size 18, sample rate 10, start epoch,
  reserved.
- Record (18 bytes, packed): `uint32_t epoch_s`, `uint16_t ms`, `int16_t ax`,
  `ay`, `az`, `gx`, `gy`, `gz`.
- Filename: `YYYYMMDD_HHMMSS_accel.bin`.
- Flush every 5 s; close on stop.

### 10.5 Session metadata

`YYYYMMDD_HHMMSS_session.json`, written once per boot. See section 4.3.

---

## 11. Backlog and child tickets

Eight child tickets were created under HS-2. HS-3 is a related bench
investigation created separately; it is not a child.

| Order | Ticket | Priority | Outcome | Depends on |
|---:|---|---|---|---|
| 1 | HS-9 | high | Build/CI: `sketch.yaml` + GitHub Actions | — |
| 2 | HS-4 | high | Bounded boot, time sources, session metadata | HS-9 (CI) |
| 3 | HS-5 | high | SD/I2C locking and shared-state ownership | HS-4 |
| 4 | HS-6 | high | Session state machine, button, LED | HS-5 |
| 5 | HS-8 | medium | Error handling, stacks, watchdog, health logging | HS-6 |
| 6 | HS-7 | medium | Audio/accel pipeline and data formats | HS-6 |
| 7 | HS-10 | low | Case export script, docs corrections, MIC1 note | HS-9 (CI case job) |
| 8 | HS-11 | low | Binary STLs, license, tools docs, README alignment | HS-4..HS-10 |
| — | HS-3 | medium | MIC1 acoustic measurement (related, not a child) | — |

Mapping to the HS-2 candidate improvements:

| Candidate | Ticket |
|---|---|
| P1 bounded boot/time | HS-4 |
| P2 SD/I2C mutexes + atomics | HS-5 |
| P3 switch edge/failure handling | HS-6 |
| P4 pinned toolchain + CI | HS-9 |
| P5 fix/delete `WEBSERVER` | HS-8 (delete) |
| P6 stacks/high-water/task checks | HS-8 |
| P7 init/write checks, audio flush | HS-7 + HS-8 |
| P8 case export/docs | HS-10 |
| P9 binary STLs/license/tools/docs | HS-11 |
| MIC1 measurement | HS-3 |

Not doing:

- Restoring the old HTML web page.
- A 3.x core migration in this batch.
- Case geometry changes, a new gasket/tube model, or PCB changes.
- Git history rewrite for the WiFi credentials; the user has assessed them as
  non-sensitive.
- OTA, deep sleep, battery optimization, release automation.
- `clang-format` enforcement in this batch.

---

## 12. Verification plan

HS-2 itself is docs-only:

```bash
git -C /home/vroth/esp32-v1337-all-in-one status --short --branch
git show --stat HEAD        # docs/architecture.md only
kaneo get task-relation/evple2ukomnyhxavnv27noz7
```

Per-ticket gates are in the child tickets. The shared bench matrix:

| Check | Gate |
|---|---|
| Offline boot, no AP, no RTC | Boots within 30 s; logs with `time_source=compile`; never `19000100` |
| Valid RTC | Boots with `time_source=rtc`; no STA association (the HS-12 AP stays up); RTC adjusted only after NTP |
| Invalid RTC + AP | NTP within 25 s; RTC adjusted; `time_source=ntp`; AP still reachable |
| AP-only boot | SSID visible, WPA2, DHCP lease `192.168.4.x`; `/` and `/api/status`; `time_source=compile` or `rtc`, `time_valid` matches |
| AP+STA | STA lease from the router; AP stays up after the time sync; `sta_connected`/`sta_ip` correct |
| AP thermal | 1 h AP on vs off; BME280 bias documented; > 0.5 °C opens a follow-up |
| 2 h audio + accel + CSV | No truncated CSV, no interleaved audio, no `sd_ok` flapping |
| 10 min WAV | Playable 16 kHz mono; duration within 1 % |
| Power cut during recording | WAV recoverable; no corrupt CSV header |
| SD removal during recording | Session stops, error LED, no reboot; reinsert allows a new session |
| Sensor unplug at boot/run | Degraded mode logged; other streams continue |
| 24 h soak | No WDT reset; stack HWM >= 25 % free; health line every 60 s |
| Button matrix | One press = one transition; hold does not repeat; ignored during transitions |
| Case export | Every part `Simple: yes`; bbox matches the manifest |
| Repo guards | `git ls-files -ci --exclude-standard` empty; binary STLs; `tools` runs |

---

## 13. Assumptions and open questions

Resolved by decision (reversible):

- **Deployment:** mains-powered, always-on; WiFi may be absent; recording
  sessions are minutes to hours; SD card at least 8 GB, class 10; the web UI is
  not required for the core function. If these are wrong, the bounded-boot and
  offline-logging decisions still hold; only power/deep-sleep work changes.
- **RTC:** may or may not be fitted. The runtime probe handles both; no decision
  depends on the answer.
- **`WEBSERVER`:** delete. Re-add a read-only endpoint later only if wanted.
- **Switch:** KEY1 is momentary to ground. Verify the polarity on the board; the
  edge-triggered toggle is the supported behavior.
- **License:** MIT as a default. Confirm before publishing if a different
  license is preferred.
- **STLs:** binary in-repo; the SCAD remains the source of truth.
- **Case MIC1:** document the gap and options now; HS-3 provides the measured
  recommendation. Side vents stay for VACG airflow.

Deferred until measured or requested:

- The final MIC1 fix (HS-3).
- A storage queue if SD latency requires it.
- A 3.x core migration, OTA, deep sleep, native tests, `clang-format`, release
  automation.
- The read-only status endpoint.

---

## 14. Approval

Review checklist:

- [ ] Every finding above has a disposition and evidence.
- [ ] Every architecture area has a chosen option, rationale, and deferrals.
- [ ] The child tickets exist and the subtask relations are verified.
- [ ] The README alignment is assigned to HS-11 and does not block this record.
- [ ] No firmware, hardware, or case changes are in HS-2.

Once approved, HS-2 is the umbrella for HS-4..HS-11; HS-3 remains a related
measurement ticket.
