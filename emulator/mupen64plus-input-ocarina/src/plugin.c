/**
 * @file plugin.c
 *
 * mupen64plus-input-ocarina
 *
 * Proxy input plugin: loads a regular input plugin (mupen64plus-input-sdl by default) for controller ports 1-3
 * and emulates the Audio Ocarina device (protocol/protocol.md, the same one as the ORU hardware) on port 4 as a raw-data Joybus device.
 * The note state comes from the microphone (pitch detection) or from debug keys.
 */
#define _GNU_SOURCE /* dladdr */
#define M64P_PLUGIN_PROTOTYPES 1
#include "m64p_common.h"
#include "m64p_config.h"
#include "m64p_plugin.h"
#include "m64p_types.h"

#include "aoc_device.h"
#include "aoc_mic.h"
#include "aoc_protocol.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define PLUGIN_EXT ".dll"
#else
#include <dlfcn.h>
#define PLUGIN_EXT ".so"
#endif

#define PLUGIN_NAME "Zelda OoT Real Ocarina input proxy"
#define PLUGIN_VERSION 0x010000
#define INPUT_API_VERSION 0x020100
#define CONFIG_API_VERSION 0x020100
#define CONFIG_SECTION "Input-Ocarina"
#define AOC_PORT 3

#define DEFAULT_BACKEND "mupen64plus-input-sdl"

/* SDL2 scancodes */
#define SCANCODE_H 11
#define SCANCODE_O 18
#define SCANCODE_R 21
#define SCANCODE_T 23
#define SCANCODE_U 24
#define SCANCODE_V 25
#define SCANCODE_N 17

/* ---- backend plugin ---- */
static m64p_dynlib_handle sBackendLib = NULL;
static ptr_PluginStartup bStartup;
static ptr_PluginShutdown bShutdown;
static ptr_InitiateControllers bInitiateControllers;
static ptr_GetKeys bGetKeys;
static ptr_ControllerCommand bControllerCommand;
static ptr_ReadController bReadController;
static ptr_RomOpen bRomOpen;
static ptr_RomClosed bRomClosed;
static ptr_SDL_KeyDown bKeyDown;
static ptr_SDL_KeyUp bKeyUp;

/* ---- core config API ---- */
static ptr_ConfigOpenSection ConfigOpenSection;
static ptr_ConfigSetDefaultInt ConfigSetDefaultInt;
static ptr_ConfigSetDefaultFloat ConfigSetDefaultFloat;
static ptr_ConfigSetDefaultBool ConfigSetDefaultBool;
static ptr_ConfigSetDefaultString ConfigSetDefaultString;
static ptr_ConfigGetParamInt ConfigGetParamInt;
static ptr_ConfigGetParamFloat ConfigGetParamFloat;
static ptr_ConfigGetParamBool ConfigGetParamBool;
static ptr_ConfigGetParamString ConfigGetParamString;
static ptr_ConfigSaveSection ConfigSaveSection;

static void (*sDebugCallback)(void*, int, const char*) = NULL;
static void* sDebugContext = NULL;

/* ---- settings ---- */
static int sEnabled = 1;
/*
 * sizeof(CONTROL) in the running core. Newer cores appended `int Type` (3 -> 4 ints) together with
 * VRU support; the vendored 2.5.9 headers only know the first three fields, so Controls[] is indexed
 * by the detected stride. The version number alone is not reliable: distribution snapshots such as
 * Ubuntu's "2.5.9+341" report 0x020509 but already use the 4-int layout (see detect_control_stride).
 */
static size_t sControlStride = 3 * sizeof(int);
static int sCoreVersion = 0;
static int sUseMic = 1;
static int sDebugLog = 0;
static char sCaptureDevice[256] = "";
static AocPitchConfig sPitchConfig;
static int sNoteKeys[AOC_NOTE_COUNT] = { SCANCODE_R, SCANCODE_T, SCANCODE_N, SCANCODE_U, SCANCODE_O };
static int sDisconnectKey = SCANCODE_H;
static int sMicToggleKey = SCANCODE_V;

