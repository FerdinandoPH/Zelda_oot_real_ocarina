/**
 * @file aoc_mic_probe.c
 *
 * aoc-mic-probe: runs the plugin's microphone capture and pitch detector standalone, to tune the
 * detector without the emulator.
 *
 * It samples the detector state at 60 Hz, like the ROM polling Port 4 once per retrace, and runs the
 * ROM's own stability filter (oot/src/code/audio_ocarina_filter.c) on it. Lines starting with ">>" are
 * the note events the game would register; with --raw it also prints every raw detector change.
 *
 *   aoc-mic-probe [--list] [--raw] [--log FILE] [--device NAME] [--octave N] [--transpose N]
 *                 [--tolerance CENTS] [--min-rms X] [--yin X] [--sticky] [--hold MS]
 */
#include "aoc_mic.h"
#include "aoc_protocol.h"
#include "audio_ocarina_filter.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define POLL_NS 16683333L /* one NTSC retrace */

static volatile sig_atomic_t sQuit = 0;

static const char* sNoteNames[] = { "-", "A (D5)", "C-Down (F5)", "C-Right (A5)", "C-Left (B5)", "C-Up (D6)" };

/** SIGINT/SIGTERM handler: asks the main loop to stop so the log file is flushed and the microphone released. */
static void on_signal(int sig) {
    (void)sig;
    sQuit = 1;
}

/** aoc_mic_list_devices() callback for --list: prints one capture device, the default one marked with '*'. */
static void print_device(const char* name, int isDefault, void* user) {
    (void)user;
    printf("%s %s\n", isDefault ? "*" : " ", name);
}

/** @return readable name of a note id, with the pitch the game plays for it */
static const char* note_name(uint8_t note) {
    return sNoteNames[note <= AOC_NOTE_COUNT ? note : 0];
}

/** @return milliseconds elapsed since `start` (monotonic clock) */
static double elapsed_ms(const struct timespec* start) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start->tv_sec) * 1000.0 + (now.tv_nsec - start->tv_nsec) / 1e6;
}

/**
 * Parses the options, starts the microphone and, until Ctrl+C, samples the detector state every NTSC retrace,
 * runs it through the ROM's filter and prints (and optionally logs to CSV) raw changes and accepted note events.
 * @return 0 on normal exit, 1 on error (bad option, log file or microphone)
 */
int main(int argc, char** argv) {
    AocMicConfig config;
    AocMicState state;
    AudioOcarinaFilter filter;
    uint8_t lastNote = 0xFF;
    int showRaw = 0;
    FILE* log = NULL;
    unsigned long events = 0;
    struct timespec start;
    struct timespec next;
    int i;

    memset(&config, 0, sizeof(config));
    aoc_pitch_default_config(&config.pitch, AOC_MIC_SAMPLE_RATE);

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            return aoc_mic_list_devices(print_device, NULL) < 0 ? 1 : 0;
        } else if (strcmp(argv[i], "--raw") == 0) {
            showRaw = 1;
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log = fopen(argv[++i], "w");
            if (log == NULL) {
                perror("--log");
                return 1;
            }
            fprintf(log, "ms,note,freq,cents,confidence,rms,flags,event\n");
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            config.deviceName = argv[++i];
        } else if (strcmp(argv[i], "--transpose") == 0 && i + 1 < argc) {
            config.pitch.transpose += atoi(argv[++i]);
        } else if (strcmp(argv[i], "--octave") == 0 && i + 1 < argc) {
            config.pitch.transpose += 12 * atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tolerance") == 0 && i + 1 < argc) {
            config.pitch.toleranceCents = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--min-rms") == 0 && i + 1 < argc) {
            config.pitch.minRms = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--yin") == 0 && i + 1 < argc) {
            config.pitch.yinThreshold = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--sticky") == 0) {
            config.pitch.stickyNotes = 1;
        } else if (strcmp(argv[i], "--hold") == 0 && i + 1 < argc) {
            config.pitch.holdMs = (float)atof(argv[++i]);
        } else {
            fprintf(stderr,
                    "usage: %s [--list] [--raw] [--log FILE] [--device NAME] [--octave N] [--transpose N]\n"
                    "          [--tolerance CENTS] [--min-rms X] [--yin X] [--sticky] [--hold MS]\n",
                    argv[0]);
            return 2;
        }
    }

    signal(SIGINT, on_signal);
    if (aoc_mic_start(&config) != 0) {
        return 1;
    }
    printf("Listening, Ctrl+C to quit. Targets: D5 F5 A5 B5 D6 transposed by %d semitones.\n"
           "'>>' lines are the notes the game would register (ROM filter: %d polls to accept, %d to re-arm).\n",
           config.pitch.transpose, AOC_NOTE_STABLE_SAMPLES, AOC_SILENCE_STABLE_SAMPLES);

    AudioOcarinaFilter_Reset(&filter);
    clock_gettime(CLOCK_MONOTONIC, &start);
    next = start;

    while (!sQuit) {
        uint8_t event;
        double ms;

        aoc_mic_get_state(&state);
        event = AudioOcarinaFilter_Step(&filter, state.note, state.confidence);
        ms = elapsed_ms(&start);

        if (showRaw && state.note != lastNote) {
            printf("   %8.0f ms  raw %-14s freq=%7.1f Hz  cents=%+4.0f  conf=%3u  rms=%.3f  flags=0x%02X\n", ms,
                   note_name(state.note), state.freq, state.cents, state.confidence, state.rms, state.flags);
        }
        if (event != AOC_NOTE_NONE) {
            events++;
            printf(">> %8.0f ms  #%lu %s\n", ms, events, note_name(event));
        }
        if (log != NULL) {
            fprintf(log, "%.1f,%u,%.1f,%.0f,%u,%.4f,%u,%u\n", ms, state.note, state.freq, state.cents,
                    state.confidence, state.rms, state.flags, event);
        }
        fflush(stdout);
        lastNote = state.note;

        next.tv_nsec += POLL_NS;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    if (log != NULL) {
        fclose(log);
    }
    aoc_mic_stop();
    return 0;
}
