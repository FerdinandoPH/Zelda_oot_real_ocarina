/**
 * @file console.c
 *
 * Pico firmware: USB serial console. See console.h.
 */
#include "console.h"

#include "aoc_protocol.h"
#include "config.h"
#include "dsp.h"
#include "i2s_mic.h"
#include "joybus.h"

#include "pico/bootrom.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 64

static Settings* sSettings;
static char sLine[LINE_MAX];
static int sLineLen;
static bool sLog;
static uint8_t sLoggedNote;
static AocPitchResult sLoggedStart; /* detector result when sLoggedNote started */
static absolute_time_t sLoggedStartTime;
static int32_t sLoggedGapMs;       /* silence before sLoggedNote, -1 if unknown */
static uint8_t sLoggedPrevNote;    /* the note before sLoggedNote */
static absolute_time_t sLoggedEndTime; /* when sLoggedPrevNote ended */
static uint32_t sLoggedIdentify;
static uint32_t sLoggedTranspose;

static const char* const sNoteNames[] = { "-", "A (D)", "C-Down (F)", "C-Right (A)", "C-Left (B)", "C-Up (high D)" };

static const char* note_name(uint8_t note) {
    return note <= AOC_NOTE_COUNT ? sNoteNames[note] : "?";
}

static void print_help(void) {
    printf("ORU (Ocarina Recognition Unit), Audio Ocarina Port 4 protocol v%d\n"
           "  status              detector and Joybus state\n"
           "  log on|off          print every note with its duration, and IDENTIFY / SET_TRANSPOSE\n"
           "  set transpose <n>   calibration in semitones (e.g. -12 for a lower ocarina)\n"
           "  set tolerance <c>   max cents from a note (default 60)\n"
           "  set minrms <x>      input gate, full scale 1.0 (default 0.01)\n"
           "  set yin <x>         pitch strictness (default 0.20)\n"
           "  set gain <0..8>     microphone gain in powers of two (default %d)\n"
           "  save                store the settings in flash (pauses the Joybus for a moment)\n"
           "  defaults            restore the default settings (not saved until 'save')\n"
           "  bootsel             reboot into the UF2 bootloader\n",
           AOC_PROTOCOL_VERSION, DEFAULT_GAIN_BITS);
}

static void print_status(void) {
    const DspStatus* d = dsp_status();
    const AocPitchResult* r = &d->result;

    printf("mic: %s, %.1f Hz sample rate\n", dsp_mic_ok() ? "ok" : "NO AUDIO", (double)i2s_mic_sample_rate());
    printf("note: %s  conf %u  freq %.1f Hz  cents %+.1f  rms %.4f  flags%s%s%s\n", note_name(r->note),
           r->confidence, (double)r->freq, (double)r->cents, (double)r->rms,
           (r->flags & AOC_FLAG_LOW_SIGNAL) ? " low" : "", (r->flags & AOC_FLAG_CLIPPING) ? " clip" : "",
           (r->flags & AOC_FLAG_OUT_OF_TUNE) ? " out-of-tune" : "");
    printf("transpose: calibration %+ld, console %+d, total %+d (decimation x%d)\n", (long)sSettings->transpose,
           joybus_rom_transpose(), d->transpose, d->decimation);
    printf("settings: tolerance %.0f cents, minrms %.4f, yin %.2f, gain %ld\n", (double)sSettings->toleranceCents,
           (double)sSettings->minRms, (double)sSettings->yinThreshold, (long)sSettings->gainBits);
    printf("analyses: %lu, slowest %lu us\n", (unsigned long)d->analyses, (unsigned long)d->maxUs);
    printf("joybus: %lu replies, %lu ignored, IDENTIFY %lu, GET_STATE %lu, SET_TRANSPOSE %lu, last cmd 0x%02lX\n",
           (unsigned long)gJoybusStats.frames, (unsigned long)gJoybusStats.ignored,
           (unsigned long)gJoybusStats.identify, (unsigned long)gJoybusStats.getState,
           (unsigned long)gJoybusStats.setTranspose, (unsigned long)gJoybusStats.lastCommand);
}

/** Parses "set <name> <value>". @return false if the name or value is invalid */
static bool set_value(const char* name, const char* value) {
    char* end;
    float v = strtof(value, &end);

    if (end == value || *end != '\0') {
        return false;
    }
    if (strcmp(name, "transpose") == 0 && v >= -48 && v <= 48) {
        sSettings->transpose = (int32_t)v;
    } else if (strcmp(name, "tolerance") == 0 && v >= 5 && v <= 100) {
        sSettings->toleranceCents = v;
    } else if (strcmp(name, "minrms") == 0 && v >= 0 && v <= 1) {
        sSettings->minRms = v;
    } else if (strcmp(name, "yin") == 0 && v > 0 && v <= 1) {
        sSettings->yinThreshold = v;
    } else if (strcmp(name, "gain") == 0 && v >= 0 && v <= 8) {
        sSettings->gainBits = (int32_t)v;
    } else {
        return false;
    }
    dsp_apply_settings(sSettings);
    return true;
}

