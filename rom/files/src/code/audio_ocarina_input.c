/**
 * @file audio_ocarina_input.c
 *
 * Audio Ocarina: lets the player play ocarina songs with a real instrument. A device on controller Port 4 (a Pico
 * based peripheral on hardware, an input plugin on emulators) performs pitch detection and reports the current
 * logical note. This file owns everything the ROM does with it:
 *
 *  - SI side (padmgr thread): custom Joybus commands to Port 4, only while audio mode is probing or listening.
 *    Each GET_STATE sample goes through the stability filter (audio_ocarina_filter.c) and accepted notes are queued.
 *  - Game side (graph thread): the audio mode state machine driven from the ocarina message mode, the injector that
 *    replays accepted notes into the vanilla ocarina input path as short button presses (so vanilla song
 *    recognition is reused unchanged), and the bounded visual note history drawn on the staff.
 *
 * Threading: the SI side only writes the probe result, the device capabilities, the disconnect flag, the queue head
 * and clears the transpose request; the game side only writes the request, the keepalive, the octave, sets the
 * transpose request and writes the queue tail. The game side resets shared state before publishing a request.
 *
 * N64 background for reading this file:
 *  - Controllers are not memory mapped. The CPU talks to them through the PIF chip: it writes a 64-byte command block
 *    ("PIF RAM") with one entry per controller port, the PIF runs the commands over each port's serial line (the
 *    "Joybus" protocol: send N bytes, receive M bytes) and the CPU reads the block back with the answers. The copies
 *    between RDRAM and PIF RAM are DMAs done by the Serial Interface (SI).
 *  - Every PIF RAM channel entry is: [tx size][rx size][tx bytes...][rx bytes...]. The top bits of the rx size byte
 *    come back set on errors (e.g. 0x80 = nothing answered on that port). A 0x00 entry skips a port, 0xFE ends the
 *    list and the last byte of the block (0x01) tells the PIF to run it.
 *  - OoT reads the controllers on the padmgr thread, once per vertical retrace (the TV refresh, 60 Hz on NTSC). This
 *    file adds its own Port 4 exchange right after it, so the device is sampled at the same rate.
 *  - The game logic and the ocarina audio code run on the graph thread, once per game frame. Both threads touch the
 *    shared variables below, which is why they are `volatile` and each one has a single writer.
 */
#include "audio_ocarina_input.h"
#include "alignment.h"
#include "controller.h"
#include "ocarina.h"

typedef enum AudioOcarinaSiRequest {
    /* 0 */ AOC_SI_REQUEST_NONE,
    /* 1 */ AOC_SI_REQUEST_PROBE,
    /* 2 */ AOC_SI_REQUEST_LISTEN
} AudioOcarinaSiRequest;

typedef enum AudioOcarinaProbeResult {
    /* 0 */ AOC_PROBE_PENDING,
    /* 1 */ AOC_PROBE_FOUND,
    /* 2 */ AOC_PROBE_NOT_FOUND
} AudioOcarinaProbeResult;

// Shared between threads
static volatile u8 sSiRequest = AOC_SI_REQUEST_NONE;
static volatile u8 sProbeResult = AOC_PROBE_PENDING;
static volatile u8 sDisconnected = false;
static volatile s16 sKeepAlive = 0;
static volatile u8 sEventQueue[AOC_EVENT_QUEUE_SIZE];
static volatile u8 sEventHead = 0; // written by the SI side
static volatile u8 sEventTail = 0; // written by the game side
static volatile u8 sDeviceCaps = 0;
// Octave shift sent to the device. Lives for the whole session (power-on default 0), not saved.
static volatile s8 sOctave = 0;
static volatile u8 sTransposePending = false;

// SI side only
static AudioOcarinaFilter sFilter;
static s16 sFailedSamples = 0;
// PIF RAM image. IDO ignores the alignment attribute of OSPifRam, so align by hand to a data cache line.
static u64 sPifRamBuf[(sizeof(OSPifRam) + 0x10) / sizeof(u64)];

