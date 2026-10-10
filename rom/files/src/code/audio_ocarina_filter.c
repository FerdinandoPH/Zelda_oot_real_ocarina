/**
 * @file audio_ocarina_filter.c
 *
 * Debounce/stability filter for the Audio Ocarina Port 4 device. See audio_ocarina_filter.h.
 *
 * Rules:
 *  - A sustained note produces exactly one event.
 *  - A different note produces a new event once it has been stable, even without silence in between.
 *  - The same note only produces a new event after silence has been stable for AOC_SILENCE_STABLE_SAMPLES.
 *  - Short intermediate pitches during a transition never reach AOC_NOTE_STABLE_SAMPLES and are discarded.
 */
#include "audio_ocarina_filter.h"

/**
 * Puts the filter back in the SILENCE state: no confirmed note, no candidate being timed.
 *
 * After a reset the next note needs AOC_NOTE_STABLE_SAMPLES stable samples to be accepted, like any new note. Used
 * when listening starts and after an octave change, so nothing heard before is carried over.
 *
 * @param filter filter to reset
 */
void AudioOcarinaFilter_Reset(AudioOcarinaFilter* filter) {
    filter->confirmedNote = AOC_NOTE_NONE;
    filter->candidateNote = AOC_NOTE_NONE;
    filter->candidateSamples = 0;
}

/**
 * Feeds one raw sample (one GET_STATE answer, i.e. one retrace) into the filter and says whether it completes a new
 * note event.
 *
 * The device reports what it hears *right now*, so a held note arrives as the same value many times in a row, and
 * the start of a note or the change between two notes can contain a few samples of noise or of an intermediate
 * pitch. The filter turns that into "the player played note X" exactly once per note:
 *
 *  - `note` equal to the confirmed state: the note is still sounding (or the silence goes on). Any pending candidate
 *    is dropped and nothing is reported.
 *  - `note` different from the confirmed state: it becomes (or stays) the candidate and its run of consecutive
 *    samples is counted. Once the run reaches AOC_NOTE_STABLE_SAMPLES (for a note) or AOC_SILENCE_STABLE_SAMPLES
 *    (for silence) the candidate becomes the confirmed state. Only a newly confirmed *note* is reported; a newly
 *    confirmed silence is not an event, it just allows the same note to be accepted again afterwards.
 *
 * Invalid note ids and, if AOC_MIN_CONFIDENCE is enabled, low-confidence samples count as silence.
 *
 * @param filter     filter state, updated in place
 * @param note       raw note id from the device (AOC_NOTE_NONE..AOC_NOTE_C_UP)
 * @param confidence raw confidence from the device (0..255), only used if AOC_MIN_CONFIDENCE > 0
 * @return the accepted note (AOC_NOTE_A..AOC_NOTE_C_UP) if this sample completes a new note event, else AOC_NOTE_NONE
 */
u8 AudioOcarinaFilter_Step(AudioOcarinaFilter* filter, u8 note, u8 confidence) {
    u16 requiredSamples;

    if (note > AOC_NOTE_MAX) {
        note = AOC_NOTE_NONE;
    }
#if AOC_MIN_CONFIDENCE > 0
    if (confidence < AOC_MIN_CONFIDENCE) {
        note = AOC_NOTE_NONE;
    }
#else
    (void)confidence;
#endif

    if (note == filter->confirmedNote) {
        // Back to (or still in) the confirmed state, drop any pending candidate
        filter->candidateNote = note;
        filter->candidateSamples = 0;
        return AOC_NOTE_NONE;
    }

    if (note != filter->candidateNote) {
        filter->candidateNote = note;
        filter->candidateSamples = 1;
    } else if (filter->candidateSamples < 0xFFFF) {
        filter->candidateSamples++;
    }

    requiredSamples = (note == AOC_NOTE_NONE) ? AOC_SILENCE_STABLE_SAMPLES : AOC_NOTE_STABLE_SAMPLES;

    if (filter->candidateSamples >= requiredSamples) {
        filter->confirmedNote = note;
        filter->candidateSamples = 0;
        return note;
    }

    return AOC_NOTE_NONE;
}
