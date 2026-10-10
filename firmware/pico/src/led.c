/**
 * @file led.c
 *
 * Pico firmware: on-board LED. See led.h.
 */
#include "led.h"

#include "pico/stdlib.h"

#if defined(CYW43_WL_GPIO_LED_PIN)
#include "pico/cyw43_arch.h"
#elif !defined(PICO_DEFAULT_LED_PIN)
#error "This board has no known on-board LED"
#endif

static LedMode sMode = LED_BOOT;
static bool sOn;
static bool sReady;

static void write(bool on) {
    if (!sReady || on == sOn) {
        return;
    }
    sOn = on;
#if defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#endif
}

bool led_init(void) {
#if defined(CYW43_WL_GPIO_LED_PIN)
    if (cyw43_arch_init() != 0) {
        return false;
    }
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    sReady = true;
    sOn = true;
    write(false);
    return true;
}

void led_shutdown(void) {
    if (!sReady) {
        return;
    }
    write(false);
    sReady = false;
#if defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_deinit(); /* also drives WL_REG_ON low: the chip is powered off */
#endif
}

void led_set_mode(LedMode mode) {
    sMode = mode;
}

void led_update(void) {
    uint32_t ms = to_ms_since_boot(get_absolute_time());

    switch (sMode) {
        case LED_BOOT:
            write((ms % 200) < 50);
            break;
        case LED_OK:
            write(true);
            break;
        case LED_MIC_FAULT:
            write((ms % 200) < 100);
            break;
    }
}
