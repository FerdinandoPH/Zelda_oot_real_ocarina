/* Host tests for pitch detection and quantization */
#include "aoc_dsp_core.h"
#include "aoc_pitch.h"
#include "aoc_protocol.h"
#include "audio_ocarina_filter.h"
#include "test_common.h"

#include <math.h>
#include <stdlib.h>

#define SR 48000.0f
#define LEN 4096

static float sBuf[LEN];

/**
 * Fills sBuf with an ocarina-like test tone: strong fundamental, weak 2nd and 3rd harmonics, optional white noise.
 * @param freq  fundamental in Hz
 * @param amp   peak amplitude (full scale = 1.0)
 * @param noise noise level relative to the tone
 * @param seed  random seed, for reproducible noise
 */
static void tone(float freq, float amp, float noise, unsigned seed) {
    int i;
    srand(seed);
    for (i = 0; i < LEN; i++) {
        float t = i / SR;
        float v = sinf(2.0f * (float)M_PI * freq * t) + 0.15f * sinf(4.0f * (float)M_PI * freq * t) +
                  0.05f * sinf(6.0f * (float)M_PI * freq * t);
        v += noise * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
        sBuf[i] = amp * v / 1.2f;
    }
}

static int16_t sBuf16[LEN];

/** Converts the first `n` samples of sBuf to 16 bits (saturating, like the firmware) into sBuf16 */
static void to_s16(int n) {
    int i;
    for (i = 0; i < n; i++) {
        float v = sBuf[i] * 32768.0f;
        sBuf16[i] = (int16_t)(v > 32767.0f ? 32767.0f : (v < -32768.0f ? -32768.0f : v));
    }
}

#define FW_SR 32000.0f

/**
 * Simulates the firmware capture: `n` INMP441 words at FW_SR of an ocarina-like tone of peak `amp` (full scale 1.0)
 * with a DC offset, through aoc_dsp_condition() with the default gain, into sBuf16. The DC blocker is settled
 * first.
 */
static void fw_capture(float freq, float amp, int n) {
    AocDspHighPass hp = { 0 };
    int i;

    for (i = -32000; i < n; i++) {
        float t = i / FW_SR;
        float v = sinf(2.0f * (float)M_PI * freq * t) + 0.15f * sinf(4.0f * (float)M_PI * freq * t) +
                  0.05f * sinf(6.0f * (float)M_PI * freq * t);
        int32_t x = (int32_t)(amp * v / 1.2f * 8388607.0f) + 60000; /* INMP441-like DC offset */
        int16_t y = aoc_dsp_condition(&hp, (uint32_t)x << 8, 2);

        if (i >= 0) {
            sBuf16[i] = y;
        }
    }
}

