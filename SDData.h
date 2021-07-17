#ifndef SDDATA_H
#define SDDATA_H

#include <SD.h> //SD: LVC125A type with AMS1117
#include "TimeFunctions.h"
#include "defines.h"

extern int sd_card;

extern File data_output;
extern File data_pir_output;
//extern File data_audio_output;
extern File data_audio_raw_output;
extern File data_accel_output;
extern int write_n;

void start_sd();
void start_audio_file();
void close_audio_file();
void start_accel_file();
void close_accel_file();
void time_to_file(File * f);
#endif
