#ifndef MICAUX_H
#define MICAUX_H

#include <Arduino.h> //mic: INMP441

void print(const char *fmt, ...);
void integerTodouble(int32_t * integer, double *vReal, double *vImag, uint16_t samples);
// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
void calculateEnergy(double *vReal, double *vImag, uint16_t samples);
// sums up energy in bins per octave
void sumEnergy(const double *bins, double *energies, int bin_size, int num_octaves);
double decibel(double v);
// converts energy to logaritmic, returns A-weighted sum
double calculateLoudness(double *energies, const double *weights, int num_octaves, double scale);
void print_level(double energy);

#endif
