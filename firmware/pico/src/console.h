/**
 * @file console.h
 *
 * Pico firmware: USB serial console (status, live log, settings). Type "help" in a serial terminal.
 */
#ifndef AOC_FW_CONSOLE_H
#define AOC_FW_CONSOLE_H

#include "settings.h"

/** @param settings the live settings; the console edits them and applies them to the detector */
void console_init(Settings* settings);

/** Reads pending input and prints the live log. Never blocks; call often from the core 0 loop. */
void console_poll(void);

#endif
