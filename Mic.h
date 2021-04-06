#ifndef MIC_H
#define MIC_H

#include <FreeRTOS.h>
#include <driver/i2s.h> //mic: INMP441
#include <arduinoFFT.h> //mic: INMP441
#include "MicAux.h"
#include "defines.h"

// setup info
extern int audio_start;

extern int32_t samples[BLOCK_SIZE];
extern size_t num_bytes_read;
extern int samples_read;
extern double loudness;

extern double real[];
extern double imag[];
extern double energy[];

extern arduinoFFT fft;

void start_audio();
void collect_audio_data();

#endif
