#include "SDData.h"

#include "Mic.h"
#include "accel.h"
#include "sensors.h"

int sd_card = 0;

File data_output;
File data_pir_output;
//File data_audio_output;
File data_audio_raw_output;
File data_accel_output;
int write_n;

// "/<YYYYMMDD_HHMMSS><suffix>", with a numeric suffix on collision.
static void build_unique_filename(char *buf, size_t len, const char *suffix) {
  char ts[24];
  format_filename_timestamp(ts, sizeof(ts));
  snprintf(buf, len, "/%s%s", ts, suffix);
  for (int i = 1; SD.exists(buf) && i <= 99; i++) {
    snprintf(buf, len, "/%s_%d%s", ts, i, suffix);
  }
}

void start_sd() {
  //sd card setup
  if(!SD.begin(SD_CARD_CS_PIN)){
      Serial.println("Failed to mount card");
  } else {
    sd_card = 1;
    uint8_t cardType = SD.cardType();
    Serial.print("\nCard Type: ");
    if(cardType == CARD_NONE){
        Serial.println("No card attached");
        sd_card = 0;
        return;
    }
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize();
    int cardSizeInMB = cardSize/(1024 * 1024);
    Serial.print("Card size: ");
    Serial.println(cardSizeInMB);
    uint64_t bytesAvailable = SD.totalBytes();
    int bytesAvailableInMB = bytesAvailable/(1024 * 1024);
    Serial.print("MB available: ");
    Serial.println(bytesAvailableInMB);
    uint64_t bytesUsed = SD.usedBytes();
    int bytesUsedInMB = bytesUsed/(1024 * 1024);
    Serial.print("MB used: ");
    Serial.println(bytesUsedInMB);

    //prepare files
    char main_data_file[48];
    char pir_data_file[48];
    //char audio_data_file[48];

    build_unique_filename(main_data_file, sizeof(main_data_file), "_main_data.csv");
    build_unique_filename(pir_data_file, sizeof(pir_data_file), "_pir_data.csv");
    //build_unique_filename(audio_data_file, sizeof(audio_data_file), "_audio_data.csv");
    Serial.printf("Output files are: %s and %s\n", main_data_file, pir_data_file);
    data_output = SD.open(main_data_file, FILE_WRITE);
    data_pir_output = SD.open(pir_data_file, FILE_WRITE);
    //data_audio_output = SD.open(audio_data_file, FILE_WRITE);

    if(!data_output | !data_pir_output) { //| !data_audio_output
      Serial.println("Failed to open files!");
      sd_card = 0;
    }

    write_n = data_output.print("date,time,dust,photo,temp,humidity,pressure\n");
    if(!write_n) {
      sd_card = 0;
    }
    write_n = data_pir_output.print("date,time,pir\n");
    if(!write_n) {
      sd_card = 0;
    }
    /*write_n = data_audio_output.print("date,time,");
    if(!write_n) {
      sd_card = 0;
    }
    for(int i = 0; i < OCTAVES; i++) {
      write_n = data_audio_output.printf("octave_%d,", i);
      if(!write_n) {
        sd_card = 0;
      }
    }
    write_n = data_audio_output.printf("dB\n");
    if(!write_n) {
      sd_card = 0;
    }*/
  }
}

void start_audio_file() {
  if (sd_card != 1) {
    return;
  }
  char audio_wav_data_file[48];
  build_unique_filename(audio_wav_data_file, sizeof(audio_wav_data_file), "_audio.raw");
  data_audio_raw_output = SD.open(audio_wav_data_file, FILE_WRITE);
  if(!data_audio_raw_output) {
    Serial.printf("Failed to open audio file: %s\n", audio_wav_data_file);
  }
}

void start_accel_file() {
  if (sd_card != 1) {
    return;
  }
  char accel_data_file[48];
  build_unique_filename(accel_data_file, sizeof(accel_data_file), "_accel.bin");
  data_accel_output = SD.open(accel_data_file, FILE_WRITE);
  if(!data_accel_output) {
    Serial.printf("Failed to open accel file: %s\n", accel_data_file);
  }
}

void close_audio_file() {
  data_audio_raw_output.flush();
  data_audio_raw_output.close();
}

void close_accel_file() {
  data_accel_output.flush();
  data_accel_output.close();
}

void time_to_file(File * f) {
  char ts[24];
  format_timestamp(ts, sizeof(ts));
  Serial.printf("%s,", ts);
  if((f != NULL) & (sd_card == 1)) {
    write_n = f->printf("%s,", ts);
    if(!write_n) {
      sd_card = 0;
    }
  }
}

// One per boot: the authoritative record of whether this boot's timestamps are
// trustworthy. Hand-formatted; no JSON dependency.
void write_session_json() {
  if(sd_card != 1) {
    Serial.println("SD not mounted; session metadata skipped.");
    return;
  }

  char path[64];
  char boot_id[40];
  build_unique_filename(path, sizeof(path), "_session.json");
  format_boot_id(boot_id, sizeof(boot_id));

  uint64_t total_bytes = SD.totalBytes();
  uint64_t used_bytes = SD.usedBytes();
  unsigned long total_mb = (unsigned long)(total_bytes / (1024ULL * 1024ULL));
  unsigned long free_mb = (unsigned long)((total_bytes - used_bytes) / (1024ULL * 1024ULL));

  char json[640];
  int n = snprintf(json, sizeof(json),
    "{\n"
    "  \"fw_version\": \"%s\",\n"
    "  \"git_sha\": \"unknown\",\n"
    "  \"boot_id\": \"%s\",\n"
    "  \"reset_reason\": \"%s\",\n"
    "  \"time_source\": \"%s\",\n"
    "  \"time_valid\": %s,\n"
    "  \"sd_total_mb\": %lu,\n"
    "  \"sd_free_mb\": %lu,\n"
    "  \"sensors\": { \"bme280\": %s, \"mpu6050\": %s, \"mic\": %s, \"sd\": true },\n"
    "  \"config\": { \"sensor_period_s\": %d, \"pir_period_s\": %d, \"audio_hz\": %d, \"accel_hz\": %d }\n"
    "}\n",
    FW_VERSION, boot_id, reset_reason_name(), time_source_name(),
    time_valid ? "true" : "false",
    total_mb, free_mb,
    bme_present ? "true" : "false",
    mpu_present ? "true" : "false",
    audio_start == 1 ? "true" : "false",
    SENSOR_TIC_SEC, SENSOR_PIR_TIC_SEC, I2S_SAMPLE_RATE, ACCEL_FREQ);

  if(n < 0 || n >= (int)sizeof(json)) {
    Serial.println("Session metadata truncated; skipping.");
    return;
  }

  File f = SD.open(path, FILE_WRITE);
  if(!f) {
    Serial.printf("Failed to open session file: %s\n", path);
    return;
  }
  f.print(json);
  f.close();
  Serial.printf("Session metadata: %s\n", path);
}
