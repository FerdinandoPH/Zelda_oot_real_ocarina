/**
 * @file i2s_mic.c
 *
 * Pico firmware: INMP441 capture. See i2s_mic.h and i2s_mic.pio.
 */
#include "i2s_mic.h"

#include "i2s_mic.pio.h"

#include "hardware/dma.h"
#include "hardware/pio.h"

/* Stereo words (left, right, left, ...): 2048 words = 1024 frames = 32 ms at 32 kHz */
#define RING_WORDS 2048

static uint32_t sRing[RING_WORDS];
static uint32_t* volatile sRingStart = sRing; /* read by the control DMA channel to rewind the data channel */
static int sDataChannel;
static unsigned sReadIndex; /* next word to read, always even (left slot) */
static float sSampleRate;

void i2s_mic_init(unsigned sdPin, unsigned sckPin, float sampleRate) {
    PIO pio;
    uint sm;
    uint offset;
    int ctrlChannel;
    dma_channel_config dc;
    dma_channel_config cc;
    bool ok = pio_claim_free_sm_and_add_program(&i2s_mic_program, &pio, &sm, &offset);

    hard_assert(ok);
    sSampleRate = i2s_mic_program_init(pio, sm, offset, sdPin, sckPin, sampleRate);

    /*
     * Data channel: PIO RX FIFO -> sRing, one ring per run; it then triggers the control channel, which writes
     * the ring start back into the data channel's write address (trigger alias) and restarts it.
     */
    sDataChannel = dma_claim_unused_channel(true);
    ctrlChannel = dma_claim_unused_channel(true);

    dc = dma_channel_get_default_config(sDataChannel);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&dc, ctrlChannel);
    dma_channel_configure(sDataChannel, &dc, sRing, &pio->rxf[sm], RING_WORDS, false);

    cc = dma_channel_get_default_config(ctrlChannel);
    channel_config_set_transfer_data_size(&cc, DMA_SIZE_32);
    channel_config_set_read_increment(&cc, false);
    channel_config_set_write_increment(&cc, false);
    dma_channel_configure(ctrlChannel, &cc, &dma_hw->ch[sDataChannel].al2_write_addr_trig, &sRingStart, 1,
                          false);

    dma_channel_start(sDataChannel);
    pio_sm_set_enabled(pio, sm, true);
}

float i2s_mic_sample_rate(void) {
    return sSampleRate;
}

int i2s_mic_read(uint32_t* out, int max) {
    uint32_t addr = dma_hw->ch[sDataChannel].write_addr;
    unsigned writeIndex = (unsigned)((addr - (uint32_t)(uintptr_t)sRing) / sizeof(uint32_t));
    unsigned avail;
    int n = 0;

    /* Between the end of a ring and the control channel's rewind the address points one past the end */
    if (writeIndex >= RING_WORDS) {
        writeIndex = 0;
    }
    writeIndex &= ~1u; /* only complete left+right pairs */
    avail = (writeIndex - sReadIndex + RING_WORDS) % RING_WORDS;
    if (avail > RING_WORDS - 64) {
        /* Overrun: the DMA lapped us, restart from just behind the writer */
        sReadIndex = (writeIndex + 64) % RING_WORDS;
        avail = RING_WORDS - 64;
    }
    while (avail >= 2 && n < max) {
        out[n++] = sRing[sReadIndex];
        sReadIndex = (sReadIndex + 2) % RING_WORDS;
        avail -= 2;
    }
    return n;
}
