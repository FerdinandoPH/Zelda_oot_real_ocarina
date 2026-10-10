/**
 * @file main.c
 *
 * Audio Ocarina Port 4 device: Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W firmware.
 *
 * An INMP441 microphone listens to the ocarina; the pitch detector turns it into one of the five Ocarina of Time
 * notes and the device reports it to the N64 on controller port 4 (protocol/protocol.md).
 *
 *  core 1: Joybus responder, nothing else (answers must start a few microseconds after each command)
 *  core 0: microphone, pitch detector, LED, USB console
 */
#include "config.h"
#include "console.h"
#include "dsp.h"
#include "joybus.h"
#include "led.h"
#include "settings.h"

#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

static Settings sSettings;

static void core1_main(void) {
    /* Lets core 0 pause this core while it writes the settings to flash */
    flash_safe_execute_core_init();
    joybus_run();
}

int main(void) {
    bool ledOk;
    absolute_time_t bootEnd;
    absolute_time_t ledEnd;

    /* The Joybus starts first so the device answers as early as possible after power-up */
    settings_load(&sSettings);
    joybus_init(JOYBUS_PIN);
    multicore_launch_core1(core1_main);

    stdio_init_all();
    ledOk = led_init();
    led_set_mode(LED_BOOT);
    dsp_init(&sSettings);
    console_init(&sSettings);
    bootEnd = make_timeout_time_ms(MIC_FAULT_MS); /* blink until the microphone had time to prove itself */
    ledEnd = make_timeout_time_ms(MIC_FAULT_MS + LED_SHOW_MS);

    for (;;) {
        dsp_poll();
        if (time_reached(bootEnd)) {
            led_set_mode(dsp_mic_ok() ? LED_OK : LED_MIC_FAULT);
        }
        if (ledOk) {
            if (time_reached(ledEnd)) {
                led_shutdown();
                ledOk = false;
            } else {
                led_update();
            }
        }
        console_poll();
    }
}
