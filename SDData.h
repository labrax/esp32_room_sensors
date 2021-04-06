#ifndef SDDATA_H
#define SDDATA_H

#include <SD.h> //SD: LVC125A type with AMS1117
#include "TimeFunctions.h"
#include "defines.h"

extern int sd_card;

extern File data_output;
extern File data_pir_output;
//extern File data_audio_output;
extern File data_audio_wav_output;
extern int write_n;

void start_sd();
void start_wav_file();
void wavHeader(byte* header, int wavSize);
void close_wav_file();
void time_to_file(File * f);
#endif
