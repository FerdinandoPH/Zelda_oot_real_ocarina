/**
 * @file aoc_pitch.h
 *
 * Monophonic pitch detection (YIN) and quantization to the five Ocarina of Time notes.
 */
#ifndef AOC_PITCH_H
#define AOC_PITCH_H

#include <stdint.h>

/*
 * Sounding pitch of the five in-game ocarina notes, as MIDI note numbers.
 * The game plays OCARINA_PITCH_D4/F4/A4/B4/D5 (include/ocarina.h) on SF0 instrument 52, whose sample
 * (SampleBank_0 Sample115) is tagged unity note 68 (Ab4) but actually sounds at ~Ab5, so every note
 * sounds one octave above its nominal name: D5 F5 A5 B5 D6.
 */
#define AOC_PITCH_MIDI_A 74       /* D5  587.33 Hz */
#define AOC_PITCH_MIDI_C_DOWN 77  /* F5  698.46 Hz */
#define AOC_PITCH_MIDI_C_RIGHT 81 /* A5  880.00 Hz */
#define AOC_PITCH_MIDI_C_LEFT 83  /* B5  987.77 Hz */
#define AOC_PITCH_MIDI_C_UP 86    /* D6 1174.66 Hz */

/** Pitch detector settings. Fill with aoc_pitch_default_config() and adjust. */
typedef struct {
    float sampleRate;     /**< Hz */
    float minFreq;        /**< extra low end of the search range, Hz (0 = automatic from the targets) */
    float maxFreq;        /**< extra high end of the search range, Hz (0 = automatic from the targets) */
    float minRms;         /**< below this the frame is silence (full scale = 1.0) */
    float yinThreshold;   /**< YIN absolute threshold; aperiodicity above it means no pitch */
    float toleranceCents; /**< max distance from a target note */
    int transpose;        /**< semitones added to the targets (e.g. +12 / -12 for other ocarinas) */
    /* Tracking (aoc_pitch_track) */
    int stickyNotes;      /**< keep a held note through off-target pitch until another target is heard */
    float holdMs;         /**< bridge pitch dropouts this long while there is still sound */
    float analysisHopMs;  /**< time between two aoc_pitch_track calls (set by the caller) */
} AocPitchConfig;

/** Result of analysing one window. */
typedef struct {
    float freq;         /**< detected fundamental, 0 if none */
    float rms;          /**< input level of the window */
    float aperiodicity; /**< YIN d'(tau) at the chosen period, 0 = perfectly periodic */
    float cents;        /**< distance to the nearest target note */
    uint8_t note;       /**< AOC_NOTE_* */
    uint8_t confidence; /**< 0..255 */
    uint8_t flags;      /**< AOC_FLAG_* */
} AocPitchResult;

/*
 * Hysteresis applied while a note is already sounding: keeping a note is easier than starting one,
 * so breath dips and small intonation wobbles do not break a held note into two.
 */
#define AOC_PITCH_HOLD_YIN_FACTOR 1.75f    /* yinThreshold multiplier to keep the current note */
#define AOC_PITCH_HOLD_RMS_FACTOR 0.5f     /* minRms multiplier to keep the current note */
#define AOC_PITCH_HOLD_EXTRA_CENTS 25.0f   /* extra tolerance, only for staying on the same note */

/** Memory of aoc_pitch_track() between two analyses. */
typedef struct {
    uint8_t note;  /**< note reported by the previous analysis */
    int gapFrames; /**< consecutive analyses without a usable pitch while holding the note */
} AocPitchTracker;

/**
 * Fills `config` with the default settings (tuned for a real ocarina): automatic search range, minRms 0.01,
 * yinThreshold 0.20, 60 cents of tolerance, no transpose, sticky notes and dropout bridging off.
 */
void aoc_pitch_default_config(AocPitchConfig* config, float sampleRate);

/**
 * @return the minimum number of samples aoc_pitch_analyze() needs for this config: twice the longest period searched
 *         (plus 2), since YIN compares the signal with a copy of itself shifted by up to one period.
 */
int aoc_pitch_window_size(const AocPitchConfig* config);

/**
 * Detects the pitch of the most recent aoc_pitch_window_size() samples and maps it to an ocarina note.
 *
 * Steps: measure the level (RMS; too quiet = silence with AOC_FLAG_LOW_SIGNAL, near full scale sets
 * AOC_FLAG_CLIPPING); run YIN, which finds the period at which the signal best matches a shifted copy of itself
 * (only periods inside the search range are tried, the first one below yinThreshold wins and is refined by parabolic
 * interpolation); convert the period to a frequency and quantize it to the nearest target note
 * (AOC_FLAG_OUT_OF_TUNE if none is close enough). Confidence grows as the signal gets more periodic.
 *
 * @param config  detector settings
 * @param samples mono samples in [-1, 1]
 * @param count   number of samples; fewer than aoc_pitch_window_size() gives an empty result
 * @param out     result (always fully written)
 */
void aoc_pitch_analyze(const AocPitchConfig* config, const float* samples, int count, AocPitchResult* out);

/**
 * aoc_pitch_analyze() for 16-bit samples (full scale = 32768). The difference function runs in integer arithmetic, so
 * it is fast on CPUs without an FPU (the RP2040 in the Pico firmware); results match the float version.
 */
void aoc_pitch_analyze_s16(const AocPitchConfig* config, const int16_t* samples, int count, AocPitchResult* out);

/**
 * aoc_pitch_analyze() with note hysteresis, for continuous input: call it once per analysis hop on the latest samples.
 * The tracker must start zeroed (and be zeroed again when the settings change).
 *
 * Starting a note uses the normal thresholds; keeping one uses relaxed ones (AOC_PITCH_HOLD_*), so breath dips and
 * small intonation wobbles do not split a held note in two. While a note is held:
 *  - a pitch within tolerance of another target switches to that note immediately;
 *  - with stickyNotes, any other pitched sound (vibrato, glides, sloppy intonation when whistling or humming)
 *    keeps the held note;
 *  - without a usable pitch but with sound, the note is kept for up to holdMs;
 *  - real silence (below minRms * AOC_PITCH_HOLD_RMS_FACTOR) ends the note at once, so the same note can be
 *    played again after a short, clean gap.
 *
 * @param config  detector settings (analysisHopMs must be set for holdMs to work)
 * @param tracker state kept between calls
 * @param samples mono samples in [-1, 1], most recent last
 * @param count   number of samples
 * @param out     result; while a note is held through a gap its confidence is 1
 */
void aoc_pitch_track(const AocPitchConfig* config, AocPitchTracker* tracker, const float* samples, int count,
                     AocPitchResult* out);

/** aoc_pitch_track() for 16-bit samples, see aoc_pitch_analyze_s16(). */
void aoc_pitch_track_s16(const AocPitchConfig* config, AocPitchTracker* tracker, const int16_t* samples, int count,
                         AocPitchResult* out);

/**
 * Maps a frequency to the nearest of the five (transposed) target notes.
 *
 * @param config   provides the transpose and toleranceCents
 * @param freq     frequency in Hz (<= 0 means no pitch)
 * @param centsOut optional: receives the signed distance in cents to the nearest target (100 cents = 1 semitone)
 * @return AOC_NOTE_A..AOC_NOTE_C_UP, or AOC_NOTE_NONE if the nearest target is further than toleranceCents
 */
uint8_t aoc_pitch_quantize(const AocPitchConfig* config, float freq, float* centsOut);

/** Converts a MIDI note number (69 = A4 = 440 Hz, 12 per octave, may be fractional) to a frequency in Hz. */
float aoc_midi_to_freq(float midi);

#endif
