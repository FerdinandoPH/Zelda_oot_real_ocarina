/**
 * @file aoc_pitch.c
 *
 * YIN pitch detector and note quantization for the Audio Ocarina plugin. See aoc_pitch.h.
 */
#include "aoc_pitch.h"
#include "aoc_protocol.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define AOC_PITCH_MAX_TAU 1024

static const int sTargetMidi[AOC_NOTE_COUNT] = {
    AOC_PITCH_MIDI_A, AOC_PITCH_MIDI_C_DOWN, AOC_PITCH_MIDI_C_RIGHT, AOC_PITCH_MIDI_C_LEFT, AOC_PITCH_MIDI_C_UP,
};

/* Public functions are documented in aoc_pitch.h */
float aoc_midi_to_freq(float midi) {
    return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

void aoc_pitch_default_config(AocPitchConfig* config, float sampleRate) {
    config->sampleRate = sampleRate;
    config->minFreq = 0.0f; /* 0: derived from the targets */
    config->maxFreq = 0.0f;
    config->minRms = 0.01f;
    config->yinThreshold = 0.20f;
    config->toleranceCents = 60.0f;
    config->transpose = 0;
    /* Off by default: with an ocarina they merge tongued repeats and delay note changes (see README) */
    config->stickyNotes = 0;
    config->holdMs = 0.0f;
    config->analysisHopMs = 0.0f; /* unknown: no dropout bridging */
}

/*
 * The searched range always covers the (transposed) target notes with some margin, so octave shifts
 * work without retuning; minFreq/maxFreq can only widen it.
 */
#define AOC_PITCH_RANGE_LOW_MARGIN 0.75f
#define AOC_PITCH_RANGE_HIGH_MARGIN 1.35f

/** @return low end of the searched frequency range: the lowest target minus a margin, or minFreq if lower */
static float range_min_freq(const AocPitchConfig* c) {
    float autoMin = aoc_midi_to_freq((float)(sTargetMidi[0] + c->transpose)) * AOC_PITCH_RANGE_LOW_MARGIN;

    return (c->minFreq > 0.0f && c->minFreq < autoMin) ? c->minFreq : autoMin;
}

/** @return high end of the searched frequency range: the highest target plus a margin, or maxFreq if higher */
static float range_max_freq(const AocPitchConfig* c) {
    float autoMax =
        aoc_midi_to_freq((float)(sTargetMidi[AOC_NOTE_COUNT - 1] + c->transpose)) * AOC_PITCH_RANGE_HIGH_MARGIN;

    return (c->maxFreq > autoMax) ? c->maxFreq : autoMax;
}

/** @return shortest period (in samples) searched by YIN, from the highest frequency */
static int tau_min(const AocPitchConfig* c) {
    int t = (int)floorf(c->sampleRate / range_max_freq(c));
    return t < 2 ? 2 : t;
}

/** @return longest period (in samples) searched by YIN, from the lowest frequency, capped to AOC_PITCH_MAX_TAU */
static int tau_max(const AocPitchConfig* c) {
    int t = (int)ceilf(c->sampleRate / range_min_freq(c));
    return t > AOC_PITCH_MAX_TAU ? AOC_PITCH_MAX_TAU : t;
}

int aoc_pitch_window_size(const AocPitchConfig* config) {
    /* Integration window equal to the longest period searched, plus that period */
    return 2 * tau_max(config) + 2;
}

uint8_t aoc_pitch_quantize(const AocPitchConfig* config, float freq, float* centsOut) {
    float midi;
    float bestDist = 1e9f;
    float bestCents = 0.0f;
    int best = -1;
    int i;

    if (freq <= 0.0f) {
        if (centsOut != NULL) {
            *centsOut = 0.0f;
        }
        return AOC_NOTE_NONE;
    }

    midi = 69.0f + 12.0f * log2f(freq / 440.0f);
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        float cents = (midi - (float)(sTargetMidi[i] + config->transpose)) * 100.0f;
        float dist = fabsf(cents);

        if (dist < bestDist) {
            bestDist = dist;
            bestCents = cents;
            best = i;
        }
    }

    if (centsOut != NULL) {
        *centsOut = bestCents;
    }
    if (best < 0 || bestDist > config->toleranceCents) {
        return AOC_NOTE_NONE;
    }
    return (uint8_t)(AOC_NOTE_A + best);
}

/*
 * Sample formats. Only the level measurement and the YIN difference function (the O(n^2) part) depend on the sample
 * type; everything after them is shared. The int16 path keeps the inner loop in integers for CPUs without an FPU
 * (RP2040).
 */
