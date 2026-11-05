// HS-3 bench recorder: ESP32 + INMP441 + SD card -> 10 s mono WAV.
// Standalone sketch: no WiFi, no main-firmware sources, no extra libraries.
// Pins match src/esp32_room_sensors/defines.h.
//
// Serial: 115200 baud. A recording starts once after boot and again on each
// button press (pin 13, active high). Files are /mic_000.wav, /mic_001.wav, ...

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <driver/i2s.h>

// ---- pins (src/esp32_room_sensors/defines.h) ----
static const int PIN_I2S_SCK   = 14;
static const int PIN_I2S_WS    = 15;
static const int PIN_I2S_SD    = 4;
static const int PIN_SD_CS     = 5;
static const int PIN_BUTTON    = 13;  // stream switch: high = pressed
static const int PIN_STATUS_LED = 2;

// ---- recording format ----
static const uint32_t SAMPLE_RATE    = 32000;  // mono, 16-bit PCM output
static const uint32_t RECORD_SECONDS = 10;
static const uint32_t TOTAL_SAMPLES  = SAMPLE_RATE * RECORD_SECONDS;
static const size_t   BLOCK_SAMPLES  = 1024;   // 32-bit slots per I2S read
static const uint16_t WAV_BITS       = 16;
static const uint16_t WAV_CHANNELS   = 1;

static int32_t i2s_buffer[BLOCK_SAMPLES];
static int16_t pcm_buffer[BLOCK_SAMPLES];

static void set_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)(v >> 8);
}

static void set_le32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void build_wav_header(uint8_t *h, uint32_t data_bytes) {
  memcpy(h + 0, "RIFF", 4);
  set_le32(h + 4, 36 + data_bytes);
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  set_le32(h + 16, 16);                                     // fmt chunk size
  set_le16(h + 20, 1);                                      // PCM
  set_le16(h + 22, WAV_CHANNELS);
  set_le32(h + 24, SAMPLE_RATE);
  set_le32(h + 28, SAMPLE_RATE * WAV_CHANNELS * WAV_BITS / 8);
  set_le16(h + 32, WAV_CHANNELS * WAV_BITS / 8);
  set_le16(h + 34, WAV_BITS);
  memcpy(h + 36, "data", 4);
  set_le32(h + 40, data_bytes);
}

static bool i2s_start() {
  const i2s_config_t config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // board ties L/R high; main firmware notes right
      .communication_format =
          (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false};

  if (i2s_driver_install(I2S_NUM_0, &config, 0, NULL) != ESP_OK) {
    return false;
  }

  const i2s_pin_config_t pins = {
      .bck_io_num = PIN_I2S_SCK,
      .ws_io_num = PIN_I2S_WS,
      .data_out_num = -1,
      .data_in_num = PIN_I2S_SD};

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

// First unused /mic_NNN.wav, or -1 when the card is full of recordings.
static int next_file_index() {
  char name[32];
  for (int i = 0; i < 1000; i++) {
    snprintf(name, sizeof(name), "/mic_%03d.wav", i);
    if (!SD.exists(name)) {
      return i;
    }
  }
  return -1;
}

static void led_blink(int count, int on_ms, int off_ms) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(on_ms);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(off_ms);
  }
}

