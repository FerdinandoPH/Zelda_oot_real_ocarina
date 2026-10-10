# ORU (Ocarina Recognition Unit): Audio Ocarina Port 4 device firmware for Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W.
# Build one board with -DPICO_BOARD=pico|pico_w|pico2|pico2_w, or all of them with build.sh.
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(audio_ocarina_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# Protocol constants, device model and pitch detector are shared with the Mupen64Plus plugin
set(SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/../../emulator/mupen64plus-input-ocarina/src)

add_executable(audio_ocarina
    src/main.c
    src/joybus.c
    src/i2s_mic.c
    src/dsp.c
    src/aoc_dsp_core.c
    src/led.c
    src/settings.c
    src/console.c
    ${SHARED_DIR}/aoc_device.c
    ${SHARED_DIR}/aoc_pitch.c
)

pico_generate_pio_header(audio_ocarina ${CMAKE_CURRENT_LIST_DIR}/src/joybus.pio)
pico_generate_pio_header(audio_ocarina ${CMAKE_CURRENT_LIST_DIR}/src/i2s_mic.pio)

target_include_directories(audio_ocarina PRIVATE src ${SHARED_DIR})
target_compile_options(audio_ocarina PRIVATE -Wall -Wextra -O2)
# The device model answers the console from core 1: keep it in RAM, away from flash (XIP) stalls
target_compile_definitions(audio_ocarina PRIVATE
    "AOC_RAMFUNC=__attribute__((section(\".time_critical.aoc_device\")))"
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
)

target_link_libraries(audio_ocarina
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_pio
    hardware_dma
    hardware_flash
)

if (PICO_CYW43_SUPPORTED)
    # Pico W / Pico 2 W: the LED is wired to the wireless chip
    target_link_libraries(audio_ocarina pico_cyw43_arch_none)
endif()

pico_enable_stdio_usb(audio_ocarina 1)
pico_enable_stdio_uart(audio_ocarina 0)

pico_set_program_name(audio_ocarina "ORU (Ocarina Recognition Unit)")
pico_set_program_description(audio_ocarina "N64 port 4 Audio Ocarina device (INMP441 + Joybus)")
pico_add_extra_outputs(audio_ocarina)
