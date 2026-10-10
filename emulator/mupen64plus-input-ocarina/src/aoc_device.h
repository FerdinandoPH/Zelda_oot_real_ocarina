/**
 * @file aoc_device.h
 *
 * Audio Ocarina Port 4 device model (protocol v1).
 * Pure logic: Joybus command bytes in, reply bytes out. Reference behaviour for the future Pico firmware.
 */
#ifndef AOC_DEVICE_H
#define AOC_DEVICE_H

#include <stdint.h>

/** Everything the emulated device needs to answer a command. */
typedef struct {
    int connected;       /**< 0: behave as if nothing is plugged in (no reply to anything) */
    uint8_t note;        /**< AOC_NOTE_*: note heard right now */
    uint8_t confidence;  /**< 0..255 */
    uint8_t flags;       /**< AOC_FLAG_* */
    int8_t romTranspose; /**< semitones requested by the console with SET_TRANSPOSE (updated by SET_TRANSPOSE/RESET) */
} AocDeviceState;

#define AOC_NO_RESPONSE (-1)

/**
 * Answers one Joybus command the way the Port 4 device does (protocol/protocol.md).
 *
 * Standard commands are answered so the game's normal controller code sees "some non-controller device" and leaves
 * it alone: STATUS/RESET report a custom device type with nothing inserted, READ_BUTTONS reports no buttons. The
 * custom commands are IDENTIFY (magic "OCAR", version, capabilities), GET_STATE (note, confidence, flags, check byte)
 * and SET_TRANSPOSE (stores the clamped offset in state->romTranspose and echoes it). RESET also clears the
 * transpose. Unknown commands, or any command while `connected` is 0, get no answer, like an empty port.
 *
 * @param state device state; only romTranspose may be modified
 * @param tx    bytes sent by the console (tx[0] is the command)
 * @param txLen number of bytes in tx
 * @param rx    reply buffer
 * @param rxCap size of rx; longer replies are truncated
 * @return the number of reply bytes the device sends (its natural reply length, possibly > rxCap in which case only
 *         rxCap bytes are written), or AOC_NO_RESPONSE
 */
int aoc_device_process(AocDeviceState* state, const uint8_t* tx, int txLen, uint8_t* rx, int rxCap);

/**
 * Computes the integrity byte of a GET_STATE reply: note ^ confidence ^ flags ^ AOC_STATE_CHECK_XOR. The ROM
 * recomputes it and drops replies that do not match (corrupted transfers). The constant makes an all-zero reply
 * invalid.
 */
uint8_t aoc_state_check(uint8_t note, uint8_t confidence, uint8_t flags);

#endif