/* ---- device state ---- */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static int sConnected = 1;
static int sKeyHeld[AOC_NOTE_COUNT];
static uint8_t sKeyNote = AOC_NOTE_NONE; /* most recently pressed held key */
static unsigned long sIdentifyCount = 0;
/* Semitones requested by the ROM with SET_TRANSPOSE (octave selection), added to the configured Transpose */
static int8_t sRomTranspose = 0;
static unsigned long sGetStateCount = 0;

/**
 * printf-style logging through the emulator core's debug callback (shown in the frontend's log), or to stderr if the
 * core gave none.
 * @param level M64MSG_* severity
 */
static void log_msg(int level, const char* fmt, ...) {
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (sDebugCallback != NULL) {
        sDebugCallback(sDebugContext, level, buf);
    } else {
        fprintf(stderr, "[ocarina] %s\n", buf);
    }
}

/* ---- shared libraries (dlopen on Linux, LoadLibrary on Windows) ---- */

static m64p_dynlib_handle lib_open(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* lib_sym(m64p_dynlib_handle lib, const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress(lib, name);
#else
    return dlsym(lib, name);
#endif
}

static void lib_close(m64p_dynlib_handle lib) {
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

/**
 * Writes the directory this plugin was loaded from, with a trailing separator, or "" if it is unknown.
 * Frontends keep every plugin in one directory, so the backend is usually there too.
 */
static void get_plugin_dir(char* dir, size_t size) {
    char* sep;

    dir[0] = '\0';
#ifdef _WIN32
    {
        HMODULE self;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(void*)get_plugin_dir, &self) &&
            GetModuleFileNameA(self, dir, (DWORD)size) < size) {
            sep = strrchr(dir, '\\');
            if (sep == NULL) {
                sep = strrchr(dir, '/');
            }
            if (sep != NULL) {
                sep[1] = '\0';
                return;
            }
        }
    }
#else
    {
        Dl_info info;

        if (dladdr((void*)get_plugin_dir, &info) != 0 && info.dli_fname != NULL) {
            snprintf(dir, size, "%s", info.dli_fname);
            sep = strrchr(dir, '/');
            if (sep != NULL) {
                sep[1] = '\0';
                return;
            }
        }
    }
#endif
    dir[0] = '\0';
}

/**
 * Loads the real input plugin that handles controller ports 1-3 (by default mupen64plus-input-sdl).
 *
 * Tries `name` in the directory this plugin was loaded from, then as given (absolute path or library search path),
 * then the usual system plugin directories, appending ".so" / ".dll" if missing.
 *
 * @param name file name or path of the backend plugin
 * @return library handle, or NULL if it could not be loaded
 */
static m64p_dynlib_handle load_backend(const char* name) {
    char path[1024];
    char ownDir[1024];
    const char* dirs[] = {
        ownDir,
        "", /* as given / library search path */
#ifndef _WIN32
        "/usr/lib/x86_64-linux-gnu/mupen64plus/",
        "/usr/local/lib/mupen64plus/",
        "/usr/lib/mupen64plus/",
        "/usr/lib64/mupen64plus/",
#endif
    };
    const char* ext = strstr(name, PLUGIN_EXT) != NULL ? "" : PLUGIN_EXT;
    size_t i;
    m64p_dynlib_handle lib;

    get_plugin_dir(ownDir, sizeof(ownDir));
    for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (i == 0 && (ownDir[0] == '\0' || strchr(name, '/') != NULL || strchr(name, '\\') != NULL)) {
            continue; /* name is already a path */
        }
        snprintf(path, sizeof(path), "%s%s%s", dirs[i], name, ext);
        lib = lib_open(path);
        if (lib != NULL) {
            log_msg(M64MSG_INFO, "backend input plugin: %s", path);
            return lib;
        }
    }
    log_msg(M64MSG_ERROR, "could not load backend input plugin '%s%s'", name, ext);
    return NULL;
}

