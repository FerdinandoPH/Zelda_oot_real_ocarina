# Zelda OoT Real Ocarina: ROM hack {#mainpage}

Code added to the `zeldaret/oot` decompilation (branch `audio-ocarina`) so the player can play ocarina songs with a
real instrument. A device on controller **Port 4** does the pitch detection; the ROM polls it, filters the notes and
replays them to the vanilla ocarina code as button presses.

Everything is compiled only with `make VERSION=ntsc-1.0 AUDIO_OCARINA=1` (`AUDIO_OCARINA_TEST=1` adds
debugger-driven test hooks). This documentation only covers the Audio Ocarina code, not the vanilla decompilation.

## Contents

| Source | What is documented |
|--------|--------------------|
| `include/audio_ocarina_input.h`, `src/code/audio_ocarina_input.c` | **New**: protocol constants, SI transfer, thread handoff, mode state machine, injector, display history |
| `include/audio_ocarina_filter.h`, `src/code/audio_ocarina_filter.c` | **New**: note stability filter |
| `src/code/z_message_audio_ocarina.c` | **Extract** of the Audio Ocarina UI block added to `src/code/z_message.c` (prompt, status texts, staff) |
| *Hooks in vanilla code* page | **Diffs** of every modified vanilla file (`Makefile`, `spec/spec`, `general.c`, `padmgr.c`, `z_message.c`, `z_en_fr.c`) |

Design documents: *Architecture* and *Port 4 Joybus protocol* pages.
