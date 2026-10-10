/**
 * @file aoc_device.c
 *
 * Audio Ocarina Port 4 device model: answers the console's Joybus commands. See aoc_device.h.
 */
#include "aoc_device.h"
#include "aoc_protocol.h"

#include <string.h>

/* The Pico firmware places these in RAM (they run on the Joybus path); nothing special elsewhere */
#ifndef AOC_RAMFUNC
#define AOC_RAMFUNC
#endif

AOC_RAMFUNC uint8_t aoc_state_check(uint8_t note, uint8_t confidence, uint8_t flags) {
    return (uint8_t)(note ^ confidence ^ flags ^ AOC_STATE_CHECK_XOR);
}

/**
 * Copies a reply into the console's buffer, truncated to its capacity.
 * @return the full reply length `len` (so the caller can detect a size mismatch)
 */
AOC_RAMFUNC static int reply(const uint8_t* data, int len, uint8_t* rx, int rxCap) {
    memcpy(rx, data, (size_t)(len < rxCap ? len : rxCap));
    return len;
}

/* Documented in aoc_device.h */
AOC_RAMFUNC int aoc_device_process(AocDeviceState* state, const uint8_t* tx, int txLen, uint8_t* rx, int rxCap) {
    uint8_t buf[8];

    if (!state->connected || txLen < 1) {
        return AOC_NO_RESPONSE;
    }

    switch (tx[0]) {
        case AOC_CMD_RESET:
            state->romTranspose = 0;
            /* fallthrough */
        case AOC_CMD_STATUS:
            buf[0] = AOC_DEVICE_TYPE_HI;
            buf[1] = AOC_DEVICE_TYPE_LO;
            buf[2] = AOC_DEVICE_STATUS;
            return reply(buf, AOC_STATUS_RX_SIZE, rx, rxCap);

        case AOC_CMD_READ_BUTTONS:
            memset(buf, 0, AOC_READ_BUTTONS_RX_SIZE);
            return reply(buf, AOC_READ_BUTTONS_RX_SIZE, rx, rxCap);

        case AOC_CMD_IDENTIFY:
            buf[0] = 'O';
            buf[1] = 'C';
            buf[2] = 'A';
            buf[3] = 'R';
            buf[4] = AOC_PROTOCOL_VERSION;
            buf[5] = AOC_CAP_CONFIDENCE | AOC_CAP_TRANSPOSE;
            return reply(buf, AOC_IDENTIFY_RX_SIZE, rx, rxCap);

        case AOC_CMD_GET_STATE: {
            uint8_t note = state->note <= AOC_NOTE_COUNT ? state->note : AOC_NOTE_NONE;
            uint8_t conf = note != AOC_NOTE_NONE ? state->confidence : 0;

            buf[0] = note;
            buf[1] = conf;
            buf[2] = state->flags;
            buf[3] = aoc_state_check(note, conf, state->flags);
            return reply(buf, AOC_GET_STATE_RX_SIZE, rx, rxCap);
        }

        case AOC_CMD_SET_TRANSPOSE: {
            int value;

            if (txLen < 2) {
                return AOC_NO_RESPONSE;
            }
            value = (int8_t)tx[1];
            if (value > AOC_TRANSPOSE_LIMIT) {
                value = AOC_TRANSPOSE_LIMIT;
            } else if (value < -AOC_TRANSPOSE_LIMIT) {
                value = -AOC_TRANSPOSE_LIMIT;
            }
            state->romTranspose = (int8_t)value;
            buf[0] = (uint8_t)state->romTranspose;
            return reply(buf, AOC_SET_TRANSPOSE_RX_SIZE, rx, rxCap);
        }

        default:
            /* Unknown commands (memory pak, EEPROM, ...) are ignored like a real device without that feature */
            return AOC_NO_RESPONSE;
    }
}