int main(void) {
    AocPitchConfig c;
    AocPitchResult r;
    static const int midi[5] = { AOC_PITCH_MIDI_A, AOC_PITCH_MIDI_C_DOWN, AOC_PITCH_MIDI_C_RIGHT,
                                 AOC_PITCH_MIDI_C_LEFT, AOC_PITCH_MIDI_C_UP };
    int i;
    float cents;

    aoc_pitch_default_config(&c, SR);

    /* Quantizer */
    CHECK(aoc_pitch_quantize(&c, 587.33f, &cents) == AOC_NOTE_A && fabsf(cents) < 1.0f);
    CHECK(aoc_pitch_quantize(&c, 698.46f, NULL) == AOC_NOTE_C_DOWN);
    CHECK(aoc_pitch_quantize(&c, 880.0f, NULL) == AOC_NOTE_C_RIGHT);
    CHECK(aoc_pitch_quantize(&c, 987.77f, NULL) == AOC_NOTE_C_LEFT);
    CHECK(aoc_pitch_quantize(&c, 1174.66f, NULL) == AOC_NOTE_C_UP);
    CHECK(aoc_pitch_quantize(&c, 659.26f, NULL) == AOC_NOTE_NONE); /* E5: not an ocarina note */
    CHECK(aoc_pitch_quantize(&c, 783.99f, NULL) == AOC_NOTE_NONE); /* G5 */
    CHECK(aoc_pitch_quantize(&c, 587.33f * powf(2.0f, 40.0f / 1200.0f), NULL) == AOC_NOTE_A); /* +40 cents */
    CHECK(aoc_pitch_quantize(&c, 587.33f * powf(2.0f, 80.0f / 1200.0f), NULL) == AOC_NOTE_NONE); /* +80 */
    c.transpose = -12;
    CHECK(aoc_pitch_quantize(&c, 293.66f, NULL) == AOC_NOTE_A);
    CHECK(aoc_pitch_quantize(&c, 587.33f, NULL) == AOC_NOTE_C_UP); /* D5 is C-Up an octave down */
    c.transpose = 0;

    /* Detector on each target, clean and noisy */
    for (i = 0; i < 5; i++) {
        float f = aoc_midi_to_freq((float)midi[i]);

        tone(f, 0.5f, 0.0f, 1);
        aoc_pitch_analyze(&c, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_A + i);
        CHECK(fabsf(r.cents) < 10.0f);
        CHECK(r.confidence > 128);

        tone(f, 0.3f, 0.05f, 2 + i);
        aoc_pitch_analyze(&c, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_A + i);
    }

    /* Slightly out of tune real instrument (-30 cents) still maps */
    tone(880.0f * powf(2.0f, -30.0f / 1200.0f), 0.5f, 0.0f, 3);
    aoc_pitch_analyze(&c, sBuf, LEN, &r);
    CHECK(r.note == AOC_NOTE_C_RIGHT);

    /* Off-list pitch */
    tone(783.99f, 0.5f, 0.0f, 4);
    aoc_pitch_analyze(&c, sBuf, LEN, &r);
    CHECK(r.note == AOC_NOTE_NONE && (r.flags & AOC_FLAG_OUT_OF_TUNE));

    /* Silence / low level */
    tone(880.0f, 0.001f, 0.0f, 5);
    aoc_pitch_analyze(&c, sBuf, LEN, &r);
    CHECK(r.note == AOC_NOTE_NONE && (r.flags & AOC_FLAG_LOW_SIGNAL));

    /* White noise has no pitch */
    srand(7);
    for (i = 0; i < LEN; i++) {
        sBuf[i] = 0.3f * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
    }
    aoc_pitch_analyze(&c, sBuf, LEN, &r);
    CHECK(r.note == AOC_NOTE_NONE);

    /* Hysteresis: a held note survives a weaker or slightly more out-of-tune frame */
    {
        AocPitchTracker tracker = { 0 };
        float drift = 880.0f * powf(2.0f, -75.0f / 1200.0f); /* outside 60 cents, inside 60+25 */

        tone(880.0f, 0.5f, 0.0f, 8);
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_C_RIGHT);

        tone(drift, 0.5f, 0.0f, 9);
        aoc_pitch_analyze(&c, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_NONE); /* would drop without hysteresis */
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_C_RIGHT);

        tone(880.0f, 0.0085f, 0.0f, 10); /* rms ~0.006: below minRms, above minRms*0.5 */
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_C_RIGHT);

        /* Starting a note still uses the strict thresholds */
        tracker.note = AOC_NOTE_NONE;
        tone(drift, 0.5f, 0.0f, 11);
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_NONE);

        /* An off-target pitch: the default drops the held note, the optional sticky mode keeps it */
        CHECK(c.stickyNotes == 0 && c.holdMs == 0.0f);
        tracker.note = AOC_NOTE_C_RIGHT;
        tone(783.99f, 0.5f, 0.0f, 12); /* G5, 200 cents below A5 */
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_NONE);
        c.stickyNotes = 1;
        tracker.note = AOC_NOTE_C_RIGHT;
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_C_RIGHT);
        c.stickyNotes = 0;

        /* Another target is always taken at once */
        tracker.note = AOC_NOTE_C_RIGHT;
        tone(987.77f, 0.5f, 0.0f, 14); /* B5 */
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_C_LEFT);

        /* Silence still ends the note */
        tracker.note = AOC_NOTE_C_RIGHT;
        tone(880.0f, 0.0005f, 0.0f, 13);
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_NONE);
    }

    /* Whistling/humming: unstable pitch through the ROM filter must give one event per sung note */
    {
        AocPitchTracker tracker = { 0 };
        AudioOcarinaFilter filter;
        int events[8];
        int nEvents = 0;
        int k;
        const float hopMs = 10.67f;
        const float a5 = 880.0f;
        const float b5 = 987.77f;

        /* Optional whistling/humming mode */
        c.analysisHopMs = hopMs;
        c.stickyNotes = 1;
        c.holdMs = 60.0f;
        AudioOcarinaFilter_Reset(&filter);

#define FEED(freqExpr, ampExpr, noiseExpr, frames)                                             \
    for (k = 0; k < (frames); k++) {                                                          \
        float f_ = (freqExpr);                                                                \
        uint8_t ev;                                                                           \
        tone(f_, (ampExpr), (noiseExpr), 100 + k);                                            \
        aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);                                         \
        ev = AudioOcarinaFilter_Step(&filter, r.note, r.confidence);                          \
        if (ev != AOC_NOTE_NONE && nEvents < 8) {                                             \
            events[nEvents++] = ev;                                                           \
        }                                                                                     \
    }

        /* A5 with wide vibrato (+-90 cents, 5 Hz) for ~500 ms */
        FEED(a5 * powf(2.0f, 90.0f * sinf(2.0f * (float)M_PI * 5.0f * k * hopMs / 1000.0f) / 1200.0f), 0.3f, 0.02f,
             48)
        /* a 40 ms breathy dropout (noise only, still loud) */
        for (k = 0; k < 4; k++) {
            uint8_t ev;
            int j;

            for (j = 0; j < LEN; j++) {
                sBuf[j] = 0.1f * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            }
            aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
            ev = AudioOcarinaFilter_Step(&filter, r.note, r.confidence);
            if (ev != AOC_NOTE_NONE && nEvents < 8) {
                events[nEvents++] = ev;
            }
        }
        FEED(a5 * powf(2.0f, -50.0f / 1200.0f), 0.3f, 0.02f, 20)
        /* glide up to B5 through the pitches in between */
        FEED(a5 * powf(2.0f, (200.0f * k / 15.0f) / 1200.0f), 0.3f, 0.02f, 15)
        FEED(b5, 0.3f, 0.02f, 30)
        /* real silence, then B5 again (a deliberately repeated note) */
        FEED(b5, 0.0005f, 0.0f, 8)
        FEED(b5, 0.3f, 0.02f, 30)
