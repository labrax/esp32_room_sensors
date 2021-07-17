#include "SDData.h"

int sd_card = 0;

File data_output;
File data_pir_output;
//File data_audio_output;
File data_audio_raw_output;
File data_accel_output;
int write_n;

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
    tick_time_update();
    char main_data_file[32];
    char pir_data_file[32];
    //char audio_data_file[32];
    
    sprintf(main_data_file, "/%04d%02d%02d_%02d%02d%02d_main_data.csv", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
    sprintf(pir_data_file, "/%04d%02d%02d_%02d%02d%02d_pir_data.csv", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
    //sprintf(audio_data_file, "/%04d%02d%02d_%02d%02d%02d_audio_data.csv", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
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
  char audio_wav_data_file[32];
  sprintf(audio_wav_data_file, "/%04d%02d%02d_%02d%02d%02d_audio.raw", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
  data_audio_raw_output = SD.open(audio_wav_data_file, FILE_WRITE);
  if(!data_audio_raw_output) {
    Serial.printf("Failed to open audio file: %s\n", audio_wav_data_file);
  }
}

void start_accel_file() {
  char accel_data_file[32];
  sprintf(accel_data_file, "/%04d%02d%02d_%02d%02d%02d_accel.bin", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
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
  tick_time_update();
  Serial.printf("%04d%02d%02d,%02d:%02d:%02d,",  current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
  if((f != NULL) & (sd_card == 1)) {
    write_n = f->printf("%04d%02d%02d,%02d:%02d:%02d,",  current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
    if(!write_n) {
      sd_card = 0;
    }
  }
}
