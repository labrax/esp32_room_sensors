// HS-3 serial bench recorder: ESP32 + INMP441 -> host over UART, no SD.
// Pins match src/esp32_room_sensors/defines.h.
//
// Protocol:
//   Host waits for "HSA_READY" (repeated while idle), sends 'S'.
//   Device replies with "HSA1" + uint32 sample_rate + uint16 channels +
//   uint16 bits + uint32 total_samples + raw 16-bit little-endian PCM.
//   Then it prints "STREAM_DONE" and returns to idle.
//
// Serial is 921600 baud: 32 kHz * 16-bit mono = 64 kB/s, 921600 8N1
// carries about 92 kB/s.

#include <Arduino.h>
#include <driver/i2s.h>

static const int PIN_I2S_SCK    = 14;
static const int PIN_I2S_WS     = 15;
static const int PIN_I2S_SD     = 4;
static const int PIN_BUTTON     = 13;  // active high
static const int PIN_STATUS_LED = 2;

static const uint32_t SAMPLE_RATE    = 32000;
static const uint32_t RECORD_SECONDS = 10;
static const uint32_t TOTAL_SAMPLES  = SAMPLE_RATE * RECORD_SECONDS;
static const size_t   BLOCK_SAMPLES  = 1024;

static int32_t i2s_buffer[BLOCK_SAMPLES];
static int16_t pcm_buffer[BLOCK_SAMPLES];

static uint32_t last_ready_ms = 0;

static void write_u16(uint16_t v) {
  uint8_t b[2] = {(uint8_t)(v & 0xff), (uint8_t)(v >> 8)};
  Serial.write(b, sizeof(b));
}

static void write_u32(uint32_t v) {
  uint8_t b[4] = {(uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff),
                  (uint8_t)((v >> 16) & 0xff), (uint8_t)((v >> 24) & 0xff)};
  Serial.write(b, sizeof(b));
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

static bool button_pressed() {
  if (digitalRead(PIN_BUTTON) != HIGH) {
    return false;
  }
  delay(30);
  return digitalRead(PIN_BUTTON) == HIGH;
}

static void wait_for_release() {
  while (digitalRead(PIN_BUTTON) == HIGH) {
    delay(10);
  }
  delay(50);
}

static void stream_recording() {
  Serial.flush();
  Serial.write("HSA1", 4);
  write_u32(SAMPLE_RATE);
  write_u16(1);
  write_u16(16);
  write_u32(TOTAL_SAMPLES);
  Serial.flush();

  // Give the host time to print "start playback" before the PCM begins.
  delay(1000);

  digitalWrite(PIN_STATUS_LED, HIGH);

  double sum_sq = 0.0;
  int32_t peak = 0;
  uint32_t samples_done = 0;
  bool ok = true;

  while (samples_done < TOTAL_SAMPLES) {
    const size_t want = BLOCK_SAMPLES * sizeof(int32_t);
    size_t bytes_read = 0;
    const esp_err_t err =
        i2s_read(I2S_NUM_0, i2s_buffer, want, &bytes_read, portMAX_DELAY);
    if (err != ESP_OK || bytes_read == 0) {
      ok = false;
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
    Serial.write((const uint8_t *)pcm_buffer, out_bytes);
    samples_done += frames;
  }

  Serial.flush();
  digitalWrite(PIN_STATUS_LED, LOW);

  if (ok && samples_done == TOTAL_SAMPLES) {
    const double rms = sqrt(sum_sq / (double)TOTAL_SAMPLES);
    const double rms_dbfs =
        rms > 0.0 ? 20.0 * log10(rms / 32768.0) : -120.0;
    const double peak_dbfs =
        peak > 0 ? 20.0 * log10((double)peak / 32768.0) : -120.0;
    Serial.printf("STREAM_DONE peak %.1f dBFS RMS %.1f dBFS\n", peak_dbfs,
                  rms_dbfs);
  } else {
    Serial.printf("STREAM_ERROR %lu/%lu\n", (unsigned long)samples_done,
                  (unsigned long)TOTAL_SAMPLES);
  }
}

void setup() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLDOWN);

  Serial.begin(921600);
  delay(100);
  Serial.println("HSA_READY");

  if (!i2s_start()) {
    Serial.println("ERROR: I2S init failed");
    while (true) {
      digitalWrite(PIN_STATUS_LED, HIGH);
      delay(100);
      digitalWrite(PIN_STATUS_LED, LOW);
      delay(900);
    }
  }

  last_ready_ms = millis();
}

void loop() {
  if (Serial.available()) {
    const int c = Serial.read();
    if (c == 'S') {
      stream_recording();
      last_ready_ms = millis();
    }
  }

  if (button_pressed()) {
    wait_for_release();
    stream_recording();
    last_ready_ms = millis();
  }

  if (millis() - last_ready_ms > 2000) {
    Serial.println("HSA_READY");
    last_ready_ms = millis();
  }

  // Idle heartbeat.
  digitalWrite(PIN_STATUS_LED, HIGH);
  delay(20);
  digitalWrite(PIN_STATUS_LED, LOW);
  delay(1980);
}
