/**
 * @file aoc_mic.h
 *
 * Microphone capture + live pitch analysis. Publishes the current logical note state.
 */
#ifndef AOC_MIC_H
#define AOC_MIC_H

#include <stdint.h>

#include "aoc_pitch.h"

#define AOC_MIC_SAMPLE_RATE 48000
#define AOC_MIC_HOP 512 /* analysis every ~10.7 ms */

/** Microphone capture settings. */
typedef struct {
    const char* deviceName; /**< substring of the capture device name, NULL or "" for the default device */
    AocPitchConfig pitch;   /**< pitch detector settings; pitch.transpose is the total (calibration + console) */
    int debugLog;           /**< log frequency/note/confidence changes to stderr */
} AocMicConfig;

/** Latest analysis result, as published by the audio thread. */
typedef struct {
    uint8_t note;       /**< AOC_NOTE_* */
    uint8_t confidence; /**< 0..255 */
    uint8_t flags;      /**< AOC_FLAG_* */
    float freq;         /**< detected frequency in Hz, 0 if none */
    float cents;        /**< distance to the nearest target note */
    float rms;          /**< input level (full scale = 1.0) */
} AocMicState;

/**
 * Opens the capture device and starts analysing it in the background.
 *
 * Capture runs on miniaudio's audio thread, mono float at AOC_MIC_SAMPLE_RATE. Every AOC_MIC_HOP samples the most
 * recent window is analysed with aoc_pitch_track() and the result becomes the state returned by aoc_mic_get_state().
 * If the named device is not found, the default capture device is used. Does nothing if already running.
 *
 * @param config settings; copied, the caller's struct can be freed afterwards
 * @return 0 on success, -1 if the audio context or device could not be opened or the analysis window is too large
 */
int aoc_mic_start(const AocMicConfig* config);

/** Stops capture, releases the audio device and context, and resets the published state to silence. */
void aoc_mic_stop(void);

/** @return non-zero while capture is running */
int aoc_mic_is_running(void);

/**
 * Copies the latest analysis result. Thread safe (protected by a mutex), called from the emulator thread each time
 * the console polls Port 4.
 */
void aoc_mic_get_state(AocMicState* state);

/**
 * Changes the total transpose (semitones) used by the running analysis. Safe to call from any thread: the value is
 * stored atomically and applied by the audio thread before its next block, which also resets the note tracker.
 */
void aoc_mic_set_transpose(int semitones);

/**
 * Lists the capture devices, calling `callback` once per device with its name and whether it is the system default.
 * @return the number of devices, or -1 on error
 */
int aoc_mic_list_devices(void (*callback)(const char* name, int isDefault, void* user), void* user);

#endif
