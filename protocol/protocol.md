# Audio Ocarina: Port 4 Joybus protocol, v1

This document is normative for every implementation of the Audio Ocarina device:

- the **ORU** (Ocarina Recognition Unit), the hardware device: Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W
  firmware in `firmware/pico` (not yet tested on hardware);
- the Mupen64Plus plugin `emulator/mupen64plus-input-ocarina`.

The reference implementations are:

- the device model in `emulator/mupen64plus-input-ocarina/src/aoc_device.c`;
- the ROM side in `oot/src/code/audio_ocarina_input.c`.

You should not need to read either to implement a device.

## 1. Role of the device

The device sits on **controller port 4** (Joybus channel 3). It listens to an instrument, estimates the pitch, and reports the **current** logical note, or silence. It is polled continuously.

The device does **not** do any of the following. The ROM does all of them:

- debounce;
- detect note events;
- keep a note history;
- recognize songs.

## 2. Logical note ids

| id | Meaning | Game button | Sounding pitch in game (default target) |
|----|---------|-------------|------------------------------------------|
| 0 | `NO_NOTE`, silence or no confident pitch | – | – |
| 1 | `NOTE_A` | A | D5 (587.33 Hz) |
| 2 | `NOTE_C_DOWN` | C-Down | F5 (698.46 Hz) |
| 3 | `NOTE_C_RIGHT` | C-Right | A5 (880.00 Hz) |
| 4 | `NOTE_C_LEFT` | C-Left | B5 (987.77 Hz) |
| 5 | `NOTE_C_UP` | C-Up | D6 (1174.66 Hz) |

Id N corresponds to the game's `OcarinaButtonIndex` N-1 (`oot/include/ocarina.h`).

The pitches are the ones the game itself plays for each button. Devices should offer a transpose setting for ocarinas in other keys or octaves. Pitches more than about ±60 cents from every target must be reported as `NO_NOTE`.

## 3. Commands

Every exchange is one command byte from the console (tx = 1 byte), followed by a fixed-length reply. All multi-byte values are listed in wire order.

| Cmd | Name | tx | rx | Reply |
|-----|------|----|----|-------|
| `0x00` | STATUS (standard) | 1 | 3 | `4F 43 00` |
| `0xFF` | RESET (standard) | 1 | 3 | `4F 43 00` (same as STATUS) |
| `0x01` | READ_BUTTONS (standard) | 1 | 4 | `00 00 00 00` |
| `0x4F` | IDENTIFY | 1 | 6 | `'O' 'C' 'A' 'R' <version=0x01> <caps>` |
| `0x4E` | GET_STATE | 1 | 4 | `<note> <confidence> <flags> <check>` |
| `0x4D` | SET_TRANSPOSE | 2 | 1 | `<applied semitones>` (only if caps bit 1) |
| other | – | – | – | **no reply** |

### 3.1 STATUS / RESET (standard commands)

- The type bytes `0x4F 0x43` ("OC") are deliberately **not** `CONT_TYPE_NORMAL` (`0x0005`). The game therefore never treats port 4 as a controller.
- The status byte is `0x00`, meaning "no accessory inserted". This matters: the game's rumble code sends Controller Pak commands (`0x02`/`0x03`) to any port whose status has bit 0 set.
- The device must still ignore `0x02`/`0x03` if they arrive (see "other" in the table above).

### 3.2 READ_BUTTONS (standard command)

The retail game polls all four ports every frame and discards port 4's data. Answer with 4 zero bytes. Staying silent is also tolerated, but answering is preferred.

### 3.3 IDENTIFY `0x4F`

| Byte | Value |
|------|-------|
| 0..3 | ASCII `OCAR` |
| 4 | Protocol version, `0x01` for this document |
| 5 | Capability flags. Bit 0: the confidence byte is meaningful. Bit 1: SET_TRANSPOSE is supported. Other bits are reserved (0). |

The ROM accepts the device only if all of these hold:

- the reply has no Joybus error;
- the magic matches;
- `version == 1`.

A future incompatible protocol must change the version byte. Compatible extensions use the capability bits.

### 3.4 GET_STATE `0x4E`

