# Host unit tests: ROM note filter, Port 4 device model, pitch detector (+ Pico firmware DSP core)
CC ?= gcc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra
OOT ?= ../oot
PLUGIN := ../emulator/mupen64plus-input-ocarina
FIRMWARE := ../firmware/pico
BUILD := build

TESTS := $(BUILD)/test_filter $(BUILD)/test_device $(BUILD)/test_pitch

.PHONY: all test clean
all: $(TESTS)

test: $(TESTS)
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; exit $$fail

$(BUILD):
	mkdir -p $@

$(BUILD)/test_filter: test_filter.c $(OOT)/src/code/audio_ocarina_filter.c | $(BUILD)
	$(CC) $(CFLAGS) -D_LANGUAGE_C -I$(OOT)/include -o $@ $^

$(BUILD)/test_device: test_device.c $(PLUGIN)/src/aoc_device.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(PLUGIN)/src -o $@ $^

$(BUILD)/test_pitch: test_pitch.c $(PLUGIN)/src/aoc_pitch.c $(OOT)/src/code/audio_ocarina_filter.c \
		$(FIRMWARE)/src/aoc_dsp_core.c | $(BUILD)
	$(CC) $(CFLAGS) -D_LANGUAGE_C -I$(PLUGIN)/src -I$(OOT)/include -I$(FIRMWARE)/src -o $@ $^ -lm

clean:
	rm -rf $(BUILD)
