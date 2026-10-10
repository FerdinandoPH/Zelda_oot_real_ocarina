/**
 * @file led.h
 *
 * Pico firmware: on-board LED. GPIO 25 on Pico / Pico 2, the wireless chip's LED pin on Pico W / Pico 2 W.
 * Core 0 only (the wireless driver is not shared with core 1).
 */
#ifndef AOC_FW_LED_H
#define AOC_FW_LED_H

#include <stdbool.h>

typedef enum {
    LED_BOOT,    /**< short blinks while starting */
    LED_OK,      /**< steady on: running, microphone delivers audio */
    LED_MIC_FAULT /**< fast blink: no audio from the microphone */
} LedMode;

/** Initializes the LED (and the wireless chip on W boards). @return false if it failed */
bool led_init(void);

void led_set_mode(LedMode mode);

/** Drives the blink patterns; call often from the core 0 loop. Does nothing after led_shutdown(). */
void led_update(void);

/** Switches the LED off for good; on W boards also powers down the wireless chip. */
void led_shutdown(void);

#endif
