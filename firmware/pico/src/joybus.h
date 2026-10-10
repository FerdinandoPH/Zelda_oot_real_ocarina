/**
 * @file joybus.h
 *
 * Pico firmware: Joybus responder for controller port 4. Runs alone on core 1 and answers the console with
 * aoc_device_process() (the same device model as the emulator plugin). The pitch detector on core 0 only
 * publishes its latest result; answering never waits for it.
 */
#ifndef AOC_FW_JOYBUS_H
#define AOC_FW_JOYBUS_H

#include <stdint.h>

/** Traffic counters, for the console (written by core 1 only) */
typedef struct {
    volatile uint32_t frames;      /**< commands that got a reply */
    volatile uint32_t ignored;     /**< frames without a reply (unknown commands, e.g. pak access) */
    volatile uint32_t identify;    /**< IDENTIFY 0x4F */
    volatile uint32_t getState;    /**< GET_STATE 0x4E */
    volatile uint32_t setTranspose; /**< SET_TRANSPOSE 0x4D */
    volatile uint32_t lastCommand; /**< last command byte */
} JoybusStats;

extern JoybusStats gJoybusStats;

/** Claims a PIO state machine and starts listening on `pin`. Call on core 0 before joybus_run() starts. */
void joybus_init(unsigned pin);

/** Publishes the current detector state; read by core 1 at every GET_STATE. Any core. */
void joybus_set_note(uint8_t note, uint8_t confidence, uint8_t flags);

/** @return the transpose last requested by the console with SET_TRANSPOSE (0 after RESET). Any core. */
int joybus_rom_transpose(void);

/** Core 1 entry point: answers the console forever. */
void joybus_run(void);

#endif
