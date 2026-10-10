CC ?= gcc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -fvisibility=hidden -Isrc -Ithird_party/m64p -Ithird_party/miniaudio

# Windows build: make CC=x86_64-w64-mingw32-gcc (any MinGW-w64 compiler). Everything is linked statically, so the
# .dll and .exe need no MinGW runtime DLLs next to them.
ifneq ($(filter %-mingw32 %-windows-gnu,$(shell $(CC) -dumpmachine)),)
  WINDOWS := 1
  BUILD ?= build-win64
  LIB_EXT := .dll
  EXE_EXT := .exe
  CFLAGS += -DWIN32
  LDFLAGS += -static
  LDLIBS = -lpthread -lm
else
  BUILD ?= build
  LIB_EXT := .so
  EXE_EXT :=
  CFLAGS += -fPIC
  LDLIBS = -ldl -lpthread -lm
endif

PLUGIN := $(BUILD)/mupen64plus-input-ocarina$(LIB_EXT)
PROBE := $(BUILD)/aoc-mic-probe$(EXE_EXT)

CORE_OBJS := $(BUILD)/aoc_device.o $(BUILD)/aoc_pitch.o
MIC_OBJS := $(BUILD)/aoc_mic.o

.PHONY: all clean test package

all: $(PLUGIN) $(PROBE)

$(BUILD):
	mkdir -p $@

# miniaudio is large and noisy with -Wextra
$(BUILD)/aoc_mic.o: CFLAGS += -Wno-unused-parameter -Wno-unused-function -Wno-unused-result
$(BUILD)/%.o: src/%.c src/*.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLUGIN): $(BUILD)/plugin.o $(CORE_OBJS) $(MIC_OBJS)
	$(CC) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)

# The probe runs the ROM's own stability filter so it shows what the game would register
OOT ?= ../../oot
$(BUILD)/audio_ocarina_filter.o: $(OOT)/src/code/audio_ocarina_filter.c $(OOT)/include/audio_ocarina_filter.h | $(BUILD)
	$(CC) $(CFLAGS) -D_LANGUAGE_C -I$(OOT)/include -c $< -o $@

$(PROBE): tools/aoc_mic_probe.c $(CORE_OBJS) $(MIC_OBJS) $(BUILD)/audio_ocarina_filter.o
	$(CC) $(CFLAGS) $(LDFLAGS) -D_LANGUAGE_C -I$(OOT)/include -o $@ $^ $(LDLIBS)

test:
	$(MAKE) -C ../../tests

# Release archive: build/zelda-oot-real-ocarina-mupen64plus-linux-x86_64.tar.gz or build-win64/...-windows-x64.zip
ifdef WINDOWS
PKG := zelda-oot-real-ocarina-mupen64plus-windows-x64
else
PKG := zelda-oot-real-ocarina-mupen64plus-linux-x86_64
endif
package: $(PLUGIN) $(PROBE)
	rm -rf $(BUILD)/pkg && mkdir -p $(BUILD)/pkg/$(PKG)
	cp $(PLUGIN) $(PROBE) $(BUILD)/pkg/$(PKG)/
ifdef WINDOWS
	sed 's/$$/\r/' package/README-windows.txt > $(BUILD)/pkg/$(PKG)/README.txt
	rm -f $(BUILD)/$(PKG).zip && cd $(BUILD)/pkg && python3 -m zipfile -c ../$(PKG).zip $(PKG)
else
	cp package/README-linux.txt $(BUILD)/pkg/$(PKG)/README.txt
	cp package/install.sh $(BUILD)/pkg/$(PKG)/
	tar -czf $(BUILD)/$(PKG).tar.gz -C $(BUILD)/pkg $(PKG)
endif

clean:
	rm -rf $(BUILD)