static void run_command(char* line) {
    char* cmd = strtok(line, " \t");
    char* arg1 = strtok(NULL, " \t");
    char* arg2 = strtok(NULL, " \t");

    if (cmd == NULL) {
        return;
    }
    if (strcmp(cmd, "help") == 0) {
        print_help();
    } else if (strcmp(cmd, "status") == 0) {
        print_status();
    } else if (strcmp(cmd, "log") == 0 && arg1 != NULL) {
        sLog = strcmp(arg1, "on") == 0;
        sLoggedNote = AOC_NOTE_NONE; /* a note already sounding is timed from now */
        sLoggedPrevNote = AOC_NOTE_NONE;
        printf("log %s\n", sLog ? "on" : "off");
    } else if (strcmp(cmd, "set") == 0 && arg1 != NULL && arg2 != NULL) {
        printf(set_value(arg1, arg2) ? "ok\n" : "invalid setting or value\n");
    } else if (strcmp(cmd, "save") == 0) {
        printf(settings_save(sSettings) ? "saved\n" : "save FAILED\n");
    } else if (strcmp(cmd, "defaults") == 0) {
        settings_defaults(sSettings);
        dsp_apply_settings(sSettings);
        printf("defaults restored (use 'save' to keep them)\n");
    } else if (strcmp(cmd, "bootsel") == 0) {
        printf("rebooting into BOOTSEL\n");
        sleep_ms(50);
        reset_usb_boot(0, 0);
    } else {
        printf("unknown command, type 'help'\n");
    }
}

void console_init(Settings* settings) {
    sSettings = settings;
}

/* The ROM polls once per retrace (60 Hz NTSC) and accepts a note seen in 5 consecutive polls
 * (AOC_NOTE_STABLE_SAMPLES in oot/include/audio_ocarina_filter.h). A note lasting longer than 5 poll periods is
 * always accepted; one lasting 4 to 5 periods only if the polls happen to fall right. Silence must likewise
 * last 3 polls (AOC_SILENCE_STABLE_SAMPLES) before the same note counts again, so a note split by a shorter gap
 * is still one note for the game. */
#define LOG_POLL_US 16683
#define LOG_STABLE_POLLS 5
#define LOG_SILENCE_POLLS 3

/** Prints a note that just ended, with the silence before it, its duration and how the game would see it */
static void log_note_end(uint32_t ms) {
    const AocPitchResult* r = &sLoggedStart;
    uint32_t us = ms * 1000;
    const char* game = us >= LOG_STABLE_POLLS * LOG_POLL_US         ? "  <- game"
                       : us >= (LOG_STABLE_POLLS - 1) * LOG_POLL_US ? "  <- game?"
                                                                   : "";
    const char* joined = "";
    char gap[16] = "      ";

    if (sLoggedGapMs >= 0) {
        uint32_t gapUs = (uint32_t)sLoggedGapMs * 1000;

        snprintf(gap, sizeof(gap), "+%4ld", (long)sLoggedGapMs);
        if (sLoggedPrevNote == sLoggedNote) {
            joined = gapUs < (LOG_SILENCE_POLLS - 1) * LOG_POLL_US ? "  (joined)"
                     : gapUs < LOG_SILENCE_POLLS * LOG_POLL_US     ? "  (joined?)"
                                                                   : "  (repeat)";
        }
    }
    printf("gap %s ms  note %-13s %6.1f Hz  %+4.0f cents  conf %3u  rms %.3f  %5lu ms%s%s\n", gap,
           note_name(sLoggedNote), (double)r->freq, (double)r->cents, r->confidence, (double)r->rms,
           (unsigned long)ms, game, joined);
}

/** Prints each note when it ends, and console events, while the log is on */
static void live_log(void) {
    const AocPitchResult* r = &dsp_status()->result;

    if (r->note != sLoggedNote) {
        absolute_time_t now = get_absolute_time();

        if (sLoggedNote != AOC_NOTE_NONE) {
            log_note_end((uint32_t)(absolute_time_diff_us(sLoggedStartTime, now) / 1000));
            sLoggedPrevNote = sLoggedNote;
            sLoggedEndTime = now;
        }
        if (r->note != AOC_NOTE_NONE) {
            /* Gap only after silence; a note that directly replaces another has none */
            sLoggedGapMs = sLoggedPrevNote == AOC_NOTE_NONE ? -1
                           : sLoggedNote == AOC_NOTE_NONE
                               ? (int32_t)(absolute_time_diff_us(sLoggedEndTime, now) / 1000)
                               : 0;
        }
        sLoggedNote = r->note;
        sLoggedStart = *r;
        sLoggedStartTime = now;
    }
    if (gJoybusStats.identify != sLoggedIdentify) {
        sLoggedIdentify = gJoybusStats.identify;
        printf("console: IDENTIFY (audio mode on)\n");
    }
    if (gJoybusStats.setTranspose != sLoggedTranspose) {
        sLoggedTranspose = gJoybusStats.setTranspose;
        printf("console: SET_TRANSPOSE %+d\n", joybus_rom_transpose());
    }
}

void console_poll(void) {
    int c;

    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (sLineLen > 0) {
                putchar('\n');
                sLine[sLineLen] = '\0';
                sLineLen = 0;
                run_command(sLine);
            }
        } else if ((c == '\b' || c == 0x7F) && sLineLen > 0) {
            sLineLen--;
            printf("\b \b");
        } else if (c >= ' ' && sLineLen < LINE_MAX - 1) {
            sLine[sLineLen++] = (char)c;
            putchar(c);
        }
    }
    if (sLog) {
        live_log();
    }
}
