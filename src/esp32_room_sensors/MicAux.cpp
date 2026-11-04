#include "Mic.h"

void print(const char *fmt, ...) {
    // format it
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // send it to serial
    Serial.write(buf);
}

void integerTodouble(int32_t * integer, double *vReal, double *vImag, uint16_t samples) {
    for (uint16_t i = 0; i < samples; i++) {
        vReal[i] = (integer[i] >> 16) / 10.0;
        vImag[i] = 0.0;
    }
}

// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
void calculateEnergy(double *vReal, double *vImag, uint16_t samples) {
    for (uint16_t i = 0; i < samples; i++) {
        vReal[i] = sq(vReal[i]) + sq(vImag[i]);
        vImag[i] = 0.0;
    }
}

// sums up energy in bins per octave
void sumEnergy(const double *bins, double *energies, int bin_size, int num_octaves) {
    // skip the first bin
    int bin = bin_size;
    for (int octave = 0; octave < num_octaves; octave++) {
        double sum = 0.0;
        for (int i = 0; i < bin_size; i++) {
            sum += bins[bin++];
        }
        energies[octave] = sum;
        bin_size *= 2;
    }
}

double decibel(double v) {
    return 10.0 * log(v) / log(10);
}

// converts energy to logaritmic, returns A-weighted sum
double calculateLoudness(double *energies, const double *weights, int num_octaves, double scale) {
    double sum = 0.0;
    for (int i = 0; i < num_octaves; i++) {
        double energy = scale * energies[i];
        sum += energy * pow(10, weights[i] / 10.0);
        energies[i] = decibel(energy);
    }
    return decibel(sum);
}

void print_level(double energy) {
    static const unsigned char levels[] = { ' ', '.', 'o', 'O', 'X' };

    int index = (int) (energy / 8) - 1;
    if (index < 0) {
        index = 0;
    }
    if (index > 4) {
        index = 4;
    }
    char c = levels[index];
    print("%c,", c);
}
