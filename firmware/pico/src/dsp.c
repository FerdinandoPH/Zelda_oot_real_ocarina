/**
 * @file dsp.c
 *
 * Pico firmware: pitch detection on core 0. See dsp.h.
 */
#include "dsp.h"

#include "aoc_dsp_core.h"
#include "aoc_protocol.h"
#include "config.h"
#include "i2s_mic.h"
#include "joybus.h"

#include "pico/time.h"

#include <string.h>

static Settings sSettings;
static AocPitchConfig sConfig;
static AocPitchTracker sTracker;
static AocDspHighPass sHighPass;
static DspStatus sStatus;
static int sRomTranspose;
static int sWindowRaw; /* raw samples per analysis: window * decimation */

static int16_t sHistory[DSP_HISTORY];
static int sHistoryLen;
static int sSinceAnalysis;
static int16_t sWork[DSP_HISTORY];

/* Mic health */
static absolute_time_t sLastSampleTime;
static absolute_time_t sLastChangeTime;
static uint32_t sLastWord;

/** Rebuilds the detector config from the settings and the console's octave; restarts the tracker */
static void configure(void) {
    int transpose = sSettings.transpose + sRomTranspose;
    int factor = aoc_dsp_decimation(transpose);

    aoc_pitch_default_config(&sConfig, i2s_mic_sample_rate() / (float)factor);
    sConfig.transpose = transpose;
    sConfig.toleranceCents = sSettings.toleranceCents;
    sConfig.minRms = sSettings.minRms;
    sConfig.yinThreshold = sSettings.yinThreshold;
    sConfig.analysisHopMs = DSP_HOP * 1000.0f / i2s_mic_sample_rate();

    sWindowRaw = aoc_pitch_window_size(&sConfig) * factor;
    if (sWindowRaw > DSP_HISTORY) {
        /* Only reachable with an absurd calibration: analyse what fits (the lowest notes will be missed) */
        sWindowRaw = DSP_HISTORY / factor * factor;
    }
    memset(&sTracker, 0, sizeof(sTracker));
    sStatus.transpose = transpose;
    sStatus.decimation = factor;
    joybus_set_note(AOC_NOTE_NONE, 0, 0);
}

void dsp_init(const Settings* settings) {
    i2s_mic_init(MIC_PIN_SD, MIC_PIN_SCK, MIC_SAMPLE_RATE);
    sLastSampleTime = get_absolute_time();
    sLastChangeTime = sLastSampleTime;
    dsp_apply_settings(settings);
}

void dsp_apply_settings(const Settings* settings) {
    sSettings = *settings;
    sRomTranspose = joybus_rom_transpose();
    configure();
}

/** Runs one analysis on the most recent sWindowRaw samples and publishes the note */
static void analyse(void) {
    absolute_time_t start = get_absolute_time();
    int factor = sStatus.decimation;
    int n = sWindowRaw <= sHistoryLen ? sWindowRaw : sHistoryLen / factor * factor;
    AocPitchResult* r = &sStatus.result;
    uint32_t us;

    aoc_dsp_decimate(sHistory + sHistoryLen - n, n, factor, sWork);
    aoc_pitch_track_s16(&sConfig, &sTracker, sWork, n / factor, r);
    joybus_set_note(r->note, r->confidence, r->flags);

    us = (uint32_t)absolute_time_diff_us(start, get_absolute_time());
    if (us > sStatus.maxUs) {
        sStatus.maxUs = us;
    }
    sStatus.analyses++;
}

bool dsp_poll(void) {
    uint32_t words[DSP_HOP];
    int n;
    int i;
    bool ran = false;

    /* The console changed the octave (SET_TRANSPOSE or RESET) */
    if (joybus_rom_transpose() != sRomTranspose) {
        sRomTranspose = joybus_rom_transpose();
        configure();
    }

    while ((n = i2s_mic_read(words, DSP_HOP - sSinceAnalysis)) > 0) {
        absolute_time_t now = get_absolute_time();

        sLastSampleTime = now;
        if (sHistoryLen + n > DSP_HISTORY) {
            int drop = sHistoryLen + n - DSP_HISTORY;

            memmove(sHistory, sHistory + drop, sizeof(sHistory[0]) * (size_t)(sHistoryLen - drop));
            sHistoryLen -= drop;
        }
        for (i = 0; i < n; i++) {
            if (words[i] != sLastWord) {
                sLastWord = words[i];
                sLastChangeTime = now;
            }
            sHistory[sHistoryLen++] = aoc_dsp_condition(&sHighPass, words[i], sSettings.gainBits);
        }

        sSinceAnalysis += n;
        if (sSinceAnalysis >= DSP_HOP) {
            sSinceAnalysis = 0;
            if (sHistoryLen >= sWindowRaw) {
                analyse();
                ran = true;
            }
        }
    }

    if (!dsp_mic_ok()) {
        joybus_set_note(AOC_NOTE_NONE, 0, AOC_FLAG_LOW_SIGNAL);
    }
    return ran;
}

bool dsp_mic_ok(void) {
    absolute_time_t now = get_absolute_time();

    return absolute_time_diff_us(sLastSampleTime, now) < MIC_FAULT_MS * 1000 &&
           absolute_time_diff_us(sLastChangeTime, now) < MIC_FAULT_MS * 1000;
}

const DspStatus* dsp_status(void) {
    return &sStatus;
}
