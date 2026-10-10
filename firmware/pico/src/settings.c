/**
 * @file settings.c
 *
 * Pico firmware: settings in the last flash sector, with a magic, a layout version and a CRC.
 */
#include "settings.h"

#include "aoc_pitch.h"
#include "config.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"

#include <stddef.h>
#include <string.h>

#define SETTINGS_MAGIC 0x5345434Fu /* "OCES" */
#define SETTINGS_VERSION 1
#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t version;
    Settings settings;
    uint32_t crc;
} StoredSettings;

static uint32_t crc32(const void* data, size_t len) {
    const uint8_t* p = data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int k;

    for (i = 0; i < len; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void settings_defaults(Settings* s) {
    AocPitchConfig c;

    aoc_pitch_default_config(&c, MIC_SAMPLE_RATE);
    s->transpose = c.transpose;
    s->toleranceCents = c.toleranceCents;
    s->minRms = c.minRms;
    s->yinThreshold = c.yinThreshold;
    s->gainBits = DEFAULT_GAIN_BITS;
}

bool settings_load(Settings* s) {
    const StoredSettings* stored = (const StoredSettings*)(XIP_BASE + SETTINGS_OFFSET);

    if (stored->magic != SETTINGS_MAGIC || stored->version != SETTINGS_VERSION ||
        stored->crc != crc32(stored, offsetof(StoredSettings, crc))) {
        settings_defaults(s);
        return false;
    }
    *s = stored->settings;
    return true;
}

/** Runs with the other core locked out and interrupts disabled (flash_safe_execute) */
static void write_sector(void* param) {
    flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SETTINGS_OFFSET, param, FLASH_PAGE_SIZE);
}

bool settings_save(const Settings* s) {
    static uint8_t page[FLASH_PAGE_SIZE];
    StoredSettings stored;

    memset(&stored, 0, sizeof(stored));
    stored.magic = SETTINGS_MAGIC;
    stored.version = SETTINGS_VERSION;
    stored.settings = *s;
    stored.crc = crc32(&stored, offsetof(StoredSettings, crc));

    memset(page, 0xFF, sizeof(page));
    memcpy(page, &stored, sizeof(stored));
    return flash_safe_execute(write_sector, page, 100) == PICO_OK;
}
