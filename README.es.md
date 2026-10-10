# Zelda OoT Real Ocarina

[English](README.md) · **Español**

Toca las canciones de *The Legend of Zelda: Ocarina of Time* con una ocarina de verdad. Saca la ocarina en el
juego, pulsa Cruceta Arriba y toca: el juego escucha tu instrumento en lugar de los botones C.

Funciona en emulador, con el micrófono del PC, y en una N64 real, con el **ORU** (Ocarina Recognition Unit), un
pequeño dispositivo enchufado al puerto de mando 4. Las partes:

- **ROM hack** de la versión NTSC-U 1.0 de N64, compilado a partir de la decompilación
  [zeldaret/oot](https://github.com/zeldaret/oot). Se distribuye como parche (`.bps`), así que necesitas tu propia
  ROM.
- **Plugin de Mupen64Plus** para Linux y Windows. Emula el ORU con tu micrófono.
- **Firmware del ORU**: el ORU es una Raspberry Pi Pico, Pico W, Pico 2 o Pico 2 W con un micrófono INMP441.
  Compila, pero **todavía no se ha probado en hardware**.

Ni este repositorio ni sus releases incluyen ninguna ROM ni recurso del juego.

- [Jugar](#jugar)
  - [1. Parchea tu ROM](#1-parchea-tu-rom)
  - [2a. En emulador](#2a-en-emulador-mupen64plus)
  - [2b. En una N64 real](#2b-en-una-n64-real-oru)
  - [En el juego](#en-el-juego)
  - [Ajustar el micrófono](#ajustar-el-micrófono)
- [Desarrollo](#desarrollo)
- [Licencia](#licencia)

## Jugar

Descarga los ficheros de la [última release](../../releases/latest).

### 1. Parchea tu ROM

Necesitas *The Legend of Zelda: Ocarina of Time* **(USA) versión 1.0** en formato `.z64` (big-endian):

| | |
|-|-|
| SHA-1 | `ad69c91157f6705e8ab06c79fe08aad47bb57ba7` |
| MD5 | `5bd1fe107bf8106b2ab6650abecd54d6` |

Las demás versiones (1.1, 1.2, PAL, GameCube) no sirven por ahora. Un `.n64` o `.v64` tiene el contenido correcto
pero con otro orden de bytes: conviértelo antes a `.z64` (por ejemplo con Tool64).

Aplica `zelda-oot-real-ocarina-<versión>.bps`:

- en el navegador, con [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/): elige tu ROM y el parche y
  pulsa *Apply patch*;
- o con [Floating IPS](https://github.com/Alcaro/Flips): `flips --apply zelda-oot-real-ocarina-<versión>.bps oot-us-1.0.z64 zelda-oot-real-ocarina.z64`.

El resultado es una ROM normal de 32 MB para emuladores y flash carts.

### 2a. En emulador (Mupen64Plus)

El plugin funciona con Mupen64Plus 2.5 o posterior y con los frontends que usan sus plugins estándar (como
mupen64plus-qt). Envuelve al plugin habitual `mupen64plus-input-sdl`, así que los mandos 1-3 siguen funcionando
como siempre. El plugin añade el dispositivo de audio en el puerto 4.

**Linux (x86_64).** Instala antes Mupen64Plus, por ejemplo con `sudo apt install mupen64plus-ui-console
mupen64plus-input-sdl mupen64plus-video-glide64mk2 mupen64plus-audio-sdl mupen64plus-rsp-hle`.

Instalarlo consiste en copiar `mupen64plus-input-ocarina.so` en la carpeta de plugins de Mupen64Plus, junto a
`mupen64plus-input-sdl.so`. Esa carpeta es del sistema (de ahí el `sudo`) y depende de la distribución:
`/usr/lib/x86_64-linux-gnu/mupen64plus/` en Debian/Ubuntu, `/usr/lib/mupen64plus/` o `/usr/lib64/mupen64plus/` en
otras, y `/usr/local/lib/mupen64plus/` si compilaste Mupen64Plus tú mismo. `install.sh` la busca y copia el fichero
por ti:

```sh
tar xzf zelda-oot-real-ocarina-mupen64plus-linux-x86_64.tar.gz
cd zelda-oot-real-ocarina-mupen64plus-linux-x86_64
./install.sh       # o a mano, p. ej.: sudo cp mupen64plus-input-ocarina.so /usr/lib/x86_64-linux-gnu/mupen64plus/
mupen64plus --input mupen64plus-input-ocarina zelda-oot-real-ocarina.z64
```

También puedes usarlo sin instalar, dándole la ruta completa (aunque los frontends gráficos solo listan los plugins
de su carpeta de plugins):

```sh
mupen64plus --input /ruta/a/mupen64plus-input-ocarina.so zelda-oot-real-ocarina.z64
```

**Windows (x64).** Usa la versión para Windows de Mupen64Plus (`mupen64plus-bundle-win64`, en las
[releases de Mupen64Plus](https://github.com/mupen64plus/mupen64plus-core/releases)). Descomprime
`zelda-oot-real-ocarina-mupen64plus-windows-x64.zip` y copia `mupen64plus-input-ocarina.dll` en la carpeta de
Mupen64Plus, junto a `mupen64plus-input-sdl.dll`. Con eso ya está instalado. Después, desde esa carpeta:

```bat
mupen64plus-ui-console.exe --input mupen64plus-input-ocarina.dll zelda-oot-real-ocarina.z64
```

En un frontend gráfico, elige `mupen64plus-input-ocarina` como plugin de entrada.

Sin micrófono también puedes probarlo con el teclado. Mantén una tecla para mantener una nota y suéltala para
dejar silencio:

| Tecla | Acción |
|-------|--------|
| R | Nota A (Re) |
| T | C-Abajo (Fa) |
| N | C-Derecha (La) |
| U | C-Izquierda (Si) |
| O | C-Arriba (Re agudo) |
| H | Desenchufar / volver a enchufar el dispositivo del puerto 4 |
| V | Micrófono sí / no |

Por ejemplo, la Nana de Zelda es U O N U O N, con una pausa corta entre notas.

### 2b. En una N64 real (ORU)

1. Monta el ORU: una Raspberry Pi Pico, Pico W, Pico 2 o Pico 2 W, un módulo de micrófono INMP441, un
   diodo Schottky y un conector de mando de N64 (tendrás que cortarlo y soldar los cables a la Pico). El cableado
   está en [firmware/pico/README.es.md](firmware/pico/README.es.md#cableado).
2. Descomprime `oru-firmware.zip`. Mantén pulsado **BOOTSEL**, conecta la Pico por USB y copia el
   `.uf2` de tu placa (`pico`, `pico_w`, `pico2` o `pico2_w`) en la unidad que aparece.
3. Enchufa el ORU al **puerto de mando 4** y arranca la ROM parcheada desde un flash cart.

El firmware tiene una consola serie por USB para ajustes y diagnóstico, y una lista de comprobación para la primera
prueba. Las dos están en [firmware/pico/README.es.md](firmware/pico/README.es.md).

### En el juego

1. Saca la ocarina. Aparece el aviso **"D-Pad Up: Audio"**.
2. Pulsa **Cruceta Arriba**. El juego busca el dispositivo en el puerto 4.
   - Si lo encuentra, muestra **"Listening...  B: Cancel"**. Los botones de nota del mando se ignoran.
   - Si no, muestra **"Audio device not found."** y el juego sigue con normalidad.
3. Toca. Cada nota estable aparece en el pentagrama:
   - una nota mantenida cuenta una vez;
   - una nota, silencio y la misma nota cuenta dos veces;
   - el pentagrama guarda las 8 últimas notas.
4. Cuando tocas una canción que el juego admite en ese momento, sigue la secuencia normal de la canción.
5. Mientras escucha, **Cruceta Arriba/Abajo** desplaza las notas esperadas una octava (-2..+2). Se muestra como
   **"D-Pad: Octave +1"**. Sirve para ocarinas sopranos o bajas, o para silbar. El ajuste dura hasta que apagas la
   consola.
6. Pulsa **B** para salir del modo audio y volver al mando.

Las notas objetivo son Re5, Fa5, La5, Si5 y Re6, las mismas que toca el juego.

Dónde funciona el modo audio:

| Situación | Modo audio |
|-----------|------------|
| Tocar libremente desde un botón C (teletransportes, Saria, Tormentas, Sol, …) | Sí |
| Avisos de "toca una canción aquí" (símbolos de la Trifuerza, bloques del Tiempo, Malon, canciones de las ranas, …) | Sí |
| Repetir una canción que te están enseñando | Sí. Sigue activo tras los intentos fallidos |
| Juego de memoria de Skull Kid (Bosque Perdido) | Sí, en tu turno. Lo que oye el dispositivo en el turno de Skull Kid se ignora |
| Juego de notas de las ranas (tras tocarles las 6 canciones) | Sí. B cancela la escucha en lugar de salir del juego |
| Grabar la canción del espantapájaros | No: graba la duración de las notas y los semitonos con R/Z, que el modo audio no produce |

### Ajustar el micrófono

Los ajustes del plugin están en la sección `[Input-Ocarina]` de `mupen64plus.cfg` (`~/.config/mupen64plus/` en
Linux, `%APPDATA%\Mupen64Plus\` en Windows). La sección se crea la primera vez que se ejecuta el plugin.

| Ajuste | Por defecto | Significado |
|--------|-------------|-------------|
| `Enabled` | true | Emular el dispositivo en el puerto 4 |
| `Microphone` | true | Usar el micrófono (las teclas de depuración funcionan siempre) |
| `CaptureDevice` | "" | Parte del nombre del dispositivo de entrada (`aoc-mic-probe --list` los lista); vacío = el predeterminado |
| `Transpose` | 0 | Semitonos que se suman a las notas objetivo, p. ej. `-12` para una ocarina una octava más grave |
| `ToleranceCents` | 60 | Cuánto puede alejarse una altura de la nota objetivo |
| `MinRms` | 0.01 | Nivel de entrada por debajo del cual no hay nota |
| `YinThreshold` | 0.20 | Exigencia del detector (más bajo = más exigente) |
| `MinFrequency` / `MaxFrequency` | 0 / 0 | Rango extra de búsqueda en Hz. El rango siempre cubre las notas objetivo; esto solo lo amplía |
| `StickyNotes` | false | *Experimental, solo para silbar/tararear.* Mantiene la nota a través del vibrato y los glissandos. Empeora la ocarina |
| `HoldMs` | 0 | *Experimental, solo para silbar/tararear.* Rellena cortes de altura de hasta esta duración (ms). Une notas repetidas de ocarina |
| `BackendPlugin` | mupen64plus-input-sdl | Plugin de entrada para los mandos 1-3 (nombre de fichero o ruta) |
| `DebugLog` | false | Registrar las notas detectadas y el tráfico del puerto 4 |
| `KeyNote*` / `KeyDisconnect` / `KeyMicToggle` | | Teclas de depuración, como scancodes de SDL2 |

`aoc-mic-probe`, incluido con el plugin, muestra lo que oiría el juego sin abrir el emulador. Ejecútalo desde una
terminal y toca: las líneas que empiezan por `>>` son las notas que registraría el juego.

```sh
aoc-mic-probe --list                            # dispositivos de captura
aoc-mic-probe --device "USB" --transpose -12    # mismo significado que los ajustes de arriba
aoc-mic-probe --octave 1                        # igual que "Octave +1" en el juego
aoc-mic-probe --raw --log ocarina.csv           # muestra también cada cambio del detector / guarda una línea temporal
```

## Desarrollo

### Estructura del repositorio

En el código, la función de la ROM y su protocolo del puerto 4 se llaman *Audio Ocarina* (`AUDIO_OCARINA=1`, `aoc_*`).

| Ruta | Contenido |
|------|-----------|
| `rom/` | El ROM hack, como cambios sobre zeldaret/oot: `UPSTREAM_COMMIT`, `audio-ocarina.patch` (ficheros originales modificados) y `files/` (ficheros nuevos) |
| `tools/` | `setup-oot.sh` (crea `oot/` a partir de `rom/`), `export-oot.sh` (vuelca `oot/` a `rom/`), `make-release.sh` (genera el `.bps`) |
| `emulator/mupen64plus-input-ocarina/` | Plugin proxy de entrada para Mupen64Plus y `aoc-mic-probe` |
| `firmware/pico/` | Firmware del ORU (Raspberry Pi Pico) |
| `protocol/protocol.md` | El protocolo Joybus del puerto 4, común a la ROM, el plugin y el firmware |
| `docs/architecture.md` | Visión general del diseño |
| `docs/doxygen/` | Configuración de Doxygen para el código del plugin y del ROM hack (`build_docs.sh`) |
| `tests/` | Tests unitarios en el PC: filtro de notas de la ROM, modelo del dispositivo, detector de altura, DSP del firmware |

### El checkout del decomp

El ROM hack vive en un checkout de zeldaret/oot en `oot/`. Este repositorio ignora ese directorio; solo se
commitean los cambios de `rom/`.

```sh
tools/setup-oot.sh      # clona zeldaret/oot en rom/UPSTREAM_COMMIT dentro de oot/ y aplica rom/
```

El checkout es superficial (shallow); ejecuta `git fetch --unshallow` dentro de `oot/` si quieres el historial de
upstream. Las dependencias de compilación están en el README de zeldaret/oot.

Trabaja en `oot/` como siempre. Cuando termines, vuelca los cambios a `rom/` y commitéalos:

```sh
tools/export-oot.sh     # rom/audio-ocarina.patch + rom/files/ desde oot/ (commiteado o no)
git add rom && git commit
```

`export-oot.sh` cubre los ficheros modificados y añadidos. Avisa si `oot/` borra o renombra ficheros originales.

### Compilar la ROM

```sh
cp /ruta/a/oot-us-1.0.z64 oot/baseroms/ntsc-1.0/baserom.z64
cd oot
make setup VERSION=ntsc-1.0                      # una vez
make VERSION=ntsc-1.0 AUDIO_OCARINA=1 -j$(nproc)
# -> build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0.z64
#    añade "compress" para la ROM de 32 MB: build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0-compressed.z64
```

`AUDIO_OCARINA=1` compila con el texto en inglés de EE. UU. (`REGION=US`) en su propio directorio de build y
desactiva la comparación de checksums. Sin esa opción la build sigue siendo idéntica byte a byte a la original:
`make VERSION=ntsc-1.0 REGION=US` imprime `OK`. No uses `NON_MATCHING=1`: rompe la build con IDO.

**ROM de test.** `make VERSION=ntsc-1.0 AUDIO_OCARINA_TEST=1` compila `build/ntsc-1.0-audio-ocarina-test/` con
variables que un depurador puede escribir para teletransportarse a una entrada, mover a Link, abrir directamente
cualquier acción de ocarina y hacer el papel de las ranas y de Skull Kid. Están en
`Message_UpdateAudioOcarinaTestHooks` (`src/code/z_message.c`); sus direcciones están en el `.map` de la build.

### Compilar el plugin

El plugin solo necesita un compilador de C. miniaudio y las cabeceras de la API de Mupen64Plus vienen en
`third_party/`. La herramienta `aoc-mic-probe` también compila el filtro de notas de la ROM, así que antes ejecuta
`tools/setup-oot.sh` (o pasa `OOT=/ruta/a/oot`).

```sh
cd emulator/mupen64plus-input-ocarina
make                                    # build/mupen64plus-input-ocarina.so, build/aoc-mic-probe
make CC=x86_64-w64-mingw32-gcc          # Windows: build-win64/mupen64plus-input-ocarina.dll, aoc-mic-probe.exe
make package                            # el archivo de la release (.tar.gz, o .zip para Windows)
```

La build de Windows funciona con MinGW-w64 GCC o con [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), que es
el que usa el CI. Enlaza todo estáticamente, así que no necesita DLLs adicionales.

Para probarlo sin instalarlo:

```sh
mupen64plus --input "$PWD/build/mupen64plus-input-ocarina.so" ../../oot/build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0.z64
```

El plugin busca `BackendPlugin` (se puede forzar con la variable de entorno `AOC_BACKEND_PLUGIN`) en su propio
directorio, luego en la ruta de búsqueda de bibliotecas y luego en los directorios de plugins habituales del
sistema. `AOC_MICROPHONE=0/1` y `AOC_DEBUG_LOG=0/1` tienen prioridad sobre el fichero de ajustes, lo que viene bien
para pruebas automáticas.

### Compilar el firmware del ORU

Necesitas el [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x (el CI usa 2.3.0) en `PICO_SDK_PATH`,
`arm-none-eabi-gcc`, `cmake`, y `ninja` o `make`.

```sh
firmware/pico/build.sh          # todas las placas -> firmware/pico/dist/oru-<placa>.uf2
firmware/pico/build.sh pico2    # solo una
```

El diseño, la consola serie y las constantes de tiempos del Joybus están en
[firmware/pico/README.es.md](firmware/pico/README.es.md).

### Tests y documentación

```sh
make -C tests test              # necesita oot/ (u OOT=/ruta/a/oot)
docs/doxygen/build_docs.sh      # docs/doxygen/out/{plugin,n64}/html/index.html, necesita doxygen
```

### Releases

El [workflow Build](.github/workflows/build.yml) pasa los tests y compila el plugin (Linux y Windows) y el firmware
en cada push. También comprueba que `rom/` sigue aplicándose sobre el commit fijado de zeldaret/oot. Compilar la
ROM necesita la baserom, que no se puede subir, así que el `.bps` se genera en local:

1. Commitea todo, incluida la salida de `tools/export-oot.sh`, y haz push.
2. `git tag v1.0.0 && git push origin v1.0.0`. El CI crea la release con el plugin y el firmware.
3. `tools/make-release.sh v1.0.0`. Comprueba que `oot/` coincide con `rom/` y que el tag es el commit actual.
   Compila la ROM comprimida, crea `dist/zelda-oot-real-ocarina-v1.0.0.bps` con Floating IPS (que compila
   automáticamente si `flips` no está instalado), comprueba que el parche reproduce la ROM y lo sube a la release
   con `gh`.

## Licencia

El código de este repositorio se distribuye bajo la GNU General Public License, versión 2 o (a tu elección)
cualquier versión posterior. Consulta [LICENSE](LICENSE).

Código de terceros:

- `emulator/mupen64plus-input-ocarina/third_party/m64p/`: cabeceras de la API de Mupen64Plus, GPL-2.0-or-later.
- `emulator/mupen64plus-input-ocarina/third_party/miniaudio/`: [miniaudio](https://miniaud.io), dominio público o
  MIT-0.
- `rom/` es un conjunto de cambios sobre la decompilación [zeldaret/oot](https://github.com/zeldaret/oot). La
  decompilación en sí no se incluye: `tools/setup-oot.sh` la descarga de su propio repositorio.

*The Legend of Zelda: Ocarina of Time* es © Nintendo. Este proyecto no está afiliado a Nintendo y no distribuye
ninguna parte del juego.