// Game side only
static u8 sMode = AOC_MODE_DISABLED;
static s16 sProbeTimer = 0;
static s16 sErrorTimer = 0;
static u16 sInjectButtons = 0;
static s16 sInjectTimer = 0;
// Whether the game currently accepts ocarina notes from the player (false during Skull Kid's turn, song
// demonstrations, frog reactions...). Notes detected meanwhile are the game's own sounds and are discarded.
static u8 sInputOpen = false;
static AudioOcarinaDisplay sDisplay;

static u16 sNoteToButton[] = {
    0,          // AOC_NOTE_NONE
    BTN_A,      // AOC_NOTE_A
    BTN_CDOWN,  // AOC_NOTE_C_DOWN
    BTN_CRIGHT, // AOC_NOTE_C_RIGHT
    BTN_CLEFT,  // AOC_NOTE_C_LEFT
    BTN_CUP,    // AOC_NOTE_C_UP
};

/**
 * Runs one raw Joybus exchange with the device on Port 4: sends `txSize` command bytes and reads back `rxSize` bytes.
 *
 * This bypasses libultra's controller functions (they only know standard commands) and drives the PIF by hand:
 *  1. Takes exclusive use of the SI (__osSiGetAccess), since other code may also use it.
 *  2. Builds a PIF RAM block in `sPifRamBuf`: three "skip" bytes for ports 1-3, then the Port 4 entry
 *     [txSize][rxSize][tx bytes][rx bytes pre-filled with 0xFF], then the end marker and the "execute" flag.
 *  3. DMAs the block to the PIF (OS_WRITE), which runs the command, and waits for the SI interrupt message on
 *     `serialEventQueue`; then DMAs the block back (OS_READ) and waits again.
 *  4. Checks the error bits the PIF set in the rx size byte and copies the answer to `rx`.
 *
 * It also sets __osContLastCmd to an invalid value. libultra caches the last controller command it built in the
 * shared PIF RAM image and skips rebuilding it when the same command is repeated; since this exchange replaced the
 * PIF's contents, the next osContStartReadData/osContStartQuery must rebuild it from scratch.
 *
 * The buffer is aligned by hand to 16 bytes because DMA works on whole data cache lines.
 *
 * @param serialEventQueue queue that receives the SI "DMA done" messages (owned by padmgr, already acquired)
 * @param tx     command bytes; tx[0] is the command id (AOC_CMD_*)
 * @param txSize number of command bytes
 * @param rx     output buffer for the answer (`rxSize` bytes; left with 0xFF where the device sent nothing)
 * @param rxSize number of answer bytes expected
 * @return true if the device answered without Joybus errors (no "no response" or size mismatch bit)
 */
static s32 AudioOcarinaInput_Transfer(OSMesgQueue* serialEventQueue, u8* tx, s32 txSize, u8* rx, s32 rxSize) {
    u8* pifRam = (u8*)ALIGN16((uintptr_t)sPifRamBuf);
    u8* ptr = pifRam;
    u8* rxSizePtr;
    u8* rxData;
    s32 i;
    s32 ok;

    __osSiGetAccess();

    for (i = 0; i < (s32)sizeof(OSPifRam); i++) {
        pifRam[i] = 0;
    }
    for (i = 0; i < AOC_CHANNEL; i++) {
        *ptr++ = CONT_CMD_SKIP_CHNL;
    }
    *ptr++ = txSize;
    rxSizePtr = ptr;
    *ptr++ = rxSize;
    for (i = 0; i < txSize; i++) {
        *ptr++ = tx[i];
    }
    rxData = ptr;
    for (i = 0; i < rxSize; i++) {
        *ptr++ = 0xFF;
    }
    *ptr = CONT_CMD_END;
    pifRam[sizeof(OSPifRam) - 1] = CONT_CMD_EXE;

    // The shared controller PIF RAM image must be rebuilt by the next osContStartReadData/osContStartQuery
    __osContLastCmd = CONT_CMD_END;

    __osSiRawStartDma(OS_WRITE, pifRam);
    osRecvMesg(serialEventQueue, NULL, OS_MESG_BLOCK);
    __osSiRawStartDma(OS_READ, pifRam);
    osRecvMesg(serialEventQueue, NULL, OS_MESG_BLOCK);

    ok = !(*rxSizePtr & CHNL_ERR_MASK);
    for (i = 0; i < rxSize; i++) {
        rx[i] = rxData[i];
    }

    __osSiRelAccess();
    return ok;
}

