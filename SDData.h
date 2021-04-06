#ifndef SDDATA_H
#define SDDATA_H

#include <SD.h> //SD: LVC125A type with AMS1117
#include "TimeFunctions.h"
#include "defines.h"

extern int sd_card;

extern File data_output;
extern File data_pir_output;
extern File data_audio_output;
extern int write_n;

void start_sd();
void time_to_file(File * f);
#endif
