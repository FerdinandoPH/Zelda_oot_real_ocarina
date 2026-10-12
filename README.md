# Zelda OoT Real Ocarina

**English** · [Español](README.es.md)

Play the songs of *The Legend of Zelda: Ocarina of Time* on a real ocarina. Take out the ocarina in the game,
press D-Pad Up, and play: the game hears your instrument instead of the C buttons.

It works on an emulator, using your PC microphone, and on a real N64, with the **ORU** (Ocarina Recognition Unit),
a small device plugged into controller port 4. The parts:

- **ROM hack** for the N64 NTSC-U 1.0 version, built from the [zeldaret/oot](https://github.com/zeldaret/oot)
  decompilation. It is distributed as a patch (`.bps`), so you need your own ROM.
- **Mupen64Plus plugin** for Linux and Windows. It emulates the ORU with your microphone.
- **ORU firmware**: the ORU is a Raspberry Pi Pico, Pico W, Pico 2 or Pico 2 W with an INMP441 microphone.
  It builds, but it **has not been tested on hardware yet**.

No ROM or game asset is included in this repository or its releases.

- [Playing](#playing)
  - [1. Patch your ROM](#1-patch-your-rom)
  - [2a. On an emulator](#2a-on-an-emulator-mupen64plus)
  - [2b. On a real N64](#2b-on-a-real-n64-oru)
  - [In the game](#in-the-game)
  - [Tuning the microphone](#tuning-the-microphone)
- [Development](#development)
- [License](#license)

## Playing

Download the files from the [latest release](../../releases/latest).

### 1. Patch your ROM

You need a legally obtained ROM copy of *The Legend of Zelda: Ocarina of Time* **(USA) version 1.0** in `.z64` format (big-endian):

|       |                                              |
| ----- | -------------------------------------------- |
| SHA-1 | `ad69c91157f6705e8ab06c79fe08aad47bb57ba7` |
| MD5   | `5bd1fe107bf8106b2ab6650abecd54d6`         |

Other versions (1.1, 1.2, PAL, GameCube) do not work as of now. A `.n64` or `.v64` file has the right content but in a
different byte order: convert it to `.z64` first (for example with Tool64).

Apply `zelda-oot-real-ocarina-<version>.bps`:

- in the browser, with [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/): choose your ROM and the
  patch, then *Apply patch*;
- or with [Floating IPS](https://github.com/Alcaro/Flips): `flips --apply zelda-oot-real-ocarina-<version>.bps oot-us-1.0.z64 zelda-oot-real-ocarina.z64`.

The result is a normal 32 MB ROM for emulators and flash carts.

### 2a. On an emulator (Mupen64Plus)

The plugin works with Mupen64Plus 2.5 or newer, and with frontends that use its standard plugins (such as
mupen64plus-qt). It wraps the usual `mupen64plus-input-sdl` plugin, so controllers 1-3 keep working as before. The
plugin adds the audio device on port 4.

**Linux (x86_64).** Install Mupen64Plus first, for example `sudo apt install mupen64plus-ui-console mupen64plus-input-sdl mupen64plus-video-glide64mk2 mupen64plus-audio-sdl mupen64plus-rsp-hle`.

Installing means copying `mupen64plus-input-ocarina.so` into the Mupen64Plus plugin directory, next to
`mupen64plus-input-sdl.so`. That directory belongs to the system (hence `sudo`) and depends on the distribution:
`/usr/lib/x86_64-linux-gnu/mupen64plus/` on Debian/Ubuntu, `/usr/lib/mupen64plus/` or `/usr/lib64/mupen64plus/` on
others, `/usr/local/lib/mupen64plus/` if you built Mupen64Plus yourself. `install.sh` finds it and copies the file
for you:

```sh
tar xzf zelda-oot-real-ocarina-mupen64plus-linux-x86_64.tar.gz
cd zelda-oot-real-ocarina-mupen64plus-linux-x86_64
./install.sh       # or by hand, e.g.: sudo cp mupen64plus-input-ocarina.so /usr/lib/x86_64-linux-gnu/mupen64plus/
mupen64plus --input mupen64plus-input-ocarina zelda-oot-real-ocarina.z64
```

You can also use it without installing, by giving its full path (graphical frontends only list the plugins in
their plugin directory, though):

```sh
mupen64plus --input /path/to/mupen64plus-input-ocarina.so zelda-oot-real-ocarina.z64
```

**Windows (x64).** Use the Windows build of Mupen64Plus (`mupen64plus-bundle-win64` from the
[Mupen64Plus releases](https://github.com/mupen64plus/mupen64plus-core/releases)). Unzip
`zelda-oot-real-ocarina-mupen64plus-windows-x64.zip` and copy `mupen64plus-input-ocarina.dll` into the Mupen64Plus
folder, next to `mupen64plus-input-sdl.dll`. That is the whole installation. Then, from that folder:

```bat
mupen64plus-ui-console.exe --input mupen64plus-input-ocarina.dll zelda-oot-real-ocarina.z64
```

In a graphical frontend, choose `mupen64plus-input-ocarina` as the input plugin.

Without a microphone you can still try it with the keyboard. Hold a key to hold a note and release it for silence:

| Key | Action                            |
| --- | --------------------------------- |
| R   | A note (D)                        |
| T   | C-Down (F)                        |
| N   | C-Right (A)                       |
| U   | C-Left (B)                        |
| O   | C-Up (high D)                     |
| H   | Unplug / replug the port 4 device |
| V   | Microphone on / off               |

For example, Zelda's Lullaby is U O N U O N, with a short gap between the notes.

### 2b. On a real N64 (ORU)

![The ORU on a breadboard: a Raspberry Pi Pico, an INMP441 microphone and a cut N64 controller cable soldered to jumper wires](docs/img/oru.jpg)

1. Build the ORU: a Raspberry Pi Pico, Pico W, Pico 2 or Pico 2 W, an INMP441 microphone module, an optional
   Schottky diode and an N64 controller plug (you'll need to clip it and solder the wires to the Pico). The wiring is in [firmware/pico/README.md](firmware/pico/README.md#wiring).
2. Unzip `oru-firmware.zip`. Hold **BOOTSEL**, plug the Pico into USB, and copy the `.uf2` for your
   board (`pico`, `pico_w`, `pico2` or `pico2_w`) onto the drive that appears.
3. Plug the ORU into **controller port 4** and run the patched ROM from a flash cart.

The firmware has a USB serial console for tuning and diagnostics, and a first-test checklist. Both are described in
[firmware/pico/README.md](firmware/pico/README.md).

### In the game

1. Take out the ocarina. The prompt **"D-Pad Up: Audio"** appears.
2. Press **D-Pad Up**. The game looks for the device on port 4.
   - If it finds it, the game shows **"Listening...  B: Cancel"**. The controller's note buttons are ignored.
   - If not, it shows **"Audio device not found."** and normal play continues.
3. Play. Each stable note appears on the staff:
   - a held note counts once;
   - a note, silence and then the same note counts twice;
   - the staff keeps the last 8 notes.
4. When you play a song the game can use at that point, the normal song sequence takes over.
5. While listening, **D-Pad Up/Down** shifts the expected notes by one octave (-2..+2), shown as **"D-Pad:
   Octave +1"**. Use it for soprano or bass ocarinas, or for whistling. The setting lasts until the console is
   switched off.
6. Press **B** to leave audio mode and go back to the controller.

The target notes are D5, F5, A5, B5 and D6, the notes the game itself plays.

Where audio mode works:

| Situation                                                                           | Audio mode                                                                       |
| ----------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| Free play from a C button (warps, Saria, Storms, Sun, …)                           | Yes                                                                              |
| "Play a song here" prompts (Triforce symbols, Time blocks, Malon, frogs' songs, …) | Yes                                                                              |
| Repeating a song you are being taught                                               | Yes. It stays on across failed attempts                                          |
| Skull Kid memory game (Lost Woods)                                                  | Yes, on your turn. What the device hears during Skull Kid's turn is ignored      |
| Frogs' note game (after playing them all 6 songs)                                   | Yes. B cancels listening instead of quitting the game                            |
| Scarecrow song recording                                                            | No: it records note lengths and R/Z semitones, which audio mode does not produce |

### Tuning the microphone

The plugin settings are in the `[Input-Ocarina]` section of `mupen64plus.cfg` (`~/.config/mupen64plus/` on Linux,
`%APPDATA%\Mupen64Plus\` on Windows). The section is created the first time the plugin runs.

| Setting                                             | Default               | Meaning                                                                                                                |
| --------------------------------------------------- | --------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `Enabled`                                         | true                  | Emulate the device on port 4                                                                                           |
| `Microphone`                                      | true                  | Use the microphone (the debug keys always work)                                                                        |
| `CaptureDevice`                                   | ""                    | Part of the name of the input device to use (`aoc-mic-probe --list` lists them); empty = default                     |
| `Transpose`                                       | 0                     | Semitones added to the target notes, e.g.`-12` for an ocarina one octave lower                                       |
| `ToleranceCents`                                  | 60                    | How far from a target note a pitch may be                                                                              |
| `MinRms`                                          | 0.01                  | Input level below which there is no note                                                                               |
| `YinThreshold`                                    | 0.20                  | Pitch strictness (lower = stricter)                                                                                    |
| `MinFrequency` / `MaxFrequency`                 | 0 / 0                 | Extra pitch search range in Hz. The range always covers the target notes; these can only widen it                      |
| `StickyNotes`                                     | false                 | *Experimental, for whistling/humming only.* Keep a held note through vibrato and glides. Makes ocarina playing worse |
| `HoldMs`                                          | 0                     | *Experimental, for whistling/humming only.* Bridge pitch dropouts this long (ms). Merges repeated ocarina notes      |
| `BackendPlugin`                                   | mupen64plus-input-sdl | Input plugin for controllers 1-3 (file name or path)                                                                   |
| `DebugLog`                                        | false                 | Log the detected notes and the port 4 traffic                                                                          |
| `KeyNote*` / `KeyDisconnect` / `KeyMicToggle` |                       | Debug keys, as SDL2 scancodes                                                                                          |

`aoc-mic-probe`, included with the plugin, shows what the game would hear without starting the emulator. Run it
from a terminal and play: the lines starting with `>>` are the notes the game would register.

```sh
aoc-mic-probe --list                            # capture devices
aoc-mic-probe --device "USB" --transpose -12    # same meaning as the settings above
aoc-mic-probe --octave 1                        # same as the in-game "Octave +1"
aoc-mic-probe --raw --log ocarina.csv           # also print every raw detector change / save a timeline
```

## Development

### Repository layout

In the code, the ROM feature and its port 4 protocol are called *Audio Ocarina* (`AUDIO_OCARINA=1`, `aoc_*`).

| Path                                    | Contents                                                                                                                                           |
| --------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rom/`                                | The ROM hack, as changes to zeldaret/oot:`UPSTREAM_COMMIT`, `audio-ocarina.patch` (modified vanilla files) and `files/` (new files)          |
| `tools/`                              | `setup-oot.sh` (creates `oot/` from `rom/`), `export-oot.sh` (writes `oot/` back to `rom/`), `make-release.sh` (builds the `.bps`) |
| `emulator/mupen64plus-input-ocarina/` | Mupen64Plus proxy input plugin and`aoc-mic-probe`                                                                                                |
| `firmware/pico/`                      | ORU firmware (Raspberry Pi Pico)                                                                                                                   |
| `protocol/protocol.md`                | The port 4 Joybus protocol that the ROM, the plugin and the firmware share                                                                         |
| `docs/architecture.md`                | Design overview                                                                                                                                    |
| `docs/doxygen/`                       | Doxygen setup for the plugin and the ROM hack code (`build_docs.sh`)                                                                             |
| `tests/`                              | Host unit tests: ROM note filter, device model, pitch detector, firmware DSP                                                                       |

### The decomp checkout

The ROM hack lives in a checkout of zeldaret/oot in `oot/`. That directory is ignored by this repository; only
the changes in `rom/` are committed.

```sh
tools/setup-oot.sh      # clones zeldaret/oot at rom/UPSTREAM_COMMIT into oot/ and applies rom/
```

The checkout is shallow; run `git fetch --unshallow` inside `oot/` if you want the upstream history. Follow
the zeldaret/oot README for its build dependencies.

Work in `oot/` as usual. When you are done, write the changes back to `rom/` and commit them:

```sh
tools/export-oot.sh     # rom/audio-ocarina.patch + rom/files/ from oot/ (committed or not)
git add rom && git commit
```

`export-oot.sh` covers modified and added files. It warns if `oot/` deletes or renames vanilla files.

### Building the ROM

```sh
cp /path/to/oot-us-1.0.z64 oot/baseroms/ntsc-1.0/baserom.z64
cd oot
make setup VERSION=ntsc-1.0                      # once
make VERSION=ntsc-1.0 AUDIO_OCARINA=1 -j$(nproc)
# -> build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0.z64
#    add "compress" for the 32 MB ROM: build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0-compressed.z64
```

`AUDIO_OCARINA=1` builds US English text (`REGION=US`) into its own build directory, and turns off the
checksum comparison. Without it, the build is still byte-identical to retail: `make VERSION=ntsc-1.0 REGION=US`
prints `OK`. Do not use `NON_MATCHING=1`: it breaks the IDO build.

**Test ROM.** `make VERSION=ntsc-1.0 AUDIO_OCARINA_TEST=1` builds `build/ntsc-1.0-audio-ocarina-test/` with
hooks that a debugger can write to: warp to an entrance, move Link, open any ocarina action directly, and stand in
for the frog and Skull Kid actors. They are in `Message_UpdateAudioOcarinaTestHooks` (`src/code/z_message.c`);
their addresses are in the build's `.map` file.

### Building the plugin

The plugin needs only a C compiler. miniaudio and the Mupen64Plus API headers are included in `third_party/`. The
`aoc-mic-probe` tool also compiles the ROM's note filter, so run `tools/setup-oot.sh` first (or pass
`OOT=/path/to/oot`).

```sh
cd emulator/mupen64plus-input-ocarina
make                                    # build/mupen64plus-input-ocarina.so, build/aoc-mic-probe
make CC=x86_64-w64-mingw32-gcc          # Windows: build-win64/mupen64plus-input-ocarina.dll, aoc-mic-probe.exe
make package                            # the release archive (.tar.gz, or .zip for Windows)
```

The Windows build works with MinGW-w64 GCC or [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (used by CI).
It links statically, so it needs no extra DLLs.

To test without installing:

```sh
mupen64plus --input "$PWD/build/mupen64plus-input-ocarina.so" ../../oot/build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0.z64
```

The plugin looks for `BackendPlugin` (environment override: `AOC_BACKEND_PLUGIN`) in its own directory, then
in the library search path, then in the usual system plugin directories. `AOC_MICROPHONE=0/1` and
`AOC_DEBUG_LOG=0/1` override the settings file, which is useful for automated runs.

### Building the ORU firmware

You need the [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x (CI uses 2.3.0) in `PICO_SDK_PATH`,
`arm-none-eabi-gcc`, `cmake`, and `ninja` or `make`.

```sh
firmware/pico/build.sh          # all boards -> firmware/pico/dist/oru-<board>.uf2
firmware/pico/build.sh pico2    # just one
```

See [firmware/pico/README.md](firmware/pico/README.md) for the design, the serial console and the Joybus timing
constants.

### Tests and documentation

```sh
make -C tests test              # needs oot/ (or OOT=/path/to/oot)
docs/doxygen/build_docs.sh      # docs/doxygen/out/{plugin,n64}/html/index.html, needs doxygen
```

### Releases

The [Build workflow](.github/workflows/build.yml) runs the tests and builds the plugin (Linux and Windows) and the
firmware on every push. It also checks that `rom/` still applies to the pinned zeldaret/oot commit. Building the
ROM needs the baserom, which cannot be uploaded, so the `.bps` is made locally:

1. Commit everything, including `tools/export-oot.sh`'s output, and push.
2. `git tag v1.0.0 && git push origin v1.0.0`. CI creates the release with the plugin and the firmware.
3. `tools/make-release.sh v1.0.0`. This checks that `oot/` matches `rom/` and that the tag is the checked-out
   commit. It builds the compressed ROM, creates `dist/zelda-oot-real-ocarina-v1.0.0.bps` with Floating IPS (built
   automatically if `flips` is not installed), checks that the patch reproduces the ROM, and uploads it to the
   release with `gh`.

## License

The code in this repository is licensed under the GNU General Public License, version 2 or (at your option) any
later version. See [LICENSE](LICENSE).

Third-party code:

- `emulator/mupen64plus-input-ocarina/third_party/m64p/`: Mupen64Plus API headers, GPL-2.0-or-later.
- `emulator/mupen64plus-input-ocarina/third_party/miniaudio/`: [miniaudio](https://miniaud.io), public domain or
  MIT-0.
- `rom/` is a set of changes to the [zeldaret/oot](https://github.com/zeldaret/oot) decompilation. The
  decompilation itself is not included: `tools/setup-oot.sh` downloads it from its own repository.

*The Legend of Zelda: Ocarina of Time* is © Nintendo. This project is not affiliated with Nintendo and does not
distribute any part of the game.
