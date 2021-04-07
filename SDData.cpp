#include "SDData.h"

int sd_card = 0;

File data_output;
File data_pir_output;
//File data_audio_output;
File data_audio_wav_output;
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

void start_wav_file() {
  tick_time_update();
  char audio_wav_data_file[32];
  sprintf(audio_wav_data_file, "/%04d%02d%02d_%02d%02d%02d_audio.wav", current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday, current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
  data_audio_wav_output = SD.open(audio_wav_data_file, FILE_WRITE);
  if(!data_audio_wav_output) {
    Serial.printf("Failed to open wav file: %s\n", audio_wav_data_file);
  }
  byte header[HEADER_SIZE];
  wavHeader(header, FLASH_RECORD_SIZE);

  data_audio_wav_output.write(header, HEADER_SIZE);
}

void wavHeader(byte* header, int wavSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavSize + HEADER_SIZE - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;
  header[21] = 0x00;
  header[22] = 0x01;
  header[23] = 0x00;
  header[24] = 0x80;
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;
  header[33] = 0x00;
  header[34] = 0x10;
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
}

void close_wav_file() {
  data_audio_wav_output.flush();
  data_audio_wav_output.close();
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
