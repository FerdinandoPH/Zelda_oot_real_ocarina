# Architecture

```
   acoustic ocarina
         |
   microphone (PC)  /  INMP441 I2S mic (ORU, firmware/pico)
         |
   pitch detector: YIN + 5-note quantizer            (emulator/.../src/aoc_pitch.c)
         |  current state: note 0..5, confidence, flags
   Port 4 device model: protocol v1                  (emulator/.../src/aoc_device.c, protocol/protocol.md)
         |  Joybus 0x4F IDENTIFY / 0x4E GET_STATE
 ========|=================== N64 / emulated PIF ===========================================
         |
   padmgr thread, every retrace                      (oot/src/code/padmgr.c -> AudioOcarinaInput_SiUpdate)
     GET_STATE -> stability filter -> accepted note events queue
         |                                           (oot/src/code/audio_ocarina_filter.c)
   graph thread, every frame
     injector: event -> 2-frame button press + 2-frame release
         |                                           (AudioOcarinaInput_TickInjector)
   AudioOcarina_ReadControllerInput: sOcarinaInputButtonCur = injected mask
         |
   vanilla: note decode -> song recognizer -> message state machine -> song success
```

## ROM (`oot/`, branch `audio-ocarina`, built with `AUDIO_OCARINA=1`)

| File | Role |
|------|------|
| `include/audio_ocarina_input.h`, `src/code/audio_ocarina_input.c` | Protocol constants, SI transfer, thread handoff, the mode state machine, the injector and the display history |
| `include/audio_ocarina_filter.h`, `src/code/audio_ocarina_filter.c` | Pure stability filter |
| `src/code/padmgr.c` | Calls `AudioOcarinaInput_SiUpdate` under the serial lock |
| `src/audio/game/general.c` | Input injection, the injector tick and suppression of the live note sound |
| `src/code/z_message.c` | The D-Pad Up / B handling in `MSGMODE_OCARINA_PLAYING`, restarting the recognizer, the mode watchdog in `Message_Update`, and the UI: text, plus the staff in free play |
| `Makefile`, `spec/spec` | `AUDIO_OCARINA=1` defines the macro, adds the objects, builds into `build/<version>-audio-ocarina`, forces `REGION=US` and `COMPARE=0` |

The vanilla build (`AUDIO_OCARINA=0`, the default) is byte-identical to the retail ROM.

### Audio mode states (game thread)

```
DISABLED --D-Pad Up--> PROBING --IDENTIFY ok--> LISTENING   (then restart: MSGMODE_OCARINA_STARTING)
   ^                      |                        |
   |                      +--no reply/timeout------+--> DISABLED + "Audio device not found."
   |                                               |
   |                                               |-- D-Pad Up/Down: octave +-1 (-2..+2, per session) -> SET_TRANSPOSE
   +------------ B (cancel) / disconnect ----------+   (restart ocarina, history cleared)
   +------------ message mode leaves {STARTING, PLAYING, FAIL, NOTES_DROP}: song recognized, ocarina closed
```

## Emulator (`emulator/mupen64plus-input-ocarina/`)

| File | Role |
|------|------|
| `src/plugin.c` | Proxy input plugin. Loads the backend (`mupen64plus-input-sdl` by default) for ports 1-3, marks port 4 as `RawData`, answers its Joybus traffic, handles the config section `Input-Ocarina` and the debug keys |
| `src/aoc_device.c` | The protocol v1 device model |
| `src/aoc_pitch.c` | YIN pitch detector and quantizer |
| `src/aoc_mic.c` | miniaudio capture at 48 kHz mono, analysing every 512 samples |
| `tools/aoc_mic_probe.c` | Standalone live detector for tuning |

## ORU firmware (`firmware/pico/`)

The ORU (Ocarina Recognition Unit) is the hardware Port 4 device: a Raspberry Pi Pico with an I2S microphone.
It reuses `aoc_device.c` and `aoc_pitch.c` from the plugin (the detector's integer path `aoc_pitch_track_s16`, for the
RP2040 which has no FPU). Details and wiring: `firmware/pico/README.md`.

| File | Role |
|------|------|
| `src/joybus.pio`, `src/joybus.c` | Port 4 responder on core 1: PIO receives and transmits, the CPU answers with `aoc_device_process()` |
| `src/i2s_mic.pio`, `src/i2s_mic.c` | INMP441 I2S master at 32 kHz, DMA ring |
| `src/aoc_dsp_core.c`, `src/dsp.c` | DC blocker, gain, octave-dependent decimation, detector every 4 ms on core 0 |
| `src/led.c`, `src/console.c`, `src/settings.c` | Status LED, USB serial console, settings in flash |

## Tests (`tests/`)

Host unit tests cover the ROM filter, the device model, the pitch detector (float and 16-bit paths) and the Pico firmware's sample pipeline (`make -C tests test`).