/**
 * Asks the device on Port 4 what it is (IDENTIFY command) to find out whether an Audio Ocarina device is plugged in.
 *
 * A valid answer is the magic "OCAR", the protocol version and a capability byte. A normal controller, a missing
 * device or an unknown version all fail. On success the capabilities are stored in sDeviceCaps (e.g. whether the
 * device supports octave selection with SET_TRANSPOSE).
 *
 * @param serialEventQueue SI message queue, see AudioOcarinaInput_Transfer()
 * @return true if a compatible device answered
 */
static s32 AudioOcarinaInput_Identify(OSMesgQueue* serialEventQueue) {
    u8 tx[1];
    u8 rx[AOC_IDENTIFY_RX_SIZE];

    tx[0] = AOC_CMD_IDENTIFY;
    if (!AudioOcarinaInput_Transfer(serialEventQueue, tx, 1, rx, AOC_IDENTIFY_RX_SIZE)) {
        return false;
    }
    if ((rx[0] != AOC_IDENTIFY_MAGIC_0) || (rx[1] != AOC_IDENTIFY_MAGIC_1) || (rx[2] != AOC_IDENTIFY_MAGIC_2) ||
        (rx[3] != AOC_IDENTIFY_MAGIC_3) || (rx[4] != AOC_PROTOCOL_VERSION)) {
        return false;
    }
    sDeviceCaps = rx[5];
    return true;
}

/**
 * Records one failed exchange (no answer, bad checksum, invalid data) with the device.
 *
 * A single failure is ignored: the filter keeps its state instead of treating it as silence, so one corrupted sample
 * does not cut a held note. After AOC_DISCONNECT_SAMPLES failures in a row the device is considered unplugged:
 * sDisconnected tells the game thread to leave audio mode and the SI side stops polling.
 */
static void AudioOcarinaInput_CountFailure(void) {
    // A failed exchange is not silence: hold the filter state and count towards a disconnect
    sFailedSamples++;
    if (sFailedSamples >= AOC_DISCONNECT_SAMPLES) {
        sDisconnected = true;
        sSiRequest = AOC_SI_REQUEST_NONE;
    }
}

/**
 * Sends the octave chosen by the player to the device (SET_TRANSPOSE command) as a semitone offset (octave * 12).
 *
 * The device answers with the value it applied; if that does not match, it counts as a failed exchange and the
 * request stays pending, so it is retried on the next retrace. The pending flag is only cleared if the player did not
 * change the octave again while this was being sent. The filter is reset because whatever it was timing was detected
 * with the old octave.
 *
 * @param serialEventQueue SI message queue, see AudioOcarinaInput_Transfer()
 */
static void AudioOcarinaInput_SendTranspose(OSMesgQueue* serialEventQueue) {
    u8 tx[2];
    u8 rx[AOC_SET_TRANSPOSE_RX_SIZE];
    s8 semitones = sOctave * 12;

    tx[0] = AOC_CMD_SET_TRANSPOSE;
    tx[1] = (u8)semitones;
    if (!AudioOcarinaInput_Transfer(serialEventQueue, tx, 2, rx, AOC_SET_TRANSPOSE_RX_SIZE) ||
        (rx[0] != (u8)semitones)) {
        AudioOcarinaInput_CountFailure();
        return;
    }
    // Only clear the request if the octave did not change again meanwhile
    if (semitones == sOctave * 12) {
        sTransposePending = false;
    }
    // Whatever was being detected belongs to the old octave
    AudioOcarinaFilter_Reset(&sFilter);
}