/**
 * Looks up the core's configuration functions (Config*), which plugins must fetch by name from the core library.
 * @param core handle of the core library, received in PluginStartup()
 * @return non-zero if all the required functions were found (ConfigSaveSection is optional)
 */
static int bind_config_api(m64p_dynlib_handle core) {
    ConfigOpenSection = (ptr_ConfigOpenSection)lib_sym(core, "ConfigOpenSection");
    ConfigSetDefaultInt = (ptr_ConfigSetDefaultInt)lib_sym(core, "ConfigSetDefaultInt");
    ConfigSetDefaultFloat = (ptr_ConfigSetDefaultFloat)lib_sym(core, "ConfigSetDefaultFloat");
    ConfigSetDefaultBool = (ptr_ConfigSetDefaultBool)lib_sym(core, "ConfigSetDefaultBool");
    ConfigSetDefaultString = (ptr_ConfigSetDefaultString)lib_sym(core, "ConfigSetDefaultString");
    ConfigGetParamInt = (ptr_ConfigGetParamInt)lib_sym(core, "ConfigGetParamInt");
    ConfigGetParamFloat = (ptr_ConfigGetParamFloat)lib_sym(core, "ConfigGetParamFloat");
    ConfigGetParamBool = (ptr_ConfigGetParamBool)lib_sym(core, "ConfigGetParamBool");
    ConfigGetParamString = (ptr_ConfigGetParamString)lib_sym(core, "ConfigGetParamString");
    ConfigSaveSection = (ptr_ConfigSaveSection)lib_sym(core, "ConfigSaveSection");
    return ConfigOpenSection && ConfigSetDefaultInt && ConfigSetDefaultFloat && ConfigSetDefaultBool &&
           ConfigSetDefaultString && ConfigGetParamInt && ConfigGetParamFloat && ConfigGetParamBool &&
           ConfigGetParamString;
}

static const char* sNoteKeyNames[AOC_NOTE_COUNT] = { "KeyNoteA", "KeyNoteCDown", "KeyNoteCRight", "KeyNoteCLeft",
                                                      "KeyNoteCUp" };

/**
 * Reads the [Input-Ocarina] section of mupen64plus.cfg into the settings globals.
 *
 * Every parameter is first registered with its default value and help text (ConfigSetDefault*: only takes effect if
 * the parameter does not exist yet), then read back, and the section is saved so new parameters appear in the file.
 * If the config API is not available, the defaults are kept.
 *
 * @param backendName output: backend plugin name (BackendPlugin)
 * @param backendSize size of backendName
 */
