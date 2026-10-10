# mupen64plus-input-ocarina {#mainpage}

Proxy input plugin for Mupen64Plus. It loads a regular input plugin (mupen64plus-input-sdl by default) for
controller ports 1-3 and emulates the **Audio Ocarina device** (the same one as the ORU hardware) on port 4 as a raw-data Joybus device. The current
note comes from the PC microphone (YIN pitch detection) or from debug keys.

## Pipeline

```
microphone ──► aoc_mic.c (miniaudio capture, audio thread)
                 └─► aoc_pitch.c (YIN + quantization to the 5 ocarina notes, tracking)
                       └─► aoc_device.c (Port 4 device model: IDENTIFY / GET_STATE / SET_TRANSPOSE)
                             └─► plugin.c (Mupen64Plus input plugin API, proxy to the real input plugin)
```

## Modules

| File | Role |
|------|------|
| plugin.c | Mupen64Plus plugin entry points, proxying ports 1-3, raw Joybus handling for port 4, config, debug keys |
| aoc_device.h / aoc_device.c | Port 4 device model answering the console's Joybus commands |
| aoc_protocol.h | Wire protocol v1 constants shared with the ROM |
| aoc_pitch.h / aoc_pitch.c | Pitch detector and note quantizer |
| aoc_mic.h / aoc_mic.c | Microphone capture feeding the tracker |
| aoc_mic_probe.c | Command-line tool to test the microphone + detector + the ROM's stability filter |
| tests/ | Unit tests of the device, pitch detector and filter |

The wire protocol is described in the *Audio Ocarina: Port 4 Joybus protocol* page.