/**
 * Reads the note the device hears right now (GET_STATE command) and feeds it to the stability filter.
 *
 * The answer is [note][confidence][flags][check], where check = note ^ confidence ^ flags ^ 0xA5 detects corrupted
 * transfers. A valid sample resets the failure counter; if the filter reports a newly accepted note, it is appended
 * to the event queue for the game thread (dropped if the queue is full). This is a single-producer / single-consumer
 * ring buffer: only this function moves the head, only the game thread moves the tail, so no lock is needed.
 *
 * @param serialEventQueue SI message queue, see AudioOcarinaInput_Transfer()
 */
static void AudioOcarinaInput_PollState(OSMesgQueue* serialEventQueue) {
    u8 tx[1];
    u8 rx[AOC_GET_STATE_RX_SIZE];
    u8 note;
    u8 nextHead;

    tx[0] = AOC_CMD_GET_STATE;
    if (!AudioOcarinaInput_Transfer(serialEventQueue, tx, 1, rx, AOC_GET_STATE_RX_SIZE) ||
        (rx[3] != (rx[0] ^ rx[1] ^ rx[2] ^ AOC_STATE_CHECK_XOR)) || (rx[0] > AOC_NOTE_MAX)) {
        AudioOcarinaInput_CountFailure();
        return;
    }

    sFailedSamples = 0;
    note = AudioOcarinaFilter_Step(&sFilter, rx[0], rx[1]);
    if (note != AOC_NOTE_NONE) {
        nextHead = (sEventHead + 1) & (AOC_EVENT_QUEUE_SIZE - 1);
        if (nextHead != sEventTail) {
            sEventQueue[sEventHead] = note;
            sEventHead = nextHead;
        }
    }
}

/**
 * SI side entry point. Called by padmgr (PadMgr_HandleRetrace) every retrace, after the standard controller traffic,
 * while it holds the serial event queue.
 *
 * Executes what the game thread asked for through sSiRequest:
 *  - AOC_SI_REQUEST_PROBE: one IDENTIFY; the result goes to sProbeResult and the request is cleared.
 *  - AOC_SI_REQUEST_LISTEN: one exchange per retrace. A pending octave change is sent first (SET_TRANSPOSE),
 *    otherwise the note state is polled (GET_STATE). Every retrace consumes one unit of sKeepAlive, which the game
 *    thread refills every frame; if the game stops refilling it (for example the play state was destroyed without
 *    going through the normal exit), polling stops by itself instead of running forever.
 *  - AOC_SI_REQUEST_NONE: nothing, so the vanilla game never sees any Port 4 traffic.
 *
 * @param serialEventQueue queue that receives the SI interrupt messages, already acquired by padmgr
 */
void AudioOcarinaInput_SiUpdate(OSMesgQueue* serialEventQueue) {
    switch (sSiRequest) {
        case AOC_SI_REQUEST_PROBE:
            sProbeResult = AudioOcarinaInput_Identify(serialEventQueue) ? AOC_PROBE_FOUND : AOC_PROBE_NOT_FOUND;
            sSiRequest = AOC_SI_REQUEST_NONE;
            break;

        case AOC_SI_REQUEST_LISTEN:
            if (sKeepAlive <= 0) {
                // The game thread stopped updating audio mode (e.g. the play state ended), stop polling
                sSiRequest = AOC_SI_REQUEST_NONE;
                break;
            }
            sKeepAlive--;
            if (sTransposePending && (sDeviceCaps & AOC_CAP_TRANSPOSE)) {
                AudioOcarinaInput_SendTranspose(serialEventQueue);
                break;
            }
            AudioOcarinaInput_PollState(serialEventQueue);
            break;

        default:
            break;
    }
}

/**
 * Empties the note history shown on the free play staff.
 */
