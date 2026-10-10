/**
 * @file aoc_dsp_core.h
 *
 * Pico firmware: sample conditioning and decimation between the INMP441 and the pitch detector.
 * Pure integer C with no SDK dependency, so it is also built by the host tests (tests/test_pitch.c).
 */
#ifndef AOC_DSP_CORE_H
#define AOC_DSP_CORE_H

#include <stdint.h>

/** Largest decimation factor aoc_dsp_decimation() returns */
#define AOC_DSP_MAX_DECIMATION 8

/** DC blocker state (the INMP441 output has an offset). Start zeroed. */
typedef struct {
    int32_t dc; /**< running mean of the input, in 24-bit units << AOC_DSP_DC_FRAC */
} AocDspHighPass;

#define AOC_DSP_DC_FRAC 6  /* fraction bits of the DC estimate */
#define AOC_DSP_DC_SHIFT 9 /* time constant 2^9 samples: ~10 Hz cut-off at 32 kHz */

/**
 * Converts one raw I2S word to a 16-bit sample: removes the DC offset and applies the gain.
 *
 * @param hp       DC blocker state
 * @param word     32-bit I2S slot as read from the PIO: 24-bit two's complement sample, MSB aligned
 * @param gainBits 0..8: extra gain in powers of two (0 keeps the 16 most significant bits of the 24)
 * @return the conditioned sample, saturated to int16
 */
int16_t aoc_dsp_condition(AocDspHighPass* hp, uint32_t word, int gainBits);

/**
 * @return the decimation factor (1, 2, 4 or 8) to run the detector at for this total transpose: one halving per
 *         octave down, so the YIN search (whose cost grows with the square of the longest period) stays as cheap at
 *         -2 octaves as at 0. The targets remain far below the Nyquist frequency of the decimated rate.
 */
int aoc_dsp_decimation(int transpose);

/**
 * Block-average decimation: out[i] = mean(in[i*factor .. i*factor+factor-1]).
 * A box filter is a weak anti-alias filter, but the INMP441 signal above a few kHz is small for an ocarina.
 *
 * @param in     input samples, count multiple of factor
 * @param count  number of input samples
 * @param factor 1, 2, 4 or 8
 * @param out    count / factor output samples (may alias `in`)
 */
void aoc_dsp_decimate(const int16_t* in, int count, int factor, int16_t* out);

#endif
