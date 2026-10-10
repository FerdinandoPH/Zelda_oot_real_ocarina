#ifndef AUDIO_OCARINA_FILTER_H
#define AUDIO_OCARINA_FILTER_H

#include "ultra64/ultratypes.h"

/**
 * Audio Ocarina note-state filter.
 *
 * Converts the stream of raw note states polled from the Port 4 device (one sample per retrace) into accepted note
 * events. It is pure logic with no libultra dependencies so it can also be compiled and tested on the host.
 *
 * Conceptual states (derived from `confirmedNote` / `candidateNote`):
 *   SILENCE            confirmed == NONE, candidate == NONE
 *   CANDIDATE_NOTE     confirmed == NONE, candidate != NONE
 *   CONFIRMED_NOTE     confirmed != NONE, candidate == confirmed
 *   CANDIDATE_CHANGE   confirmed != NONE, candidate != NONE, candidate != confirmed
 *   CANDIDATE_SILENCE  confirmed != NONE, candidate == NONE
 */

// Logical note ids of the Port 4 protocol (see protocol/protocol.md). Note N (1..5) maps to OcarinaButtonIndex N - 1.
#define AOC_NOTE_NONE 0
#define AOC_NOTE_A 1
#define AOC_NOTE_C_DOWN 2
#define AOC_NOTE_C_RIGHT 3
#define AOC_NOTE_C_LEFT 4
#define AOC_NOTE_C_UP 5
#define AOC_NOTE_MAX 5

// Tuning constants, in samples. The ROM polls once per vertical retrace (~16.7 ms on NTSC).
// A note must be stable for this many consecutive samples before it is accepted (~83 ms)
#define AOC_NOTE_STABLE_SAMPLES 5
// Silence must be stable for this many consecutive samples before the same note can be accepted again (~50 ms)
#define AOC_SILENCE_STABLE_SAMPLES 3
// Samples reporting a confidence below this are treated as silence (0 disables the check)
#define AOC_MIN_CONFIDENCE 0

/**
 * State of the note filter. One instance lives on the SI side (padmgr thread) of audio_ocarina_input.c.
 */
typedef struct AudioOcarinaFilter {
    /* 0x00 */ u8 confirmedNote;     ///< Last state accepted as stable: a note, or AOC_NOTE_NONE for silence
    /* 0x01 */ u8 candidateNote;     ///< State currently being timed to see if it stays long enough to be accepted
    /* 0x02 */ u16 candidateSamples; ///< Consecutive samples in which candidateNote has been seen
} AudioOcarinaFilter; // size = 0x4

// Documented in audio_ocarina_filter.c
void AudioOcarinaFilter_Reset(AudioOcarinaFilter* filter);
u8 AudioOcarinaFilter_Step(AudioOcarinaFilter* filter, u8 note, u8 confidence);

#endif