| Byte | Meaning |
|------|---------|
| 0 `note` | Logical note id 0..5 (§2), the **current** state, reported as long as it lasts |
| 1 `confidence` | 0..255, where 255 is most confident. Must be 0 when `note == 0`. |
| 2 `flags` | Bit 0 low signal, bit 1 clipping, bit 2 out of tune (a pitch was heard but is not one of the five notes). Other bits are reserved (0). Informational only. |
| 3 `check` | `note ^ confidence ^ flags ^ 0xA5` |

The ROM treats a reply as a **failed poll**, not as silence, if any of these hold:

- it has a Joybus error (no response or wrong length);
- its `check` does not match;
- its `note` is greater than 5.

After 10 consecutive failed polls (about 167 ms) the ROM leaves audio mode and shows "Audio device not found.".

### 3.5 SET_TRANSPOSE `0x4D`

The console selects which octave the player is using (D-Pad Up/Down while listening, -2..+2 octaves).

| Byte | Meaning |
|------|---------|
| tx 0 | `0x4D` |
| tx 1 | Signed semitones (`s8`) to add to the target pitches of §2, e.g. `0x0C` = +12, `0xE8` = -24 |
| rx 0 | The value actually applied, after clamping to ±24 |

- The device adds this value to any transpose of its own (a calibration setting). All five targets move together, so the notes keep their meaning in the new octave.
- The value persists until the next SET_TRANSPOSE or a RESET (`0xFF`), which sets it back to 0. STATUS (`0x00`) does not change it.
- The ROM sends it every time audio mode starts and whenever the player changes the octave. It then resets its stability filter.
- If the echo does not match what was sent, the ROM counts the exchange as a failed poll and retries on the next retrace.
- A device without this capability simply does not reply. The ROM never sends it in that case, and hides the octave option.

### 3.6 Unknown commands

Do not reply. This makes the PIF report "no response" (`0x80`), which is exactly what a standard controller does for `0x4E`/`0x4F`. That is how the ROM tells a normal controller apart from this device.

## 4. Traffic the device will see

| When | Traffic on port 4 |
|------|-------------------|
| Boot (`osContInit`) | STATUS/RESET |
| Every vertical retrace (60 Hz NTSC), always (vanilla game behaviour) | READ_BUTTONS, then STATUS |
| Player presses D-Pad Up in the ocarina | One IDENTIFY, on the next retrace |
| While audio mode is listening | One GET_STATE per retrace, after the standard traffic |
| Listening starts, or the octave changes | One SET_TRANSPOSE in place of that retrace's GET_STATE |
| Rumble handling (vanilla) | Possibly STATUS again |

The ROM never sends custom commands (`0x4E`/`0x4F`) outside audio mode. Each custom exchange is a separate PIF transaction: channels 0-2 are skipped, and only channel 3 carries the command.

## 5. Timing requirements

- **Latency budget.** The ROM runs the stability filter itself:
  - a note must be reported unchanged for **5 consecutive polls** (≈83 ms) to be accepted;
  - silence must last **3 polls** (≈50 ms) before the same note counts again.

  Device-side smoothing should add as little latency as possible. Report the raw quantized pitch every analysis hop, ideally ≤ 10 ms.
- **Do not latch.** Report the state at the moment of the poll. Do not queue events and do not edge-detect.
- **Joybus electrical and bit timing** (N64 standard, identical to a controller):
  - The line is open-drain at 3.3 V with a pull-up; drive it low only.
  - The bit period is 4 µs. A `0` bit is 3 µs low then 1 µs high. A `1` bit is 1 µs low then 3 µs high.
  - The console ends its command with a stop bit (1 µs low).
  - The device should start replying about 2-4 µs after the console's stop bit, and ends its reply with a stop bit (2 µs low).
  - The reply length must exactly match the table in §3, otherwise the PIF flags an overrun or underrun.
- **Keep pitch analysis off the Joybus path.** On a Pico, answering must never wait for pitch analysis. Keep the last state in a variable that the PIO/IRQ responder reads, for example with the Joybus responder on one core and the DSP on the other.

## 6. Versioning

This is version 1. A change to any of the following is a new major version and changes byte 4 of IDENTIFY:

- command bytes;
- reply lengths;
- note ids;
- the check formula.
