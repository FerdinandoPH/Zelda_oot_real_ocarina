Zelda OoT Real Ocarina: input plugin for Mupen64Plus (Linux x86_64)
===================================================================

[English]

Needs Mupen64Plus 2.5 or newer with mupen64plus-input-sdl, e.g. on Debian/Ubuntu:
    sudo apt install mupen64plus-ui-console mupen64plus-input-sdl mupen64plus-video-glide64mk2 \
                     mupen64plus-audio-sdl mupen64plus-rsp-hle

1. Copy mupen64plus-input-ocarina.so into the Mupen64Plus plugin directory, next to mupen64plus-input-sdl.so.
   ./install.sh finds that directory and does it for you (asking for sudo if needed). By hand, on Debian/Ubuntu:
       sudo cp mupen64plus-input-ocarina.so /usr/lib/x86_64-linux-gnu/mupen64plus/
   (other distributions: /usr/lib/mupen64plus, /usr/lib64/mupen64plus or /usr/local/lib/mupen64plus).
   To install somewhere else: ./install.sh /path/to/plugin/dir
2. mupen64plus --input mupen64plus-input-ocarina zelda-oot-real-ocarina.z64
   In a graphical frontend (e.g. mupen64plus-qt) choose "mupen64plus-input-ocarina" as the input plugin.

Without installing: mupen64plus --input /full/path/to/mupen64plus-input-ocarina.so zelda-oot-real-ocarina.z64

Controllers 1-3 keep working exactly as with mupen64plus-input-sdl. The plugin emulates the audio device on
port 4 with your default microphone. Settings: section [Input-Ocarina] of ~/.config/mupen64plus/mupen64plus.cfg.

aoc-mic-probe shows which notes the game would hear, without the emulator:
    ./aoc-mic-probe --list      (capture devices)
    ./aoc-mic-probe             (play and watch the ">>" lines)

Debug keys (no microphone needed): R T N U O = A, C-Down, C-Right, C-Left, C-Up notes; H = unplug the device;
V = microphone on/off.

[Español]

Necesita Mupen64Plus 2.5 o posterior con mupen64plus-input-sdl, por ejemplo en Debian/Ubuntu:
    sudo apt install mupen64plus-ui-console mupen64plus-input-sdl mupen64plus-video-glide64mk2 \
                     mupen64plus-audio-sdl mupen64plus-rsp-hle

1. Copia mupen64plus-input-ocarina.so en la carpeta de plugins de Mupen64Plus, junto a mupen64plus-input-sdl.so.
   ./install.sh busca esa carpeta y lo hace por ti (pide sudo si hace falta). A mano, en Debian/Ubuntu:
       sudo cp mupen64plus-input-ocarina.so /usr/lib/x86_64-linux-gnu/mupen64plus/
   (otras distribuciones: /usr/lib/mupen64plus, /usr/lib64/mupen64plus o /usr/local/lib/mupen64plus).
   Para instalarlo en otro sitio: ./install.sh /ruta/a/los/plugins
2. mupen64plus --input mupen64plus-input-ocarina zelda-oot-real-ocarina.z64
   En un frontend gráfico (p. ej. mupen64plus-qt) elige "mupen64plus-input-ocarina" como plugin de entrada.

Sin instalar: mupen64plus --input /ruta/completa/a/mupen64plus-input-ocarina.so zelda-oot-real-ocarina.z64

Los mandos 1-3 funcionan igual que con mupen64plus-input-sdl. El plugin emula el dispositivo de audio en el
puerto 4 con tu micrófono por defecto. Ajustes: sección [Input-Ocarina] de ~/.config/mupen64plus/mupen64plus.cfg.

aoc-mic-probe muestra qué notas oiría el juego, sin el emulador:
    ./aoc-mic-probe --list      (dispositivos de captura)
    ./aoc-mic-probe             (toca y mira las líneas ">>")

Teclas de depuración (sin micrófono): R T N U O = notas A, C-Abajo, C-Derecha, C-Izquierda, C-Arriba;
H = desenchufar el dispositivo; V = micrófono sí/no.