static void load_config(char* backendName, size_t backendSize) {
    m64p_handle section;
    const char* s;
    int i;

    aoc_pitch_default_config(&sPitchConfig, AOC_MIC_SAMPLE_RATE);
    snprintf(backendName, backendSize, "%s", DEFAULT_BACKEND);

    if (ConfigOpenSection == NULL || ConfigOpenSection(CONFIG_SECTION, &section) != M64ERR_SUCCESS) {
        log_msg(M64MSG_WARNING, "config API unavailable, using defaults");
        return;
    }

    ConfigSetDefaultString(section, "BackendPlugin", DEFAULT_BACKEND,
                           "Input plugin used for controller ports 1-3 (file name or path)");
    ConfigSetDefaultBool(section, "Enabled", 1, "Emulate the Audio Ocarina device on controller port 4");
    ConfigSetDefaultBool(section, "Microphone", 1, "Use the microphone as note source (debug keys always work)");
    ConfigSetDefaultString(section, "CaptureDevice", "", "Substring of the capture device name, empty = default");
    ConfigSetDefaultInt(section, "Transpose", 0,
                        "Semitones added to the target notes (D5 F5 A5 B5 D6), e.g. -12 or +12 for other ocarinas");
    ConfigSetDefaultFloat(section, "ToleranceCents", sPitchConfig.toleranceCents,
                          "Max distance in cents from a target note");
    ConfigSetDefaultFloat(section, "MinRms", sPitchConfig.minRms, "Input level below which there is no note (0..1)");
    ConfigSetDefaultFloat(section, "YinThreshold", sPitchConfig.yinThreshold,
                          "Pitch detector threshold (lower = stricter)");
    ConfigSetDefaultFloat(section, "MinFrequency", sPitchConfig.minFreq, "Extra low end of the pitch search range in Hz (0 = automatic)");
    ConfigSetDefaultFloat(section, "MaxFrequency", sPitchConfig.maxFreq, "Extra high end of the pitch search range in Hz (0 = automatic)");
    ConfigSetDefaultBool(section, "StickyNotes", 0,
                         "Experimental, for whistling/humming: keep a held note through vibrato/glides until "
                         "another note is heard. Hurts ocarina playing");
    ConfigSetDefaultFloat(section, "HoldMs", sPitchConfig.holdMs,
                          "Experimental, for whistling/humming: keep a held note through pitch dropouts this long "
                          "while there is still sound (ms). Merges tongued ocarina repeats; 0 = off");
    ConfigSetDefaultBool(section, "DebugLog", 0, "Log detected notes and Port 4 protocol traffic");
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        ConfigSetDefaultInt(section, sNoteKeyNames[i], sNoteKeys[i], "Debug key (SDL2 scancode) held = this note");
    }
    ConfigSetDefaultInt(section, "KeyDisconnect", sDisconnectKey, "Debug key (SDL2 scancode): unplug/replug device");
    ConfigSetDefaultInt(section, "KeyMicToggle", sMicToggleKey, "Debug key (SDL2 scancode): microphone on/off");

    s = ConfigGetParamString(section, "BackendPlugin");
    if (s != NULL && s[0] != '\0') {
        snprintf(backendName, backendSize, "%s", s);
    }
    sEnabled = ConfigGetParamBool(section, "Enabled");
    sUseMic = ConfigGetParamBool(section, "Microphone");
    s = ConfigGetParamString(section, "CaptureDevice");
    snprintf(sCaptureDevice, sizeof(sCaptureDevice), "%s", s != NULL ? s : "");
    sPitchConfig.transpose = ConfigGetParamInt(section, "Transpose");
    sPitchConfig.toleranceCents = ConfigGetParamFloat(section, "ToleranceCents");
    sPitchConfig.minRms = ConfigGetParamFloat(section, "MinRms");
    sPitchConfig.yinThreshold = ConfigGetParamFloat(section, "YinThreshold");
    sPitchConfig.minFreq = ConfigGetParamFloat(section, "MinFrequency");
    sPitchConfig.maxFreq = ConfigGetParamFloat(section, "MaxFrequency");
    sPitchConfig.stickyNotes = ConfigGetParamBool(section, "StickyNotes");
    sPitchConfig.holdMs = ConfigGetParamFloat(section, "HoldMs");
    sDebugLog = ConfigGetParamBool(section, "DebugLog");
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        sNoteKeys[i] = ConfigGetParamInt(section, sNoteKeyNames[i]);
    }
    sDisconnectKey = ConfigGetParamInt(section, "KeyDisconnect");
    sMicToggleKey = ConfigGetParamInt(section, "KeyMicToggle");

    if (ConfigSaveSection != NULL) {
        ConfigSaveSection(CONFIG_SECTION);
    }
}

/**
 * Applies environment variable overrides (AOC_MICROPHONE, AOC_DEBUG_LOG) on top of the config file. Used by
 * automation (e.g. emucap) that runs with a fresh config directory.
 */
static void apply_env_overrides(void) {
    const char* s;

    s = getenv("AOC_MICROPHONE");
    if (s != NULL && s[0] != '\0') {
        sUseMic = atoi(s) != 0;
    }
    s = getenv("AOC_DEBUG_LOG");
    if (s != NULL && s[0] != '\0') {
        sDebugLog = atoi(s) != 0;
    }
}

/**
 * Starts microphone capture with the current settings. The transpose is the configured Transpose plus the octave
 * the ROM requested. On failure only the debug keys produce notes.
 */