static void record_file(const char *name) {
  const uint32_t data_bytes =
      TOTAL_SAMPLES * WAV_CHANNELS * (WAV_BITS / 8);
  uint8_t header[44];
  build_wav_header(header, data_bytes);

  File file = SD.open(name, FILE_WRITE);
  if (!file) {
    Serial.printf("ERROR: cannot open %s\n", name);
    return;
  }

  size_t written = file.write(header, sizeof(header));
  if (written != sizeof(header)) {
    Serial.printf("ERROR: header write failed on %s\n", name);
    file.close();
    return;
  }

  Serial.printf("Recording %s (%lu samples, %.1f s)\n", name,
                (unsigned long)TOTAL_SAMPLES, (double)RECORD_SECONDS);
  digitalWrite(PIN_STATUS_LED, HIGH);

  double sum_sq = 0.0;
  int32_t peak = 0;
  uint32_t samples_done = 0;
  uint32_t next_progress = SAMPLE_RATE;  // 1 s

  while (samples_done < TOTAL_SAMPLES) {
    const size_t want = BLOCK_SAMPLES * sizeof(int32_t);
    size_t bytes_read = 0;
    const esp_err_t err =
        i2s_read(I2S_NUM_0, i2s_buffer, want, &bytes_read, portMAX_DELAY);
    if (err != ESP_OK || bytes_read == 0) {
      Serial.printf("ERROR: i2s_read failed (%d)\n", (int)err);
      break;
    }

    size_t frames = bytes_read / sizeof(int32_t);
    if (frames > TOTAL_SAMPLES - samples_done) {
      frames = TOTAL_SAMPLES - samples_done;
    }

    for (size_t i = 0; i < frames; i++) {
      // INMP441: 24-bit data in the high bits of the 32-bit slot.
      int32_t s = i2s_buffer[i] >> 16;
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      pcm_buffer[i] = (int16_t)s;
      sum_sq += (double)s * (double)s;
      const int32_t mag = s < 0 ? -s : s;
      if (mag > peak) peak = mag;
    }

    const size_t out_bytes = frames * sizeof(int16_t);
    if (file.write((const uint8_t *)pcm_buffer, out_bytes) != out_bytes) {
      Serial.printf("ERROR: SD write failed on %s\n", name);
      break;
    }

    samples_done += frames;
    if (samples_done >= next_progress) {
      Serial.printf("  %lu%%\n",
                    (unsigned long)(samples_done * 100ULL / TOTAL_SAMPLES));
      next_progress += SAMPLE_RATE;
    }
  }

  file.close();
  digitalWrite(PIN_STATUS_LED, LOW);

  if (samples_done == TOTAL_SAMPLES) {
    const double rms = sqrt(sum_sq / (double)TOTAL_SAMPLES);
    const double rms_dbfs =
        rms > 0.0 ? 20.0 * log10(rms / 32768.0) : -120.0;
    const double peak_dbfs =
        peak > 0 ? 20.0 * log10((double)peak / 32768.0) : -120.0;
    Serial.printf("Saved %s: peak %.1f dBFS, RMS %.1f dBFS\n", name, peak_dbfs,
                  rms_dbfs);
  } else {
    Serial.printf("WARNING: %s is incomplete (%lu/%lu samples)\n", name,
                  (unsigned long)samples_done, (unsigned long)TOTAL_SAMPLES);
  }
}

static void record_next() {
  const int index = next_file_index();
  if (index < 0) {
    Serial.println("ERROR: no free /mic_NNN.wav file name");
    return;
  }

  char name[32];
  snprintf(name, sizeof(name), "/mic_%03d.wav", index);
  record_file(name);
}

static bool button_pressed() {
  if (digitalRead(PIN_BUTTON) != HIGH) {
    return false;
  }
  delay(30);  // debounce
  return digitalRead(PIN_BUTTON) == HIGH;
}

static void wait_for_release() {
  while (digitalRead(PIN_BUTTON) == HIGH) {
    delay(10);
  }
  delay(50);
}

void setup() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLDOWN);  // PCB switch pulls pin 13 high

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("HS-3 INMP441 case test recorder");
  Serial.printf("Format: %lu Hz, %u-bit mono, %lu s\n",
                (unsigned long)SAMPLE_RATE, WAV_BITS,
                (unsigned long)RECORD_SECONDS);

  if (!i2s_start()) {
    Serial.println("ERROR: I2S init failed");
    while (true) {
      led_blink(1, 100, 900);
    }
  }
  Serial.println("I2S ready (32-bit slots, left channel)");

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("ERROR: SD init failed - check card (FAT32) and CS pin 5");
    while (true) {
      led_blink(2, 100, 100);
    }
  }
  Serial.println("SD ready");

  Serial.println("First recording starts in 3 s...");
  delay(3000);
  record_next();
  Serial.println("Press the button (pin 13 high) for another recording.");
}

void loop() {
  if (button_pressed()) {
    wait_for_release();
    record_next();
    Serial.println("Press the button (pin 13 high) for another recording.");
  }

  // Idle heartbeat: short blink every 2 s.
  digitalWrite(PIN_STATUS_LED, HIGH);
  delay(20);
  digitalWrite(PIN_STATUS_LED, LOW);
  delay(1980);
}