static void AudioOcarinaInput_ClearDisplay(void) {
    s32 i;

    sDisplay.count = 0;
    for (i = 0; i < AOC_MAX_VISIBLE_NOTES; i++) {
        sDisplay.buttonIndex[i] = OCARINA_BTN_INVALID;
        sDisplay.alpha[i] = 0;
    }
}

/**
 * Appends an accepted note to the staff history. When all AOC_MAX_VISIBLE_NOTES slots are in use, the oldest note is
 * dropped and the rest move one slot to the left. The new note starts transparent and fades in while drawn.
 *
 * @param buttonIndex OcarinaButtonIndex of the note (OCARINA_BTN_A..OCARINA_BTN_C_UP)
 */
static void AudioOcarinaInput_PushDisplay(u8 buttonIndex) {
    s32 i;

    if (sDisplay.count >= AOC_MAX_VISIBLE_NOTES) {
        // Scroll: drop the oldest note
        for (i = 0; i < AOC_MAX_VISIBLE_NOTES - 1; i++) {
            sDisplay.buttonIndex[i] = sDisplay.buttonIndex[i + 1];
            sDisplay.alpha[i] = sDisplay.alpha[i + 1];
        }
        sDisplay.count = AOC_MAX_VISIBLE_NOTES - 1;
    }
    sDisplay.buttonIndex[sDisplay.count] = buttonIndex;
    sDisplay.alpha[sDisplay.count] = 0;
    sDisplay.count++;
}

/**
 * Enters AOC_MODE_LISTENING after a successful probe.
 *
 * All the shared state is reset *before* sSiRequest is set to LISTEN, so the SI thread never sees half-initialized
 * data: filter and failure counter cleared, event queue emptied, injector idle, staff cleared, keepalive filled. The
 * octave is marked as pending so the device is synchronized with the octave chosen earlier in this session.
 */
static void AudioOcarinaInput_StartListening(void) {
    AudioOcarinaFilter_Reset(&sFilter);
    sFailedSamples = 0;
    sDisconnected = false;
    sEventTail = sEventHead;
    sInjectButtons = 0;
    sInjectTimer = 0;
    AudioOcarinaInput_ClearDisplay();
    sKeepAlive = AOC_KEEPALIVE_RETRACES;
    // Sync the device with the session's octave every time listening starts
    sTransposePending = true;
    sMode = AOC_MODE_LISTENING;
    sSiRequest = AOC_SI_REQUEST_LISTEN;
}

/**
 * Leaves audio mode (back to AOC_MODE_DISABLED). The SI side stops talking to Port 4 on the next retrace, queued notes
 * are discarded, any injected button press is released and the staff history is cleared. The octave is kept.
 */
static void AudioOcarinaInput_Stop(void) {
    sSiRequest = AOC_SI_REQUEST_NONE;
    sMode = AOC_MODE_DISABLED;
    sEventTail = sEventHead;
    sInjectButtons = 0;
    sInjectTimer = 0;
    AudioOcarinaInput_ClearDisplay();
}

/**
 * Audio mode state machine, driven by the player's buttons. Called by Message_UpdateAudioOcarina() every frame in
 * which the player can play notes (free play, song checks, song playback, memory game and frog game turns).
 *
 *  - DISABLED: D-Pad Up starts a probe (asks the SI side to send IDENTIFY) and moves to PROBING.
 *  - PROBING: waits for the SI side's answer. Found: starts listening and asks for an ocarina restart. Not found, or
 *    no answer within AOC_PROBE_TIMEOUT_FRAMES: back to DISABLED showing "Audio device not found." for a while.
 *  - LISTENING: refills the SI keepalive. B leaves audio mode; a disconnect leaves it and shows the error. If the
 *    device supports it, D-Pad Up/Down change the octave (-2..+2), which is sent to the device on the SI side.
 *
 * Entering or leaving listening returns AOC_ACTION_RESTART_OCARINA: the caller must restart the vanilla ocarina
 * prompt so that notes played before the switch are not mixed into the song being checked.
 *
 * Also counts down the error message timer.
 *
 * @param pressedButtons controller 1 buttons newly pressed this frame (BTN_* bits)
 * @return AOC_ACTION_RESTART_OCARINA if the vanilla ocarina state must be restarted, else AOC_ACTION_NONE
 */