static void start_mic(void) {
    AocMicConfig micConfig;

    micConfig.deviceName = sCaptureDevice;
    micConfig.pitch = sPitchConfig;
    micConfig.pitch.transpose = sPitchConfig.transpose + sRomTranspose;
    micConfig.debugLog = sDebugLog;
    if (aoc_mic_start(&micConfig) != 0) {
        log_msg(M64MSG_WARNING, "microphone unavailable, only debug keys will produce notes");
    }
}

/* ---- Mupen64Plus plugin API ---- */

/**
 * Mupen64Plus API: first call after the core loads the plugin.
 *
 * Reads the configuration, loads the backend input plugin, resolves its functions and starts it with the same
 * arguments, so ports 1-3 behave exactly as with the backend alone. Also records the core version, used to detect
 * the layout of the CONTROL array (see detect_control_stride()).
 *
 * @param CoreLibHandle handle of the core library (to look up its API)
 * @param Context       opaque pointer for DebugCallback
 * @param DebugCallback core logging function
 * @return M64ERR_SUCCESS, or an error if the backend could not be loaded or started
 */
EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle CoreLibHandle, void* Context,
                                     void (*DebugCallback)(void*, int, const char*)) {
    char backendName[512];
    const char* env;
    m64p_error err;

    sDebugCallback = DebugCallback;
    sDebugContext = Context;

    bind_config_api(CoreLibHandle);
    {
        ptr_PluginGetVersion coreGetVersion = (ptr_PluginGetVersion)lib_sym(CoreLibHandle, "PluginGetVersion");
        int coreVersion = 0;

        if (coreGetVersion != NULL) {
            coreGetVersion(NULL, &coreVersion, NULL, NULL, NULL);
        }
        sCoreVersion = coreVersion;
        log_msg(M64MSG_INFO, "core version 0x%06X", coreVersion);
    }
    load_config(backendName, sizeof(backendName));
    apply_env_overrides();
    env = getenv("AOC_BACKEND_PLUGIN");
    if (env != NULL && env[0] != '\0') {
        snprintf(backendName, sizeof(backendName), "%s", env);
    }

    sBackendLib = load_backend(backendName);
    if (sBackendLib == NULL) {
        return M64ERR_INPUT_NOT_FOUND;
    }

    bStartup = (ptr_PluginStartup)lib_sym(sBackendLib, "PluginStartup");
    bShutdown = (ptr_PluginShutdown)lib_sym(sBackendLib, "PluginShutdown");
    bInitiateControllers = (ptr_InitiateControllers)lib_sym(sBackendLib, "InitiateControllers");
    bGetKeys = (ptr_GetKeys)lib_sym(sBackendLib, "GetKeys");
    bControllerCommand = (ptr_ControllerCommand)lib_sym(sBackendLib, "ControllerCommand");
    bReadController = (ptr_ReadController)lib_sym(sBackendLib, "ReadController");
    bRomOpen = (ptr_RomOpen)lib_sym(sBackendLib, "RomOpen");
    bRomClosed = (ptr_RomClosed)lib_sym(sBackendLib, "RomClosed");
    bKeyDown = (ptr_SDL_KeyDown)lib_sym(sBackendLib, "SDL_KeyDown");
    bKeyUp = (ptr_SDL_KeyUp)lib_sym(sBackendLib, "SDL_KeyUp");

    if (bStartup == NULL || bInitiateControllers == NULL || bGetKeys == NULL) {
        log_msg(M64MSG_ERROR, "backend input plugin is missing required functions");
        lib_close(sBackendLib);
        sBackendLib = NULL;
        return M64ERR_INPUT_INVALID;
    }

    err = bStartup(CoreLibHandle, Context, DebugCallback);
    if (err != M64ERR_SUCCESS) {
        return err;
    }

    log_msg(M64MSG_INFO, "%s, Port 4 device %s, microphone %s (default debug keys: notes R T N U O, H = unplug, V = mic on/off)",
            PLUGIN_NAME, sEnabled ? "enabled" : "disabled", sUseMic ? "on" : "off");
    return M64ERR_SUCCESS;
}