typedef struct {
    size_t sampleSize;
    /** Measures the RMS and peak (full scale = 1.0) of `n` samples */
    void (*level)(const void* samples, int n, float* rms, float* peak);
    /** YIN steps 2 and 3: cumulative mean normalized difference for tau 1..tMax+1 into diff[] (diff[0] = 1) */
    void (*difference)(const void* samples, int w, int tMax, float* diff);
} AocSampleOps;

static void level_f32(const void* data, int n, float* rms, float* peak) {
    const float* samples = data;
    double energy = 0.0;
    float p = 0.0f;
    int j;

    for (j = 0; j < n; j++) {
        float a = fabsf(samples[j]);

        energy += (double)samples[j] * samples[j];
        if (a > p) {
            p = a;
        }
    }
    *rms = (float)sqrt(energy / n);
    *peak = p;
}

static void difference_f32(const void* data, int w, int tMax, float* diff) {
    const float* samples = data;
    double running = 0.0;
    int tau;
    int j;

    diff[0] = 1.0f;
    for (tau = 1; tau <= tMax + 1 && tau <= AOC_PITCH_MAX_TAU; tau++) {
        double sum = 0.0;

        for (j = 0; j < w; j++) {
            float d = samples[j] - samples[j + tau];
            sum += (double)d * d;
        }
        running += sum;
        diff[tau] = running > 0.0 ? (float)(sum * tau / running) : 1.0f;
    }
}

static void level_s16(const void* data, int n, float* rms, float* peak) {
    const int16_t* samples = data;
    uint64_t energy = 0;
    int p = 0;
    int j;

    for (j = 0; j < n; j++) {
        int v = samples[j];
        int a = v < 0 ? -v : v;

        energy += (uint32_t)(v * v);
        if (a > p) {
            p = a;
        }
    }
    *rms = sqrtf((float)energy / (float)n) / 32768.0f;
    *peak = (float)p / 32768.0f;
}

static void difference_s16(const void* data, int w, int tMax, float* diff) {
    const int16_t* samples = data;
    uint64_t running = 0;
    int tau;
    int j;

    diff[0] = 1.0f;
    for (tau = 1; tau <= tMax + 1 && tau <= AOC_PITCH_MAX_TAU; tau++) {
        const int16_t* shifted = samples + tau;
        uint64_t sum = 0;

        for (j = 0; j < w; j++) {
            int32_t d = (int32_t)samples[j] - shifted[j];
            uint32_t a = (uint32_t)(d < 0 ? -d : d);

            sum += a * a; /* |d| <= 65535, so a * a fits in 32 bits */
        }
        running += sum;
        diff[tau] = running > 0 ? (float)sum * (float)tau / (float)running : 1.0f;
    }
}

static const AocSampleOps sOpsF32 = { sizeof(float), level_f32, difference_f32 };
static const AocSampleOps sOpsS16 = { sizeof(int16_t), level_s16, difference_s16 };

static void analyze(const AocSampleOps* ops, const AocPitchConfig* config, const void* samples, int count,
                    AocPitchResult* out) {
    float diff[AOC_PITCH_MAX_TAU + 2];
    int tMin = tau_min(config);
    int tMax = tau_max(config);
    int w = tMax; /* integration window */
    int window = aoc_pitch_window_size(config);
    float peak;
    int tau;
    int best = -1;

    out->freq = 0.0f;
    out->rms = 0.0f;
    out->aperiodicity = 1.0f;
    out->cents = 0.0f;
    out->note = AOC_NOTE_NONE;
    out->confidence = 0;
    out->flags = 0;

    if (count < window) {
        return;
    }

    /* Use the most recent samples */
    samples = (const uint8_t*)samples + (size_t)(count - window) * ops->sampleSize;

    ops->level(samples, w + tMax, &out->rms, &peak);
    if (peak >= 0.99f) {
        out->flags |= AOC_FLAG_CLIPPING;
    }
    if (out->rms < config->minRms) {
        out->flags |= AOC_FLAG_LOW_SIGNAL;
        return;
    }

    ops->difference(samples, w, tMax, diff);

    /* Step 4: absolute threshold, take the first dip below it and walk to its local minimum */
    for (tau = tMin; tau <= tMax; tau++) {
        if (diff[tau] < config->yinThreshold) {
            while (tau + 1 <= tMax && diff[tau + 1] < diff[tau]) {
                tau++;
            }
            best = tau;
            break;
        }
    }
    if (best < 0) {
        return;
    }

    /* Step 5: parabolic interpolation */
    {
        float betterTau = (float)best;

        if (best > 1 && best < tMax + 1) {
            float s0 = diff[best - 1];
            float s1 = diff[best];
            float s2 = diff[best + 1];
            float denom = 2.0f * (2.0f * s1 - s2 - s0);

            if (fabsf(denom) > 1e-9f) {
                betterTau = best + (s2 - s0) / denom;
            }
        }
        out->aperiodicity = diff[best];
        out->freq = config->sampleRate / betterTau;
    }

    out->note = aoc_pitch_quantize(config, out->freq, &out->cents);
    if (out->note == AOC_NOTE_NONE) {
        out->flags |= AOC_FLAG_OUT_OF_TUNE;
        return;
    }

    {
        float conf = 1.0f - out->aperiodicity / (config->yinThreshold > 0.0f ? config->yinThreshold : 1.0f);

        if (conf < 0.0f) {
            conf = 0.0f;
        }
        out->confidence = (uint8_t)(conf * 254.0f + 1.0f);
    }
}

