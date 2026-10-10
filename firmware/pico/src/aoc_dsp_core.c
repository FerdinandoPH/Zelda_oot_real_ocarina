/**
 * @file aoc_dsp_core.c
 *
 * Pico firmware: sample conditioning and decimation. See aoc_dsp_core.h.
 */
#include "aoc_dsp_core.h"

/* Public functions are documented in aoc_dsp_core.h */
int16_t aoc_dsp_condition(AocDspHighPass* hp, uint32_t word, int gainBits) {
    int32_t x = (int32_t)word >> 8; /* 24-bit sample, sign extended */
    int32_t y;

    hp->dc += ((x << AOC_DSP_DC_FRAC) - hp->dc) >> AOC_DSP_DC_SHIFT;
    y = x - (hp->dc >> AOC_DSP_DC_FRAC);

    if (gainBits < 0) {
        gainBits = 0;
    } else if (gainBits > 8) {
        gainBits = 8;
    }
    y >>= 8 - gainBits;
    if (y > 32767) {
        y = 32767;
    } else if (y < -32768) {
        y = -32768;
    }
    return (int16_t)y;
}

int aoc_dsp_decimation(int transpose) {
    int factor = 1;

    while (transpose <= -12 && factor < AOC_DSP_MAX_DECIMATION) {
        factor *= 2;
        transpose += 12;
    }
    return factor;
}

void aoc_dsp_decimate(const int16_t* in, int count, int factor, int16_t* out) {
    int i;
    int j;

    if (factor <= 1) {
        for (i = 0; i < count; i++) {
            out[i] = in[i];
        }
        return;
    }
    for (i = 0; i < count / factor; i++) {
        int32_t sum = 0;

        for (j = 0; j < factor; j++) {
            sum += in[i * factor + j];
        }
        out[i] = (int16_t)(sum / factor);
    }
}
