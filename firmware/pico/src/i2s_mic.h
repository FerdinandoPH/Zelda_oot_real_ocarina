/**
 * @file i2s_mic.h
 *
 * Pico firmware: INMP441 capture. PIO generates the I2S clocks and reads the data line, DMA copies the words
 * into a ring buffer forever; the caller reads the left-slot samples out of it.
 */
#ifndef AOC_FW_I2S_MIC_H
#define AOC_FW_I2S_MIC_H

#include <stdint.h>

/** Starts the capture at about `sampleRate` Hz. */
void i2s_mic_init(unsigned sdPin, unsigned sckPin, float sampleRate);

/** @return the exact sample rate produced by the PIO clock divider */
float i2s_mic_sample_rate(void);

/**
 * Copies up to `max` new raw left-slot words (24-bit sample in the top bits) into `out`.
 * If the caller fell more than a ring behind, the oldest samples are lost.
 * @return the number of words copied
 */
int i2s_mic_read(uint32_t* out, int max);

#endif