/**
 * Mupen64Plus API: last call before the plugin is unloaded. Stops the microphone, shuts down and unloads the backend.
 */
EXPORT m64p_error CALL PluginShutdown(void) {
    m64p_error err = M64ERR_SUCCESS;

    aoc_mic_stop();
    if (sBackendLib != NULL) {
        if (bShutdown != NULL) {
            err = bShutdown();
        }
        lib_close(sBackendLib);
        sBackendLib = NULL;
    }
    return err;
}

/**
 * Mupen64Plus API: reports that this is an input plugin, its version, the input API version it implements and its
 * name. Any output pointer may be NULL.
 */
EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type* PluginType, int* PluginVersion, int* APIVersion,
                                        const char** PluginNamePtr, int* Capabilities) {
    if (PluginType != NULL) {
        *PluginType = M64PLUGIN_INPUT;
    }
    if (PluginVersion != NULL) {
        *PluginVersion = PLUGIN_VERSION;
    }
    if (APIVersion != NULL) {
        *APIVersion = INPUT_API_VERSION;
    }
    if (PluginNamePtr != NULL) {
        *PluginNamePtr = PLUGIN_NAME;
    }
    if (Capabilities != NULL) {
        *Capabilities = 0;
    }
    return M64ERR_SUCCESS;
}

/**
 * Picks the CONTROL stride (size of one entry of the core's controller array) from the array the backend just filled.
 * Newer cores (>= 2.6.0) always use 4 ints. Otherwise only ints 0..7 are read, which are inside the array for both
 * layouts (4 controllers x 3 ints = 12 ints minimum).
 *   3-int layout: ints[5] is Controls[1].Plugin, always >= PLUGIN_NONE (1)
 *   4-int layout: ints[5] is Controls[1].RawData (0 for the SDL backend), ints[6] is Controls[1].Plugin
 */
static size_t detect_control_stride(const CONTROL* controls) {
    const int* ints = (const int*)controls;

    if (sCoreVersion >= 0x020600) {
        return 4 * sizeof(int);
    }
    if (ints[5] == 0 && ints[6] >= PLUGIN_NONE) {
        return 4 * sizeof(int);
    }
    if (ints[5] >= PLUGIN_NONE) {
        return 3 * sizeof(int);
    }
    return 3 * sizeof(int);
}

/**
 * Mupen64Plus API: called when a game starts, to describe which controllers are plugged in.
 *
 * Lets the backend fill ports 1-3 as usual, detects the CONTROL layout, then, if the device is enabled, declares
 * port 4 as present in "raw data" mode with no accessory. Raw mode means the core does not interpret the port's
 * Joybus traffic itself: it hands every command to ReadController()/ControllerCommand(), which is how the custom
 * Audio Ocarina commands reach aoc_device_process(). Also starts the microphone if enabled.
 *
 * @param ControlInfo core structure with the controller array to fill
 */
EXPORT void CALL InitiateControllers(CONTROL_INFO ControlInfo) {
    bInitiateControllers(ControlInfo);
    sControlStride = detect_control_stride(ControlInfo.Controls);
    log_msg(M64MSG_INFO, "CONTROL size %u", (unsigned)sControlStride);

    if (sEnabled) {
        /* Present, RawData and Plugin are the first three ints in every core version */
        CONTROL* port4 = (CONTROL*)((char*)ControlInfo.Controls + AOC_PORT * sControlStride);

        port4->Present = 1;
        port4->RawData = 1;
        port4->Plugin = PLUGIN_NONE;
        if (sUseMic) {
            start_mic();
        }
    }
}

/**
 * Mupen64Plus API: returns the buttons of a standard controller. Port 4 reports no buttons (the core does not call
 * this for raw ports anyway); other ports go to the backend.
 */
EXPORT void CALL GetKeys(int Control, BUTTONS* Keys) {
    if (sEnabled && Control == AOC_PORT) {
        Keys->Value = 0;
        return;
    }
    bGetKeys(Control, Keys);
}

