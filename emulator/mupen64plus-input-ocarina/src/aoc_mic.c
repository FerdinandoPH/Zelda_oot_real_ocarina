/**
 * @file aoc_mic.c
 *
 * Microphone capture (miniaudio) feeding the pitch tracker on the audio thread. See aoc_mic.h.
 */
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include "aoc_mic.h"
#include "aoc_protocol.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define AOC_MIC_HISTORY 4096

static ma_context sContext;
static ma_device sDevice;
static int sContextInit = 0;
static int sRunning = 0;

static AocMicConfig sConfig;
static char sDeviceName[256];

/* Audio thread only */
static float sHistory[AOC_MIC_HISTORY];
static int sHistoryLen = 0;
static int sSinceAnalysis = 0;
static int sWindow = 0;
static AocPitchTracker sTracker;
static int sAppliedTranspose = 0;

/* Written by any thread, applied by the audio thread */
static atomic_int sRequestedTranspose = 0;

/* Shared */
static pthread_mutex_t sStateLock = PTHREAD_MUTEX_INITIALIZER;
static AocMicState sState;

/**
 * Makes an analysis result visible to the emulator thread (under sStateLock) and, with debugLog, logs note changes.
 * Audio thread only.
 */
static void publish(const AocPitchResult* r) {
    AocMicState prev;

    pthread_mutex_lock(&sStateLock);
    prev = sState;
    sState.note = r->note;
    sState.confidence = r->confidence;
    sState.flags = r->flags;
    sState.freq = r->freq;
    sState.cents = r->cents;
    sState.rms = r->rms;
    pthread_mutex_unlock(&sStateLock);

    if (sConfig.debugLog && prev.note != r->note) {
        fprintf(stderr, "[ocarina] mic note=%u freq=%.1fHz cents=%+.0f conf=%u rms=%.3f flags=0x%02X\n", r->note,
                r->freq, r->cents, r->confidence, r->rms, r->flags);
    }
}

/**
 * miniaudio capture callback, runs on the audio thread for every block of recorded samples (~10 ms).
 *
 * First applies a transpose change requested with aoc_mic_set_transpose() (it changes the search range, so the window
 * size is recomputed and the tracker reset). Then appends the samples to the sHistory ring (when full, the oldest
 * AOC_MIC_HOP samples are discarded) and, every AOC_MIC_HOP samples once enough history exists, runs the pitch tracker
 * on the history and publishes the result.
 *
 * @param device     miniaudio device (unused)
 * @param output     playback buffer (unused, capture only)
 * @param input      `frameCount` mono float samples
 * @param frameCount number of samples in `input`
 */
static void data_callback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    const float* in = (const float*)input;
    ma_uint32 i;

    (void)device;
    (void)output;
    if (in == NULL) {
        return;
    }

    {
        int transpose = atomic_load(&sRequestedTranspose);

        if (transpose != sAppliedTranspose) {
            sAppliedTranspose = transpose;
            sConfig.pitch.transpose = transpose;
            sWindow = aoc_pitch_window_size(&sConfig.pitch);
            memset(&sTracker, 0, sizeof(sTracker));
            if (sConfig.debugLog) {
                fprintf(stderr, "[ocarina] mic transpose %+d semitones\n", transpose);
            }
        }
    }

    for (i = 0; i < frameCount; i++) {
        if (sHistoryLen == AOC_MIC_HISTORY) {
            memmove(sHistory, sHistory + AOC_MIC_HOP, sizeof(float) * (AOC_MIC_HISTORY - AOC_MIC_HOP));
            sHistoryLen -= AOC_MIC_HOP;
        }
        sHistory[sHistoryLen++] = in[i];

        if (++sSinceAnalysis >= AOC_MIC_HOP && sHistoryLen >= sWindow) {
            AocPitchResult r;

            sSinceAnalysis = 0;
            aoc_pitch_track(&sConfig.pitch, &sTracker, sHistory, sHistoryLen, &r);
            publish(&r);
        }
    }
}

/**
 * Initializes the miniaudio context (the connection to the system audio backend: PulseAudio, ALSA...) if needed.
 * @return 0 on success, -1 on error
 */
static int ensure_context(void) {
    if (!sContextInit) {
        if (ma_context_init(NULL, 0, NULL, &sContext) != MA_SUCCESS) {
            return -1;
        }
        sContextInit = 1;
    }
    return 0;
}

