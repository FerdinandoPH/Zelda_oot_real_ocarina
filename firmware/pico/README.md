# ORU (Ocarina Recognition Unit): Pico firmware

**English** · [Español](README.es.md)

The **ORU** (Ocarina Recognition Unit) is the physical Port 4 device. It runs on a Raspberry Pi **Pico, Pico W,
Pico 2 or Pico 2 W**. An INMP441 I2S microphone listens to the ocarina. The ORU detects which of the five Ocarina
of Time notes is playing and reports it to the N64 on controller port 4 using `protocol/protocol.md` v1.

> **Status:** builds for all four boards; the shared parts are covered by the host tests. It has **not been
> run on hardware yet**. See "First test" below.

## Wiring

| From               | To (Pico)                                                       | Pico pin         |
| ------------------ | --------------------------------------------------------------- | ---------------- |
| INMP441 VDD        | 3V3(OUT)                                                        | 36               |
| INMP441 GND        | GND                                                             | 38 (or any GND)  |
| INMP441 L/R        | GND                                                             | 38 (or any GND)  |
| INMP441 SCK / BCLK | GP18                                                            | 24               |
| INMP441 WS / LRCLK | GP19                                                            | 25               |
| INMP441 SD         | GP20                                                            | 26               |
| N64 port 4 DATA    | GP22                                                            | 29               |
| N64 port 4 3.3 V   | VSYS, **through a Schottky diode** (anode on the N64 side) | 39               |
| N64 port 4 GND     | GND                                                             | 38               |

The N64 controller plug has three pins: GND, DATA (the middle one) and 3.3 V. Before you connect anything,
identify GND and 3.3 V on your own cable or socket with a multimeter, with the console switched on.

Notes:

- **DATA is open drain.** The console has its own pull-up to 3.3 V. The firmware only ever pulls the line low or
  releases it. Never connect DATA to a 5 V source.
- **The diode on VSYS** lets you plug in USB, for the serial console, while the Pico sits on the console. The
  Pico already has a diode from VBUS to VSYS, so the two supplies cannot fight. A BAT54 or 1N5817 works. The
  Pico's regulator runs fine from the ~3.0 V that is left after the diode.
- Current draw is about 25 mA for a Pico or Pico 2 with the mic. On Pico W boards the wireless chip only drives
  the LED, and it is powered down together with the LED a few seconds after boot (see "LED"). This is well within
  what a port gives a Rumble Pak.
- Keep the mic wires short: SCK runs at 2 MHz.

## Flash

1. Download `oru-firmware.zip` from the [latest release](../../../../releases/latest) and unzip it. Use the `.uf2`
   that matches your board: `oru-pico.uf2`, `oru-pico_w.uf2`, `oru-pico2.uf2` or `oru-pico2_w.uf2`. A Pico W
   build does not run on a plain Pico, and a Pico 2 build does not run on a Pico either.
2. Hold **BOOTSEL** and plug the Pico into USB. A drive named `RPI-RP2` or `RP2350` appears.
3. Copy the `.uf2` onto it. The Pico reboots into the firmware.

Once the firmware is on the board, `bootsel` on the serial console does step 2 for you.

## LED

| LED                 | Meaning                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------- |
| Short blinks, 0.5 s | Starting                                                                                                  |
| **Steady on**       | Running, and the microphone delivers audio                                                                |
| Fast blink (5 Hz)   | No audio from the microphone: nothing arrives, or the signal is constant (check SD, SCK, WS, L/R and VDD) |

The LED only shows this for 5 seconds after the boot blinks (`LED_SHOW_MS` in `src/config.h`), then it stays
off. On Pico W / Pico 2 W the wireless chip, which drives the LED, is powered down at the same time, so it draws
nothing from the console. Use `status` on the serial console to check the mic later.

The LED says nothing about the console. Use `status` on the serial console for that; see below.

## Serial console

USB CDC, any baud rate, for example `screen /dev/ttyACM0` or `picocom /dev/ttyACM0`. On Windows, use PuTTY on
the COM port. Type `help`.

