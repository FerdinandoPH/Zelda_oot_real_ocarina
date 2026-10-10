/**
 * @file aoc_protocol.h
 *
 * Audio Ocarina Port 4 protocol v1 - shared constants.
 * Normative description: protocol/protocol.md. Must match oot/include/audio_ocarina_input.h.
 */
#ifndef AOC_PROTOCOL_H
#define AOC_PROTOCOL_H

#define AOC_PROTOCOL_VERSION 1

/* Standard Joybus commands the device must also answer */
#define AOC_CMD_STATUS 0x00
#define AOC_CMD_READ_BUTTONS 0x01
#define AOC_CMD_RESET 0xFF

/* Custom commands */
#define AOC_CMD_SET_TRANSPOSE 0x4D
#define AOC_CMD_GET_STATE 0x4E
#define AOC_CMD_IDENTIFY 0x4F

#define AOC_STATUS_RX_SIZE 3
#define AOC_READ_BUTTONS_RX_SIZE 4
#define AOC_IDENTIFY_RX_SIZE 6
#define AOC_GET_STATE_RX_SIZE 4
#define AOC_SET_TRANSPOSE_RX_SIZE 1
#define AOC_TRANSPOSE_LIMIT 24 /* SET_TRANSPOSE is clamped to +-this many semitones */

/* Device type reported to STATUS/RESET: not CONT_TYPE_NORMAL, so games ignore it */
#define AOC_DEVICE_TYPE_HI 0x4F /* 'O' */
#define AOC_DEVICE_TYPE_LO 0x43 /* 'C' */
#define AOC_DEVICE_STATUS 0x00  /* no accessory inserted, keeps pak/rumble code away */

#define AOC_CAP_CONFIDENCE 0x01
#define AOC_CAP_TRANSPOSE 0x02

#define AOC_STATE_CHECK_XOR 0xA5

/* Logical note ids */
#define AOC_NOTE_NONE 0
#define AOC_NOTE_A 1
#define AOC_NOTE_C_DOWN 2
#define AOC_NOTE_C_RIGHT 3
#define AOC_NOTE_C_LEFT 4
#define AOC_NOTE_C_UP 5
#define AOC_NOTE_COUNT 5

/* GET_STATE flags */
#define AOC_FLAG_LOW_SIGNAL 0x01
#define AOC_FLAG_CLIPPING 0x02
#define AOC_FLAG_OUT_OF_TUNE 0x04

#endif