AudioOcarinaAction AudioOcarinaInput_UpdatePlaying(u16 pressedButtons) {
    if (sErrorTimer > 0) {
        sErrorTimer--;
    }

    switch (sMode) {
        case AOC_MODE_DISABLED:
            if (CHECK_BTN_ALL(pressedButtons, BTN_DUP)) {
                sErrorTimer = 0;
                sProbeTimer = 0;
                sProbeResult = AOC_PROBE_PENDING;
                sMode = AOC_MODE_PROBING;
                sSiRequest = AOC_SI_REQUEST_PROBE;
            }
            break;

        case AOC_MODE_PROBING:
            sProbeTimer++;
            if (sProbeResult == AOC_PROBE_FOUND) {
                AudioOcarinaInput_StartListening();
                return AOC_ACTION_RESTART_OCARINA;
            }
            if ((sProbeResult == AOC_PROBE_NOT_FOUND) || (sProbeTimer > AOC_PROBE_TIMEOUT_FRAMES)) {
                AudioOcarinaInput_Stop();
                sErrorTimer = AOC_ERROR_MESSAGE_FRAMES;
            }
            break;

        case AOC_MODE_LISTENING:
            sKeepAlive = AOC_KEEPALIVE_RETRACES;
            if (sDisconnected) {
                AudioOcarinaInput_Stop();
                sErrorTimer = AOC_ERROR_MESSAGE_FRAMES;
                return AOC_ACTION_RESTART_OCARINA;
            }
            if (CHECK_BTN_ALL(pressedButtons, BTN_B)) {
                AudioOcarinaInput_Stop();
                return AOC_ACTION_RESTART_OCARINA;
            }
            if (AudioOcarinaInput_HasOctaveControl()) {
                if (CHECK_BTN_ALL(pressedButtons, BTN_DUP) && (sOctave < AOC_OCTAVE_MAX)) {
                    sOctave++;
                    sTransposePending = true;
                } else if (CHECK_BTN_ALL(pressedButtons, BTN_DDOWN) && (sOctave > AOC_OCTAVE_MIN)) {
                    sOctave--;
                    sTransposePending = true;
                }
            }
            break;

        default:
            break;
    }

    return AOC_ACTION_NONE;
}

/**
 * Keeps audio mode tied to the ocarina UI. Called every frame from Message_UpdateAudioOcarina().
 *
 * Audio mode only survives the message modes in which the ocarina is open (including short transitional ones, e.g.
 * the fail animation before a retry). In anything else (song recognized, ocarina closed, a normal textbox, no message
 * at all) audio mode is stopped and control goes back to vanilla. While it survives, the SI keepalive is refilled.
 *
 * @param allowed true if the message system is in one of the modes where audio mode may stay active
 */
void AudioOcarinaInput_CheckMessageState(s32 allowed) {
    if (!allowed) {
        sErrorTimer = 0;
        if (sMode != AOC_MODE_DISABLED) {
            AudioOcarinaInput_Stop();
        }
    } else if (sMode == AOC_MODE_LISTENING) {
        sKeepAlive = AOC_KEEPALIVE_RETRACES;
    }
}

/**
 * Advances the note injector. Called by AudioOcarina_Update() (audio code, once per frame) right before it reads the
 * controller, so AudioOcarina_ReadControllerInput() sees the injected buttons.
 *
 * The vanilla ocarina code turns button presses into notes and recognizes songs from them. Instead of changing that
 * code, each accepted note is replayed as a fake press of its button: pressed for AOC_INJECT_PRESS_FRAMES, then
 * released for AOC_INJECT_RELEASE_FRAMES so that the next note is seen as a new press even if it is the same one.
 * Vanilla ignores input for one frame after a pitch change, hence at least 2 frames each.
 *
 * When the previous press/release cycle is over, the next note is taken from the event queue, its button becomes
 * the injected button and the note is added to the staff history. Outside listening, or while the game does not
 * accept notes (see AudioOcarinaInput_SetInputOpen()), the injector stays idle.
 */
