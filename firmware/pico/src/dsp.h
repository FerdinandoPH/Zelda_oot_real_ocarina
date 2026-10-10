/**
 * @file dsp.h
 *
 * Pico firmware: pitch detection on core 0. Reads the microphone, conditions and decimates the samples, runs the
 * shared detector (aoc_pitch_track_s16) every DSP_HOP samples and publishes the note to the Joybus responder.
 */
#ifndef AOC_FW_DSP_H
#define AOC_FW_DSP_H

#include <stdbool.h>
#include <stdint.h>

#include "aoc_pitch.h"
#include "settings.h"

/** Latest analysis, for the console */
typedef struct {
    AocPitchResult result;
    int transpose;    /**< total: calibration + console octave */
    int decimation;   /**< 1, 2, 4 or 8 */
    uint32_t analyses; /**< analyses run so far */
    uint32_t maxUs;   /**< slowest analysis, microseconds */
} DspStatus;

/** Starts the microphone and the detector with these settings. */
void dsp_init(const Settings* settings);

/** Applies new settings (the detector restarts). */
void dsp_apply_settings(const Settings* settings);

/**
 * Processes the samples that arrived since the last call. Call continuously from the core 0 loop.
 * @return true if an analysis ran (a new result is available)
 */
bool dsp_poll(void);

/** @return false if the microphone has delivered no samples or a constant signal for MIC_FAULT_MS */
bool dsp_mic_ok(void);

const DspStatus* dsp_status(void);

#endif