| Command                   | Effect                                                                                                                                                                                                                                                                                                                                                      |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `status`                | Current note, frequency, cents, level and flags; transposition; settings; Joybus counters                                                                                                                                                                                                                                                                   |
| `log on` / `log off`  | Print every note when it ends: the silence before it (`gap`), its duration, `<- game` if it is long enough for the ROM to accept it (`<- game?`: borderline), and, when it repeats the previous note, whether the game sees one note (`joined`, gap under ~33 ms) or two (`repeat`, 50 ms or more). Also the console's IDENTIFY and SET_TRANSPOSE |
| `set transpose <n>`     | Calibration in semitones, added to the console's octave. Use `-12` for an ocarina one octave lower                                                                                                                                                                                                                                                        |
| `set tolerance <cents>` | Maximum distance from a note (default 60)                                                                                                                                                                                                                                                                                                                   |
| `set minrms <x>`        | Input gate (default 0.01)                                                                                                                                                                                                                                                                                                                                   |
| `set yin <x>`           | Pitch strictness (default 0.20)                                                                                                                                                                                                                                                                                                                             |
| `set gain <0..8>`       | Microphone gain in powers of two (default 2)                                                                                                                                                                                                                                                                                                                |
| `save`                  | Store the settings in flash. This pauses the Joybus for a few ms, so do not do it while the game is listening                                                                                                                                                                                                                                               |
| `defaults`              | Back to the defaults (`save` to keep them)                                                                                                                                                                                                                                                                                                                |
| `bootsel`               | Reboot into the UF2 bootloader                                                                                                                                                                                                                                                                                                                              |

The settings mean the same as the plugin's `[Input-Ocarina]` settings, so values tuned with `aoc-mic-probe` on the
PC carry over. The gain is the exception: `aoc-mic-probe` has no gain setting.

## First test (checklist)

1. **Mic only, USB power, no N64.** The LED should be steady on (for the first 5 seconds). `status` should show `mic: ok`. Enter `log on`
   and play each note: A, C-Down, C-Right, C-Left, C-Up should give D5, F5, A5, B5, D6 at 0 cents ± some. If the
   LED blinks fast, check the mic wiring.
   - If the notes are always one octave off, use `set transpose ±12`.
   - If `rms` is below 0.01 while you play, raise `set gain`.
   - If `flags` shows `clip`, lower it.
2. **On the N64, port 4, console power only.** The LED should be steady on for 5 seconds, then go off. Boot the hack ROM, take out the
   ocarina and press D-Pad Up.
   - The game should show **"Listening..."**. With USB also connected and `log on`, the log shows
     `console: IDENTIFY` and `SET_TRANSPOSE +0`.
   - Play Zelda's Lullaby.
3. **If the game says "Audio device not found."**, check `status`:
   - `joybus: 0 replies`: nothing is received. Check DATA/GND and the connector pinout.
   - Replies count up, but IDENTIFY never succeeds: this is a timing problem. The constants are at the top of
     `src/joybus.pio`:

     - `SAMPLE_DELAY`: sample point, about 2 µs after the falling edge;
     - `FRAME_IDLE_LOOPS`: end of frame;
     - `REPLY_IDLE_LOOPS`: how long the line must be idle before the reply starts, about 3.5 µs. Real
       controllers answer 2–4 µs after the stop bit.

     A logic analyser on DATA settles this quickly.

## Development

Only needed if you want to change the firmware: the release already contains the `.uf2` files.

### Build

You need the Pico SDK 2.x (`PICO_SDK_PATH`), `arm-none-eabi-gcc`, `cmake` and `ninja` or `make`.

```sh
cd firmware/pico
./build.sh                # all boards -> dist/oru-{pico,pico_w,pico2,pico2_w}.uf2
./build.sh pico2          # just one
```

### How it works

The Joybus replies come from the same device model as the emulator plugin
(`emulator/mupen64plus-input-ocarina/src/aoc_device.c`). The pitch detector is shared too
(`aoc_pitch.c`, integer path `aoc_pitch_track_s16`).

- **Core 1** only answers the console. A PIO state machine (`src/joybus.pio`) receives the command bytes. Core 1
  calls `aoc_device_process()` and hands the reply to the same state machine, which waits until the console's
  stop bit is over and then sends the reply. This code runs from RAM. It never waits for the detector: it reads
  the last published note, a single 32-bit word.
- **Core 0** reads the microphone and runs the detector:
  - A second PIO program (`src/i2s_mic.pio`) is the I2S master at 32 kHz. DMA copies the samples into a ring.
  - Every 4 ms, the latest samples go through a DC blocker and the gain, and are converted to 16 bits.
  - When the octave goes down, the samples are also decimated ×2/×4/×8, so the detector costs the same at every
    octave.
  - `aoc_pitch_track_s16()` runs on the result. Its inner loop is integer-only, because the RP2040 has no FPU.
  - Core 0 also drives the LED and the USB console.
- **SET_TRANSPOSE** from the console (D-Pad Up/Down while listening) is added to the calibration `transpose`.
