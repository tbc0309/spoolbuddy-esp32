#pragma once
#include <stdint.h>

/* Bambuddy calibration_factor is grams per ADC count, not counts per gram. */
static inline float sb_weight_from_adc(int32_t raw, int32_t tare, double grams_per_count) {
    return (float)(((double)raw - (double)tare) * grams_per_count);
}
