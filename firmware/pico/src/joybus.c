/**
 * @file joybus.c
 *
 * Pico firmware: Joybus responder. See joybus.h and joybus.pio.
 */
#include "joybus.h"

#include "aoc_device.h"
#include "aoc_protocol.h"
#include "joybus.pio.h"
#include "config.h"

#include "hardware/pio.h"
#include "pico/platform.h"

#define JOYBUS_MAX_TX 4 /* longest command handled (SET_TRANSPOSE is 2 bytes) */

JoybusStats gJoybusStats;

static PIO sPio;
static uint sSm;
static uint sOffset;

/* note | confidence << 8 | flags << 16, written by core 0 in one store */
static volatile uint32_t sNoteState;
static volatile int32_t sRomTranspose;

void joybus_init(unsigned pin) {
    bool ok = pio_claim_free_sm_and_add_program(&joybus_program, &sPio, &sSm, &sOffset);

    hard_assert(ok);
    joybus_program_init(sPio, sSm, sOffset, pin, JOYBUS_PIO_HZ);
}

void joybus_set_note(uint8_t note, uint8_t confidence, uint8_t flags) {
    sNoteState = (uint32_t)note | ((uint32_t)confidence << 8) | ((uint32_t)flags << 16);
}

int joybus_rom_transpose(void) {
    return sRomTranspose;
}

/**
 * @return the length of the command that starts with `cmd`, or 0 if the device does not answer it
 *         (the rest of that frame is then ignored)
 */
static inline int command_length(uint8_t cmd) {
    switch (cmd) {
        case AOC_CMD_STATUS:
        case AOC_CMD_RESET:
        case AOC_CMD_READ_BUTTONS:
        case AOC_CMD_IDENTIFY:
        case AOC_CMD_GET_STATE:
            return 1;
        case AOC_CMD_SET_TRANSPOSE:
            return 2;
        default:
            return 0;
    }
}

/** Answers a complete command: queues the reply and sends the state machine to its transmit routine. */
static void __not_in_flash_func(reply)(const uint8_t* tx, int txLen) {
    AocDeviceState state;
    uint8_t rx[8];
    uint32_t packed = sNoteState;
    uint32_t word = 0;
    int len;
    int i;

    state.connected = 1;
    state.note = (uint8_t)packed;
    state.confidence = (uint8_t)(packed >> 8);
    state.flags = (uint8_t)(packed >> 16);
    state.romTranspose = (int8_t)sRomTranspose;

    len = aoc_device_process(&state, tx, txLen, rx, sizeof(rx));
    if (len <= 0 || len > (int)sizeof(rx)) {
        gJoybusStats.ignored++;
        return;
    }

    /* Bit count - 1, then the bytes packed MSB first */
    pio_sm_put(sPio, sSm, (uint32_t)(len * 8 - 1));
    for (i = 0; i < len; i++) {
        word |= (uint32_t)rx[i] << (24 - 8 * (i & 3));
        if ((i & 3) == 3 || i == len - 1) {
            pio_sm_put(sPio, sSm, word);
            word = 0;
        }
    }
    pio_sm_exec(sPio, sSm, pio_encode_jmp(sOffset + joybus_offset_transmit));

    sRomTranspose = state.romTranspose;
    gJoybusStats.frames++;
    gJoybusStats.lastCommand = tx[0];
    switch (tx[0]) {
        case AOC_CMD_IDENTIFY:
            gJoybusStats.identify++;
            break;
        case AOC_CMD_GET_STATE:
            gJoybusStats.getState++;
            break;
        case AOC_CMD_SET_TRANSPOSE:
            gJoybusStats.setTranspose++;
            break;
        default:
            break;
    }
}

void __not_in_flash_func(joybus_run)(void) {
    uint8_t tx[JOYBUS_MAX_TX];
    int count = 0;
    int need = 0;    /* bytes the current command needs, 0 = not known yet */
    bool skip = false; /* rest of this frame is ignored */

    for (;;) {
        if (pio_interrupt_get(sPio, sSm)) {
            /* End of frame: whatever is left belongs to a frame we did not answer */
            pio_interrupt_clear(sPio, sSm);
            while (!pio_sm_is_rx_fifo_empty(sPio, sSm)) {
                (void)pio_sm_get(sPio, sSm);
            }
            if (count > 0 || skip) {
                gJoybusStats.ignored++;
            }
            count = 0;
            need = 0;
            skip = false;
            continue;
        }
        if (pio_sm_is_rx_fifo_empty(sPio, sSm)) {
            continue;
        }

        {
            uint8_t b = (uint8_t)pio_sm_get(sPio, sSm);

            if (skip) {
                continue;
            }
            if (count == 0) {
                need = command_length(b);
                if (need == 0) {
                    skip = true;
                    gJoybusStats.lastCommand = b;
                    continue;
                }
            }
            tx[count++] = b;
            if (count == need) {
                reply(tx, count);
                count = 0;
                need = 0;
                /* After the reply the program listens again; any stray end-of-frame IRQ is harmless */
            }
        }
    }
}
