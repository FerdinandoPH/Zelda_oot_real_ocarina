/* Host tests for the ROM-side note filter (oot/src/code/audio_ocarina_filter.c) */
#include "audio_ocarina_filter.h"
#include "test_common.h"

#include <string.h>

#define N AOC_NOTE_NONE
#define A AOC_NOTE_A
#define F AOC_NOTE_C_DOWN
#define R AOC_NOTE_C_RIGHT

/**
 * Feeds `count` identical samples of `note` to the filter (like `count` retraces of GET_STATE) and appends the
 * accepted note events to `out`.
 */
static void feed(AudioOcarinaFilter* f, u8 note, int count, u8* out, int* outLen) {
    int i;
    for (i = 0; i < count; i++) {
        u8 ev = AudioOcarinaFilter_Step(f, note, 200);
        if (ev != AOC_NOTE_NONE) {
            out[(*outLen)++] = ev;
        }
    }
}

int main(void) {
    AudioOcarinaFilter f;
    u8 ev[64];
    int n;

    /* D. Held note -> one event */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 200, ev, &n);
    CHECK(n == 1 && ev[0] == A);

    /* Accepted exactly after AOC_NOTE_STABLE_SAMPLES */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, AOC_NOTE_STABLE_SAMPLES - 1, ev, &n);
    CHECK(n == 0);
    feed(&f, A, 1, ev, &n);
    CHECK(n == 1);

    /* E. Same note separated by stable silence -> two events */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 10, ev, &n);
    feed(&f, N, AOC_SILENCE_STABLE_SAMPLES, ev, &n);
    feed(&f, A, 10, ev, &n);
    CHECK(n == 2 && ev[0] == A && ev[1] == A);

    /* Silence shorter than the re-arm time does not split a held note */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 10, ev, &n);
    feed(&f, N, AOC_SILENCE_STABLE_SAMPLES - 1, ev, &n);
    feed(&f, A, 10, ev, &n);
    CHECK(n == 1);

    /* Direct change to another note (legato) -> new event without silence */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 10, ev, &n);
    feed(&f, R, 10, ev, &n);
    CHECK(n == 2 && ev[0] == A && ev[1] == R);

    /* Transition glitch: brief intermediate pitch is discarded */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 10, ev, &n);
    feed(&f, F, AOC_NOTE_STABLE_SAMPLES - 1, ev, &n);
    feed(&f, R, 10, ev, &n);
    CHECK(n == 2 && ev[0] == A && ev[1] == R);

    /* Glitch back to the confirmed note does not re-trigger it */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 10, ev, &n);
    feed(&f, F, 2, ev, &n);
    feed(&f, A, 10, ev, &n);
    CHECK(n == 1);

    /* Noise shorter than acceptance from silence produces nothing */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, A, 2, ev, &n);
    feed(&f, N, 2, ev, &n);
    feed(&f, F, 3, ev, &n);
    feed(&f, N, 20, ev, &n);
    CHECK(n == 0);

    /* Out-of-range note ids are silence */
    AudioOcarinaFilter_Reset(&f);
    n = 0;
    feed(&f, 9, 20, ev, &n);
    CHECK(n == 0);

    /* A full song: Zelda's Lullaby = C-Left C-Up C-Right x2 with gaps */
    {
        static const u8 song[] = { AOC_NOTE_C_LEFT, AOC_NOTE_C_UP, AOC_NOTE_C_RIGHT,
                                   AOC_NOTE_C_LEFT, AOC_NOTE_C_UP, AOC_NOTE_C_RIGHT };
        int i;

        AudioOcarinaFilter_Reset(&f);
        n = 0;
        for (i = 0; i < 6; i++) {
            feed(&f, song[i], 20, ev, &n);
            feed(&f, N, 5, ev, &n);
        }
        CHECK(n == 6 && memcmp(ev, song, 6) == 0);
    }

    return TEST_SUMMARY();
}
