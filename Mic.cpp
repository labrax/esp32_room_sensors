#include "Mic.h"

//based on: https://github.com/0015/ThatProject/blob/master/ESP32_MICROPHONE/ESP32_INMP441_RECORDING/ESP32_INMP441_RECORDING.ino

int audio_start = 0;

//int32_t samples[BLOCK_SIZE];
//size_t num_bytes_read;
//int samples_read;
//double loudness;

int i2s_read_len = I2S_READ_LEN;
int flash_wr_size = 0;
size_t bytes_read;

char i2_read_buff_vector[I2S_READ_LEN];
char flash_write_buff_vector[I2S_READ_LEN];

/*
double real[SAMPLES];
double imag[SAMPLES];
double energy[OCTAVES];
*/

//arduinoFFT fft(real, imag, SAMPLES, SAMPLES);

void start_audio() {
  //audio setup
  Serial.print("Configuring I2S... ");
  esp_err_t err;
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),      // Receive, not transfer
      .sample_rate = I2S_SAMPLE_RATE,
      .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,       // Interrupt level 1
      .dma_buf_count = 16,     // number of buffers
      .dma_buf_len = BLOCK_SIZE,      // samples per buffer
      .use_apll = true
  };
  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  Serial.println("INSTALLED");
  if (err != ESP_OK) {
      Serial.printf("Failed installing driver: %d\n", err);
  } else { //success
    const i2s_pin_config_t pin_config = {
          .bck_io_num = I2S_SCK,       // BCKL
          .ws_io_num = I2S_WS,        // LRCL
          .data_out_num = -1,     // not used (only for speakers)
          .data_in_num = I2S_SD       // DOUT
    };
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

void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len) {
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < len; i += 2) {
        dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
        d_buff[j++] = 0;
        d_buff[j++] = dac_value * 256 / 2048;
    }
}

void i2s_adc(void *arg) {
  char* i2s_read_buff = (char*) i2_read_buff_vector;
  uint8_t* flash_write_buff = (uint8_t*) flash_write_buff_vector;
  
  while(true) {
    if(sd_card == 0) {
      Serial.println("No SD card setup!");
      delay(1000);
    } else if(digitalRead(AUDIO_DISABLE_PIN) == HIGH) {
      digitalWrite(statusLED, HIGH); // power on the LED
      delay(250);
      digitalWrite(statusLED, LOW); // power on the LED
      delay(250);
      digitalWrite(statusLED, HIGH); // power on the LED
      delay(250);
      digitalWrite(statusLED, LOW); // power on the LED
      delay(250);
      delay(1000);
    } else {
      start_wav_file();
      Serial.println("Recording started");
      while (flash_wr_size < FLASH_RECORD_SIZE) {
          //read data from I2S bus, in this case, from ADC.
          i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
          //example_disp_buf((uint8_t*) i2s_read_buff, 64);
          //save original data from I2S(ADC) into flash.
          i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, i2s_read_len);
          data_audio_wav_output.write((const byte*) flash_write_buff, i2s_read_len);
          flash_wr_size += i2s_read_len;
      }
      flash_wr_size = 0;
      close_wav_file();
    }
  }
}

void collect_audio_data() {
  i2s_adc(NULL);

  /*
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
  */
}