/* Public functions are documented in aoc_mic.h */
int aoc_mic_list_devices(void (*callback)(const char* name, int isDefault, void* user), void* user) {
    ma_device_info* captureInfos;
    ma_uint32 captureCount;
    ma_uint32 i;

    if (ensure_context() != 0) {
        return -1;
    }
    if (ma_context_get_devices(&sContext, NULL, NULL, &captureInfos, &captureCount) != MA_SUCCESS) {
        return -1;
    }
    for (i = 0; i < captureCount; i++) {
        callback(captureInfos[i].name, captureInfos[i].isDefault, user);
    }
    return (int)captureCount;
}

int aoc_mic_start(const AocMicConfig* config) {
    ma_device_config deviceConfig;
    ma_device_info* captureInfos;
    ma_uint32 captureCount;
    ma_uint32 i;
    const ma_device_id* deviceId = NULL;

    if (sRunning) {
        return 0;
    }

    sConfig = *config;
    sConfig.pitch.analysisHopMs = AOC_MIC_HOP * 1000.0f / AOC_MIC_SAMPLE_RATE;
    /* The config's transpose is the total (calibration + console request); later changes use set_transpose */
    atomic_store(&sRequestedTranspose, sConfig.pitch.transpose);
    sAppliedTranspose = sConfig.pitch.transpose;
    sDeviceName[0] = '\0';
    if (config->deviceName != NULL) {
        snprintf(sDeviceName, sizeof(sDeviceName), "%s", config->deviceName);
    }
    sConfig.deviceName = sDeviceName;
    sConfig.pitch.sampleRate = AOC_MIC_SAMPLE_RATE;
    sWindow = aoc_pitch_window_size(&sConfig.pitch);
    if (sWindow > AOC_MIC_HISTORY) {
        fprintf(stderr, "[ocarina] analysis window too large (%d samples), raise MinFrequency\n", sWindow);
        return -1;
    }
    sHistoryLen = 0;
    sSinceAnalysis = 0;
    memset(&sTracker, 0, sizeof(sTracker));
    memset(&sState, 0, sizeof(sState));

    if (ensure_context() != 0) {
        fprintf(stderr, "[ocarina] could not initialize audio context\n");
        return -1;
    }

    if (sDeviceName[0] != '\0' &&
        ma_context_get_devices(&sContext, NULL, NULL, &captureInfos, &captureCount) == MA_SUCCESS) {
        for (i = 0; i < captureCount; i++) {
            if (strstr(captureInfos[i].name, sDeviceName) != NULL) {
                deviceId = &captureInfos[i].id;
                break;
            }
        }
        if (deviceId == NULL) {
            fprintf(stderr, "[ocarina] capture device '%s' not found, using the default device\n", sDeviceName);
        }
    }

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.pDeviceID = deviceId;
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 1;
    deviceConfig.sampleRate = AOC_MIC_SAMPLE_RATE;
    deviceConfig.periodSizeInMilliseconds = 10;
    deviceConfig.dataCallback = data_callback;

    if (ma_device_init(&sContext, &deviceConfig, &sDevice) != MA_SUCCESS) {
        fprintf(stderr, "[ocarina] could not open the capture device\n");
        return -1;
    }
    if (ma_device_start(&sDevice) != MA_SUCCESS) {
        fprintf(stderr, "[ocarina] could not start the capture device\n");
        ma_device_uninit(&sDevice);
        return -1;
    }

    sRunning = 1;
    fprintf(stderr, "[ocarina] microphone: %s\n", sDevice.capture.name);
    return 0;
}

void aoc_mic_stop(void) {
    if (sRunning) {
        ma_device_uninit(&sDevice);
        sRunning = 0;
    }
    if (sContextInit) {
        ma_context_uninit(&sContext);
        sContextInit = 0;
    }
    pthread_mutex_lock(&sStateLock);
    memset(&sState, 0, sizeof(sState));
    pthread_mutex_unlock(&sStateLock);
}

void aoc_mic_set_transpose(int semitones) {
    atomic_store(&sRequestedTranspose, semitones);
}

int aoc_mic_is_running(void) {
    return sRunning;
}

void aoc_mic_get_state(AocMicState* state) {
    pthread_mutex_lock(&sStateLock);
    *state = sState;
    pthread_mutex_unlock(&sStateLock);
}
