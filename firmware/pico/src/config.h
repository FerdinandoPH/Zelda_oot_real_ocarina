/**
 * @file config.h
 *
 * Pico firmware for the Audio Ocarina Port 4 device: pins and tunables.
 * Protocol: protocol/protocol.md. Wiring: firmware/pico/README.md.
 */
#ifndef AOC_FW_CONFIG_H
#define AOC_FW_CONFIG_H

/* N64 controller port 4, DATA line (open drain, the console provides the pull-up) */
#define JOYBUS_PIN 22

/* INMP441 I2S microphone (L/R tied to GND: the mic sends the left slot) */
#define MIC_PIN_SCK 18 /* BCLK */
#define MIC_PIN_WS 19  /* LRCLK, must be MIC_PIN_SCK + 1 (PIO side-set) */
#define MIC_PIN_SD 20

/*
 * Nominal sample rate. The INMP441 accepts 7.8..50 kHz with BCLK = 64 * fs. The detector uses the real rate
 * produced by the PIO clock divider (i2s_mic_sample_rate()).
 */
#define MIC_SAMPLE_RATE 32000

/* Samples between two pitch analyses: 128 at 32 kHz = 4 ms (the ROM polls every 16.7 ms) */
#define DSP_HOP 128

/* Longest analysis window in raw samples (window * decimation); see aoc_dsp_decimation() */
#define DSP_HISTORY 2048

/* Mic fault: no new samples, or a constant signal, for this long */
#define MIC_FAULT_MS 500

/* After the boot blinks, the LED shows the mic state for this long and is then switched off for good. On W boards
 * this also powers down the wireless chip, which only drives the LED and draws current from the console's port. */
#define LED_SHOW_MS 5000

/* Default gain in powers of two applied to the 24-bit samples (0..8). 2 suits an ocarina at 10-30 cm */
#define DEFAULT_GAIN_BITS 2

/*
 * Joybus PIO clock. The bit timings live in joybus.pio as cycle counts at this rate (16 cycles per microsecond):
 * sample point, end-of-frame idle and the line-idle wait before a reply. Adjust them there if the first test on a
 * console shows timing problems.
 */
#define JOYBUS_PIO_HZ 16000000

#endif
