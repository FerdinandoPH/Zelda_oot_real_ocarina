Zelda OoT Real Ocarina: input plugin for Mupen64Plus (Windows x64)
==================================================================

[English]

Needs the Mupen64Plus 2.5 or newer Windows build (mupen64plus-bundle-win64 from
https://github.com/mupen64plus/mupen64plus-core/releases), which includes mupen64plus-input-sdl.dll.

1. Copy mupen64plus-input-ocarina.dll into the Mupen64Plus folder, next to mupen64plus-input-sdl.dll.
   That is the whole installation.
2. Run from that folder:
       mupen64plus-ui-console.exe --input mupen64plus-input-ocarina.dll zelda-oot-real-ocarina.z64
   In a graphical frontend that uses the standard Mupen64Plus plugins, choose "mupen64plus-input-ocarina" as the
   input plugin.

Controllers 1-3 keep working exactly as with mupen64plus-input-sdl. The plugin emulates the audio device on
port 4 with your default microphone. Settings: section [Input-Ocarina] of mupen64plus.cfg
(%APPDATA%\Mupen64Plus\mupen64plus.cfg). Frontends with their own input plugin: set BackendPlugin to its file
name (untested).

aoc-mic-probe.exe shows which notes the game would hear, without the emulator (run it from a terminal):
    aoc-mic-probe.exe --list      (capture devices)
    aoc-mic-probe.exe             (play and watch the ">>" lines)

Debug keys (no microphone needed): R T N U O = A, C-Down, C-Right, C-Left, C-Up notes; H = unplug the device;
V = microphone on/off.

[Español]

Necesita la versión para Windows de Mupen64Plus 2.5 o posterior (mupen64plus-bundle-win64 de
https://github.com/mupen64plus/mupen64plus-core/releases), que incluye mupen64plus-input-sdl.dll.

1. Copia mupen64plus-input-ocarina.dll en la carpeta de Mupen64Plus, junto a mupen64plus-input-sdl.dll.
   Con eso ya está instalado.
2. Ejecuta desde esa carpeta:
       mupen64plus-ui-console.exe --input mupen64plus-input-ocarina.dll zelda-oot-real-ocarina.z64
   En un frontend gráfico que use los plugins estándar de Mupen64Plus, elige "mupen64plus-input-ocarina" como
   plugin de entrada.

Los mandos 1-3 funcionan igual que con mupen64plus-input-sdl. El plugin emula el dispositivo de audio en el
puerto 4 con tu micrófono por defecto. Ajustes: sección [Input-Ocarina] de mupen64plus.cfg
(%APPDATA%\Mupen64Plus\mupen64plus.cfg). Frontends con su propio plugin de entrada: pon en BackendPlugin el
nombre de su fichero (sin probar).

aoc-mic-probe.exe muestra qué notas oiría el juego, sin el emulador (ejecútalo desde una terminal):
    aoc-mic-probe.exe --list      (dispositivos de captura)
    aoc-mic-probe.exe             (toca y mira las líneas ">>")

Teclas de depuración (sin micrófono): R T N U O = notas A, C-Abajo, C-Derecha, C-Izquierda, C-Arriba;
H = desenchufar el dispositivo; V = micrófono sí/no.
