/**
 * @file settings.h
 *
 * Pico firmware: detector settings, kept in the last flash sector.
 */
#ifndef AOC_FW_SETTINGS_H
#define AOC_FW_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t transpose;    /**< calibration, semitones added to the targets (added to the console's octave) */
    float toleranceCents; /**< max distance from a target note */
    float minRms;         /**< input gate (full scale = 1.0) */
    float yinThreshold;   /**< pitch strictness */
    int32_t gainBits;     /**< 0..8, gain in powers of two on the 24-bit samples */
} Settings;

/** Fills `s` with the defaults: those of aoc_pitch_default_config() and DEFAULT_GAIN_BITS. */
void settings_defaults(Settings* s);

/** Loads the saved settings. @return false (and the defaults) if none are saved or they are corrupt */
bool settings_load(Settings* s);

/**
 * Saves the settings to flash. Core 1 is paused for the few milliseconds this takes, so the console gets no
 * answers meanwhile (it would leave audio mode if it was listening).
 * @return true on success
 */
bool settings_save(const Settings* s);

#endif
