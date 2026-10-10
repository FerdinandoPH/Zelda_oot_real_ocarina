#!/usr/bin/env bash
# Builds the firmware for every supported board into dist/oru-<board>.uf2
# Needs PICO_SDK_PATH (Pico SDK >= 2.0), arm-none-eabi-gcc, cmake and ninja (or make).
set -euo pipefail
cd "$(dirname "$0")"

: "${PICO_SDK_PATH:?set PICO_SDK_PATH to your pico-sdk checkout}"
boards=("${@:-pico pico_w pico2 pico2_w}")
generator=()
command -v ninja >/dev/null && generator=(-G Ninja)

mkdir -p dist
for board in ${boards[@]}; do
    echo "=== $board ==="
    cmake -S . -B "build-$board" "${generator[@]}" -DPICO_BOARD="$board" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "build-$board" -j"$(nproc)"
    cp "build-$board/audio_ocarina.uf2" "dist/oru-$board.uf2"
done
ls -l dist