/**
 * Builds the device state for the current command: connected flag and note from the debug keys, and if no key is
 * held and the microphone is running, note/confidence/flags from the microphone. A held debug key always wins.
 * @param state output (romTranspose is not filled)
 */
static void current_state(AocDeviceState* state) {
    AocMicState mic;

    pthread_mutex_lock(&sLock);
    state->connected = sConnected;
    state->note = sKeyNote;
    pthread_mutex_unlock(&sLock);

    state->confidence = 255;
    state->flags = 0;
    if (state->note == AOC_NOTE_NONE && sUseMic && aoc_mic_is_running()) {
        aoc_mic_get_state(&mic);
        state->note = mic.note;
        state->confidence = mic.confidence;
        state->flags = mic.flags;
    }
}

/**
 * Handles one Joybus command sent by the console to port 4.
 *
 * `Command` points into the emulated PIF RAM at this channel's entry: [tx size][rx size][tx bytes...][rx bytes...].
 * The reply is written in place over the rx bytes. Errors are reported like on hardware, by setting bits in the rx
 * size byte: 0x80 = no device answered, 0x40 = the reply length differs from what the console asked for.
 *
 * Also applies a SET_TRANSPOSE from the ROM to the microphone analysis, and logs protocol traffic with DebugLog.
 *
 * @param Command pointer to the channel's tx size byte
 */
static void process_port4(unsigned char* Command) {
    AocDeviceState state;
    int txLen = Command[0] & 0x3F;
    int rxLen = Command[1] & 0x3F;
    unsigned char* tx = Command + 2;
    unsigned char* rx = tx + txLen;
    int replyLen;

    current_state(&state);
    state.romTranspose = sRomTranspose;
    replyLen = aoc_device_process(&state, tx, txLen, rx, rxLen);
    if (state.romTranspose != sRomTranspose) {
        sRomTranspose = state.romTranspose;
        aoc_mic_set_transpose(sPitchConfig.transpose + sRomTranspose);
        if (sDebugLog) {
            log_msg(M64MSG_INFO, "Port 4 SET_TRANSPOSE %+d semitones (octave %+d)", sRomTranspose, sRomTranspose / 12);
        }
    } else if (sDebugLog && txLen >= 2 && tx[0] == AOC_CMD_SET_TRANSPOSE) {
        log_msg(M64MSG_INFO, "Port 4 SET_TRANSPOSE %+d semitones (unchanged)", sRomTranspose);
    }

    if (txLen >= 1) {
        if (tx[0] == AOC_CMD_IDENTIFY) {
            sIdentifyCount++;
            if (sDebugLog) {
                log_msg(M64MSG_INFO, "Port 4 IDENTIFY #%lu -> %s", sIdentifyCount,
                        replyLen < 0 ? "no response" : "ok");
            }
        } else if (tx[0] == AOC_CMD_GET_STATE) {
            static int sLastLoggedNote = -1;

            sGetStateCount++;
            if (sDebugLog && state.note != sLastLoggedNote) {
                log_msg(M64MSG_INFO, "Port 4 GET_STATE #%lu: note %u -> %u (conf %u)", sGetStateCount,
                        sLastLoggedNote < 0 ? 0 : sLastLoggedNote, state.note, state.confidence);
                sLastLoggedNote = state.note;
            }
            if (sDebugLog && (sGetStateCount % 600) == 1) {
                log_msg(M64MSG_INFO, "Port 4 GET_STATE polling (#%lu), note=%u", sGetStateCount, state.note);
            }
        }
    }

    if (replyLen < 0) {
        Command[1] |= 0x80; /* no response */
    } else if (replyLen != rxLen) {
        Command[1] |= 0x40; /* size mismatch */
    }
}

/**
 * Mupen64Plus API: called when the console writes a command for a raw-data port. Port 4 answers everything in
 * ReadController() instead, so this is ignored for it; other ports go to the backend.
 */