void AudioOcarinaInput_TickInjector(void) {
    u8 note;

    if (sMode != AOC_MODE_LISTENING) {
        sInjectButtons = 0;
        sInjectTimer = 0;
        return;
    }

    if (!sInputOpen) {
        sEventTail = sEventHead;
        sInjectButtons = 0;
        sInjectTimer = 0;
        return;
    }

    if (sInjectTimer > 0) {
        sInjectTimer--;
        if (sInjectTimer > 0) {
            return;
        }
        if (sInjectButtons != 0) {
            // End of the press, release so the next note (even the same one) registers as new
            sInjectButtons = 0;
            sInjectTimer = AOC_INJECT_RELEASE_FRAMES;
            return;
        }
    }

    if (sEventTail != sEventHead) {
        note = sEventQueue[sEventTail];
        sEventTail = (sEventTail + 1) & (AOC_EVENT_QUEUE_SIZE - 1);
        sInjectButtons = sNoteToButton[note];
        sInjectTimer = AOC_INJECT_PRESS_FRAMES;
        AudioOcarinaInput_PushDisplay(note - 1);
    }
}

/**
 * Tells the injector whether the game currently accepts notes from the player. Called every frame from
 * Message_UpdateAudioOcarina().
 *
 * While it does not (Skull Kid's or the frogs' turn, a song demonstration...), what the microphone hears is the
 * game's own ocarina, not the player, so it is discarded here: the queue is emptied and any injected press is
 * released. This cannot be left to AudioOcarinaInput_TickInjector(), because the audio code only calls it while
 * vanilla ocarina input is enabled.
 *
 * @param open true if the player can play notes this frame
 */
void AudioOcarinaInput_SetInputOpen(s32 open) {
    sInputOpen = open;
    if (!open) {
        sEventTail = sEventHead;
        sInjectButtons = 0;
        sInjectTimer = 0;
    }
}

/**
 * @return the buttons (BTN_* bits) the injector is pressing this frame, 0 if none. While listening, the audio code
 *         uses this instead of the real controller buttons for the ocarina.
 */
u16 AudioOcarinaInput_GetInjectedButtons(void) {
    return sInjectButtons;
}

/**
 * @return true while audio mode is active (notes come from the device, the controller's note buttons are ignored and
 *         the game does not play its own ocarina sound)
 */
s32 AudioOcarinaInput_IsListening(void) {
    return sMode == AOC_MODE_LISTENING;
}

/**
 * @return the current audio mode state, used by the UI to choose which text to draw
 */
AudioOcarinaMode AudioOcarinaInput_GetMode(void) {
    return sMode;
}

/**
 * @return frames left for the "Audio device not found." message, 0 if it must not be shown
 */
s16 AudioOcarinaInput_GetErrorTimer(void) {
    return sErrorTimer;
}

/**
 * @return the note history for the free play staff. The caller may update the alpha values (fade-in) while drawing.
 */
AudioOcarinaDisplay* AudioOcarinaInput_GetDisplay(void) {
    return &sDisplay;
}

/**
 * @return true if the device reported the AOC_CAP_TRANSPOSE capability, i.e. the octave can be changed with the D-Pad
 */
s32 AudioOcarinaInput_HasOctaveControl(void) {
    return (sDeviceCaps & AOC_CAP_TRANSPOSE) != 0;
}

/**
 * @return the octave shift chosen by the player (AOC_OCTAVE_MIN..AOC_OCTAVE_MAX), kept until the console is reset
 */
s8 AudioOcarinaInput_GetOctave(void) {
    return sOctave;
}