void aoc_pitch_analyze(const AocPitchConfig* config, const float* samples, int count, AocPitchResult* out) {
    analyze(&sOpsF32, config, samples, count, out);
}

void aoc_pitch_analyze_s16(const AocPitchConfig* config, const int16_t* samples, int count, AocPitchResult* out) {
    analyze(&sOpsS16, config, samples, count, out);
}

static void track(const AocSampleOps* ops, const AocPitchConfig* config, AocPitchTracker* tracker,
                  const void* samples, int count, AocPitchResult* out) {
    AocPitchConfig relaxed;
    uint8_t held = tracker->note;
    float cents;
    int maxGap;

    if (held == AOC_NOTE_NONE) {
        analyze(ops, config, samples, count, out);
        tracker->note = out->note;
        tracker->gapFrames = 0;
        return;
    }

    relaxed = *config;
    relaxed.yinThreshold = config->yinThreshold * AOC_PITCH_HOLD_YIN_FACTOR;
    if (relaxed.yinThreshold > 0.5f) {
        relaxed.yinThreshold = 0.5f;
    }
    relaxed.minRms = config->minRms * AOC_PITCH_HOLD_RMS_FACTOR;
    analyze(ops, &relaxed, samples, count, out);

    if (out->note != AOC_NOTE_NONE) {
        /* The held note, or a clear move to another target */
        tracker->note = out->note;
        tracker->gapFrames = 0;
        return;
    }

    if (out->rms < relaxed.minRms) {
        /* Real silence ends the note immediately */
        tracker->note = AOC_NOTE_NONE;
        tracker->gapFrames = 0;
        return;
    }

    if (out->freq > 0.0f) {
        /* Pitched, but not on any target within tolerance */
        relaxed.toleranceCents = config->toleranceCents + AOC_PITCH_HOLD_EXTRA_CENTS;
        if (config->stickyNotes || aoc_pitch_quantize(&relaxed, out->freq, &cents) == held) {
            aoc_pitch_quantize(&relaxed, out->freq, &cents);
            out->note = held;
            out->cents = cents;
            out->flags &= (uint8_t)~AOC_FLAG_OUT_OF_TUNE;
            out->confidence = 1;
            tracker->gapFrames = 0;
            return;
        }
        tracker->note = AOC_NOTE_NONE;
        tracker->gapFrames = 0;
        return;
    }

    /* Sound without a usable pitch (breath noise, a crack in the whistle): bridge short gaps */
    maxGap = (config->analysisHopMs > 0.0f) ? (int)(config->holdMs / config->analysisHopMs + 0.5f) : 0;
    if (tracker->gapFrames < maxGap) {
        tracker->gapFrames++;
        out->note = held;
        out->confidence = 1;
        return;
    }
    tracker->note = AOC_NOTE_NONE;
    tracker->gapFrames = 0;
}

void aoc_pitch_track(const AocPitchConfig* config, AocPitchTracker* tracker, const float* samples, int count,
                     AocPitchResult* out) {
    track(&sOpsF32, config, tracker, samples, count, out);
}

void aoc_pitch_track_s16(const AocPitchConfig* config, AocPitchTracker* tracker, const int16_t* samples, int count,
                         AocPitchResult* out) {
    track(&sOpsS16, config, tracker, samples, count, out);
}
