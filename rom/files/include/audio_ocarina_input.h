#ifndef AUDIO_OCARINA_INPUT_H
#define AUDIO_OCARINA_INPUT_H

#include "ultra64.h"
#include "audio_ocarina_filter.h"

/**
 * Audio Ocarina: optional ocarina input from an audio-pitch device on controller Port 4.
 * Wire protocol: protocol/protocol.md (outside the decomp tree).
 */

// Port 4 wire protocol v1
#define AOC_CHANNEL 3 // Port 4
#define AOC_CMD_SET_TRANSPOSE 0x4D
#define AOC_CMD_GET_STATE 0x4E
#define AOC_CMD_IDENTIFY 0x4F
#define AOC_IDENTIFY_RX_SIZE 6
#define AOC_GET_STATE_RX_SIZE 4
#define AOC_SET_TRANSPOSE_RX_SIZE 1
#define AOC_IDENTIFY_MAGIC_0 'O'
#define AOC_IDENTIFY_MAGIC_1 'C'
#define AOC_IDENTIFY_MAGIC_2 'A'
#define AOC_IDENTIFY_MAGIC_3 'R'
#define AOC_PROTOCOL_VERSION 1
#define AOC_STATE_CHECK_XOR 0xA5
// IDENTIFY capability bits
#define AOC_CAP_CONFIDENCE 0x01
#define AOC_CAP_TRANSPOSE 0x02 // device accepts SET_TRANSPOSE

// Octave shift selectable with D-Pad Up/Down while listening, kept for the whole play session
#define AOC_OCTAVE_MIN -2
#define AOC_OCTAVE_MAX 2

// Consecutive failed GET_STATE polls (retraces) before the device is considered disconnected (~167 ms)
#define AOC_DISCONNECT_SAMPLES 10
// Retraces the SI side keeps polling without hearing from the game thread before it stops on its own
#define AOC_KEEPALIVE_RETRACES 30
// Game frames to wait for the IDENTIFY answer before giving up
#define AOC_PROBE_TIMEOUT_FRAMES 10
// Game frames the "Audio device not found." message stays on screen
#define AOC_ERROR_MESSAGE_FRAMES 40
// Each accepted note is injected into the vanilla ocarina code as a button press of this many frames followed by a
// release of this many frames. Vanilla drops ocarina input for one frame after every pitch change, so both must be >= 2.
#define AOC_INJECT_PRESS_FRAMES 2
#define AOC_INJECT_RELEASE_FRAMES 2
// Accepted notes buffered between the SI thread and the game thread (power of 2)
#define AOC_EVENT_QUEUE_SIZE 16
// Notes visible on the staff. Matches the vanilla 8-slot ocarina staff layout and the longest song.
#define AOC_MAX_VISIBLE_NOTES 8

/**
 * State of audio mode, as seen by the game thread.
 */
typedef enum AudioOcarinaMode {
    /* 0 */ AOC_MODE_DISABLED, ///< Vanilla ocarina controls; no Port 4 traffic at all
    /* 1 */ AOC_MODE_PROBING,  ///< D-Pad Up was pressed; waiting a few frames for the IDENTIFY answer
    /* 2 */ AOC_MODE_LISTENING ///< Device found; notes come from the device, polled every retrace
} AudioOcarinaMode;

/**
 * What the message system must do after AudioOcarinaInput_UpdatePlaying().
 */
typedef enum AudioOcarinaAction {
    /* 0 */ AOC_ACTION_NONE,
    /* 1 */ AOC_ACTION_RESTART_OCARINA // clear vanilla ocarina state by going back to MSGMODE_OCARINA_STARTING
} AudioOcarinaAction;

/**
 * Notes accepted while listening, oldest first, for the free play staff drawn by z_message.c.
 * When full, the oldest note scrolls out.
 */
typedef struct AudioOcarinaDisplay {
    /* 0x00 */ u8 count;                               ///< Number of valid entries
    /* 0x01 */ u8 buttonIndex[AOC_MAX_VISIBLE_NOTES]; ///< OcarinaButtonIndex values (which note icon to draw)
    /* 0x0A */ s16 alpha[AOC_MAX_VISIBLE_NOTES];      ///< Per-note opacity, raised every frame for the fade-in
} AudioOcarinaDisplay;

// All functions are documented in src/code/audio_ocarina_input.c

// padmgr thread, called with the serial event queue held
void AudioOcarinaInput_SiUpdate(OSMesgQueue* serialEventQueue);

// Game (graph) thread
AudioOcarinaAction AudioOcarinaInput_UpdatePlaying(u16 pressedButtons);
void AudioOcarinaInput_CheckMessageState(s32 allowed);
void AudioOcarinaInput_SetInputOpen(s32 open);
void AudioOcarinaInput_TickInjector(void);
u16 AudioOcarinaInput_GetInjectedButtons(void);
s32 AudioOcarinaInput_IsListening(void);
AudioOcarinaMode AudioOcarinaInput_GetMode(void);
s16 AudioOcarinaInput_GetErrorTimer(void);
AudioOcarinaDisplay* AudioOcarinaInput_GetDisplay(void);
s32 AudioOcarinaInput_HasOctaveControl(void);
s8 AudioOcarinaInput_GetOctave(void);

#endif