#undef FEED

        CHECK(nEvents == 3);
        CHECK(nEvents >= 1 && events[0] == AOC_NOTE_C_RIGHT);
        CHECK(nEvents >= 2 && events[1] == AOC_NOTE_C_LEFT);
        CHECK(nEvents >= 3 && events[2] == AOC_NOTE_C_LEFT);

        /* A dropout longer than holdMs ends the note */
        tracker.note = AOC_NOTE_C_RIGHT;
        tracker.gapFrames = 0;
        for (k = 0; k < 12; k++) {
            int j;

            for (j = 0; j < LEN; j++) {
                sBuf[j] = 0.1f * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            }
            aoc_pitch_track(&c, &tracker, sBuf, LEN, &r);
        }
        CHECK(r.note == AOC_NOTE_NONE);
        c.analysisHopMs = 0.0f;
        c.stickyNotes = 0;
        c.holdMs = 0.0f;
    }

    /* Octave shifts: every note at -2..+2 octaves with the automatic search range */
    {
        int oct;

        for (oct = -2; oct <= 2; oct++) {
            c.transpose = 12 * oct;
            CHECK(aoc_pitch_window_size(&c) <= LEN);
            for (i = 0; i < 5; i++) {
                float f = aoc_midi_to_freq((float)(midi[i] + 12 * oct));

                tone(f, 0.5f, 0.0f, 20 + i);
                aoc_pitch_analyze(&c, sBuf, LEN, &r);
                CHECK(r.note == AOC_NOTE_A + i);
                CHECK(fabsf(r.cents) < 25.0f);
            }
            /* The same pitch an octave away from the selected one is not accepted as the wrong note */
            tone(aoc_midi_to_freq((float)(AOC_PITCH_MIDI_C_RIGHT + 12 * oct)), 0.5f, 0.0f, 30);
            aoc_pitch_analyze(&c, sBuf, LEN, &r);
            CHECK(r.note == AOC_NOTE_C_RIGHT);
        }
        /* Stored config values from older versions only widen the range */
        c.transpose = -24;
        c.minFreq = 250.0f;
        c.maxFreq = 2500.0f;
        tone(aoc_midi_to_freq((float)(AOC_PITCH_MIDI_A - 24)), 0.5f, 0.0f, 31); /* D3, 147 Hz */
        aoc_pitch_analyze(&c, sBuf, LEN, &r);
        CHECK(r.note == AOC_NOTE_A);
        c.transpose = 0;
        c.minFreq = 0.0f;
        c.maxFreq = 0.0f;
    }

    /* 16-bit path (Pico firmware) gives the same results as the float path */
    for (i = 0; i < 5; i++) {
        AocPitchResult rf;

        tone(aoc_midi_to_freq((float)midi[i]), 0.4f, 0.03f, 40 + i);
        to_s16(LEN);
        aoc_pitch_analyze(&c, sBuf, LEN, &rf);
        aoc_pitch_analyze_s16(&c, sBuf16, LEN, &r);
        CHECK(r.note == AOC_NOTE_A + i && r.note == rf.note);
        CHECK(fabsf(r.freq - rf.freq) < 0.5f);
        CHECK(fabsf(r.rms - rf.rms) < 0.001f);
        CHECK(abs((int)r.confidence - (int)rf.confidence) <= 2);

        /* Tracker too */
        {
            AocPitchTracker tracker = { 0 };

            aoc_pitch_track_s16(&c, &tracker, sBuf16, LEN, &r);
            CHECK(r.note == AOC_NOTE_A + i && tracker.note == r.note);
        }
    }
    tone(783.99f, 0.5f, 0.0f, 50);
    to_s16(LEN);
    aoc_pitch_analyze_s16(&c, sBuf16, LEN, &r);
    CHECK(r.note == AOC_NOTE_NONE && (r.flags & AOC_FLAG_OUT_OF_TUNE));
    tone(880.0f, 0.001f, 0.0f, 51);
    to_s16(LEN);
    aoc_pitch_analyze_s16(&c, sBuf16, LEN, &r);
    CHECK(r.note == AOC_NOTE_NONE && (r.flags & AOC_FLAG_LOW_SIGNAL));
    tone(880.0f, 1.2f, 0.0f, 52);
    to_s16(LEN);
    aoc_pitch_analyze_s16(&c, sBuf16, LEN, &r);
    CHECK(r.flags & AOC_FLAG_CLIPPING);

    /*
     * Firmware pipeline: INMP441 words at 24 kHz with a DC offset -> aoc_dsp_condition -> decimation chosen by
     * aoc_dsp_decimation -> aoc_pitch_analyze_s16, for every note at -2..+2 octaves (plus a -12 calibration).
     */
    {
        static const int transposes[] = { -36, -24, -12, 0, 12, 24 };
        int t;

        CHECK(aoc_dsp_decimation(0) == 1 && aoc_dsp_decimation(-11) == 1 && aoc_dsp_decimation(-12) == 2);
        CHECK(aoc_dsp_decimation(-24) == 4 && aoc_dsp_decimation(-36) == 8 && aoc_dsp_decimation(-48) == 8);

        for (t = 0; t < (int)(sizeof(transposes) / sizeof(transposes[0])); t++) {
            AocPitchConfig fc;
            int factor = aoc_dsp_decimation(transposes[t]);

            aoc_pitch_default_config(&fc, FW_SR / factor);
            fc.transpose = transposes[t];
            /* Window cost stays bounded thanks to decimation */
            CHECK(aoc_pitch_window_size(&fc) <= 280);
            for (i = 0; i < 5; i++) {
                float f = aoc_midi_to_freq((float)(midi[i] + transposes[t]));
                int n = aoc_pitch_window_size(&fc) * factor;

                fw_capture(f, 0.02f, n);
                aoc_dsp_decimate(sBuf16, n, factor, sBuf16);
                aoc_pitch_analyze_s16(&fc, sBuf16, n / factor, &r);
                CHECK(r.note == AOC_NOTE_A + i);
                CHECK(fabsf(r.cents) < 25.0f);
                if (r.note != AOC_NOTE_A + i || fabsf(r.cents) >= 25.0f) {
                    printf("  transpose %d note %d: got %d, %.1f Hz, %.1f cents\n", transposes[t], i + 1, r.note,
                           r.freq, r.cents);
                }
            }
        }
    }

    /* DC blocker removes the INMP441 offset and gain bits scale the output */
    {
        AocDspHighPass hp = { 0 };
        int16_t s = 0;

        for (i = 0; i < 24000; i++) {
            s = aoc_dsp_condition(&hp, (uint32_t)(40000 << 8), 2);
        }
        CHECK(abs(s) <= 2);
        CHECK(aoc_dsp_condition(&(AocDspHighPass){ 0 }, (uint32_t)(0x7FFFFF << 8), 8) == 32767);
        CHECK(aoc_dsp_condition(&(AocDspHighPass){ 0 }, (uint32_t)(-0x800000 * 256), 8) == -32768);
        CHECK(abs(aoc_dsp_condition(&(AocDspHighPass){ 0 }, (uint32_t)(0x10000 << 8), 0) - 0x100) <= 1);
    }

    /* Too short buffer */
    aoc_pitch_analyze(&c, sBuf, 10, &r);
    CHECK(r.note == AOC_NOTE_NONE);

    return TEST_SUMMARY();
}