EXPORT void CALL ControllerCommand(int Control, unsigned char* Command) {
    if (sEnabled && Control == AOC_PORT) {
        /* Everything is handled when the PIF processes the channel (ReadController) */
        return;
    }
    if (bControllerCommand != NULL) {
        bControllerCommand(Control, Command);
    }
}

/**
 * Mupen64Plus API: called when the PIF processes a raw-data port's command and the console expects the answer. For
 * port 4 the command is processed and answered by process_port4(); other ports go to the backend.
 *
 * @param Control port index (0-3)
 * @param Command pointer to the channel's entry in PIF RAM, may be NULL
 */
EXPORT void CALL ReadController(int Control, unsigned char* Command) {
    if (sEnabled && Control == AOC_PORT) {
        if (Command != NULL) {
            process_port4(Command);
        }
        return;
    }
    if (bReadController != NULL) {
        bReadController(Control, Command);
    }
}

/** Mupen64Plus API: a ROM was opened; forwarded to the backend. */
EXPORT int CALL RomOpen(void) {
    return bRomOpen != NULL ? bRomOpen() : 1;
}

/** Mupen64Plus API: the ROM was closed; forwarded to the backend. */
EXPORT void CALL RomClosed(void) {
    if (bRomClosed != NULL) {
        bRomClosed();
    }
}

/**
 * Recomputes the note produced by the debug keys after a key release: the most recently pressed note if it is still
 * held, otherwise the first held one, otherwise none. Called with sLock held.
 */
static void update_key_note(void) {
    int i;

    /* Keep the most recently pressed note if still held, otherwise any held note */
    if (sKeyNote != AOC_NOTE_NONE && sKeyHeld[sKeyNote - 1]) {
        return;
    }
    sKeyNote = AOC_NOTE_NONE;
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        if (sKeyHeld[i]) {
            sKeyNote = (uint8_t)(AOC_NOTE_A + i);
            break;
        }
    }
}

/**
 * Mupen64Plus API: a key was pressed in the emulator window. Forwarded to the backend (keyboard controls for ports
 * 1-3), then handles the debug keys: note keys (held = that note is heard), the unplug/replug key and the microphone
 * on/off key.
 *
 * @param keymod modifier keys
 * @param keysym SDL2 scancode of the key
 */
EXPORT void CALL SDL_KeyDown(int keymod, int keysym) {
    int i;

    if (bKeyDown != NULL) {
        bKeyDown(keymod, keysym);
    }
    if (!sEnabled) {
        return;
    }

    if (sDebugLog) {
        log_msg(M64MSG_INFO, "key down scancode %d", keysym);
    }
    pthread_mutex_lock(&sLock);
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        if (keysym == sNoteKeys[i] && !sKeyHeld[i]) {
            sKeyHeld[i] = 1;
            sKeyNote = (uint8_t)(AOC_NOTE_A + i);
        }
    }
    if (keysym == sDisconnectKey) {
        sConnected = !sConnected;
        log_msg(M64MSG_INFO, "Port 4 device %s", sConnected ? "plugged in" : "unplugged");
    }
    pthread_mutex_unlock(&sLock);

    if (keysym == sMicToggleKey) {
        sUseMic = !sUseMic;
        if (sUseMic && !aoc_mic_is_running()) {
            start_mic();
        }
        log_msg(M64MSG_INFO, "microphone %s", sUseMic ? "on" : "off");
    }
}

/**
 * Mupen64Plus API: a key was released. Forwarded to the backend, then releases the debug note key if it was one.
 */
EXPORT void CALL SDL_KeyUp(int keymod, int keysym) {
    int i;

    if (bKeyUp != NULL) {
        bKeyUp(keymod, keysym);
    }
    if (!sEnabled) {
        return;
    }

    pthread_mutex_lock(&sLock);
    for (i = 0; i < AOC_NOTE_COUNT; i++) {
        if (keysym == sNoteKeys[i]) {
            sKeyHeld[i] = 0;
        }
    }
    update_key_note();
    pthread_mutex_unlock(&sLock);
}
