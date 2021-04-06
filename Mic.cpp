#include "Mic.h"

int audio_start = 0;

int32_t samples[BLOCK_SIZE];
size_t num_bytes_read;
int samples_read;
double loudness;

double real[SAMPLES];
double imag[SAMPLES];
double energy[OCTAVES];

arduinoFFT fft(real, imag, SAMPLES, SAMPLES);

void start_audio() {
  //audio setup
  Serial.println("Configuring I2S...");
  esp_err_t err;
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),      // Receive, not transfer
      .sample_rate = 22627,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,       // Interrupt level 1
      .dma_buf_count = 8,     // number of buffers
      .dma_buf_len = BLOCK_SIZE,      // samples per buffer
      .use_apll = true
  };
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,       // BCKL
      .ws_io_num = 15,        // LRCL
      .data_out_num = -1,     // not used (only for speakers)
      .data_in_num = 4       // DOUT
  };
  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
      Serial.printf("Failed installing driver: %d\n", err);
  } else { //success
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) { //fail
        Serial.printf("Failed setting pin: %d\n", err);
    } else { //success
      audio_start = 1;
    }
  }
  if(audio_start == 1) {
    Serial.println("I2S driver installed.");
  } else {
    Serial.println("Failed to load I2S driver.");
  }
}

void collect_audio_data() {
  esp_err_t err = i2s_read(I2S_PORT,
                           (char *) samples,
                           BLOCK_SIZE,        // the doc says bytes, but its elements.
                           &num_bytes_read,
                           portMAX_DELAY);    // no timeout
  samples_read = num_bytes_read / 8;
  // integer to double
  integerTodouble(samples, real, imag, SAMPLES);
  // apply flat top window, optimal for energy calculations
  fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  // calculate energy in each bin
  calculateEnergy(real, imag, SAMPLES);
  // sum up energy in bin for each octave
  sumEnergy(real, energy, 1, OCTAVES);
  // calculate loudness per octave + A weighted loudness
  loudness = calculateLoudness(energy, aweighting, OCTAVES, 1.0);
  // show loudness
}
