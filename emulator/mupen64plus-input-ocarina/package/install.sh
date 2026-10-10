#!/usr/bin/env bash
# Installs the Zelda OoT Real Ocarina input plugin next to the system's Mupen64Plus plugins
# (the directory that holds mupen64plus-input-sdl.so), or into the directory given as argument.
#   ./install.sh [plugin-dir]
set -euo pipefail
cd "$(dirname "$0")"

dir="${1:-}"
if [ -z "$dir" ]; then
    for d in /usr/lib/x86_64-linux-gnu/mupen64plus /usr/lib/mupen64plus /usr/lib64/mupen64plus \
             /usr/local/lib/mupen64plus; do
        if [ -e "$d/mupen64plus-input-sdl.so" ]; then
            dir="$d"
            break
        fi
    done
fi
if [ -z "$dir" ]; then
    echo "Mupen64Plus plugin directory not found (no mupen64plus-input-sdl.so)." >&2
    echo "Install Mupen64Plus first (e.g. sudo apt install mupen64plus-ui-console mupen64plus-input-sdl)" >&2
    echo "or pass the plugin directory: ./install.sh /path/to/plugins" >&2
    exit 1
fi

sudo=()
[ -w "$dir" ] || sudo=(sudo)
"${sudo[@]}" install -m 644 mupen64plus-input-ocarina.so "$dir/"
echo "Installed $dir/mupen64plus-input-ocarina.so"
echo "Run: mupen64plus --input mupen64plus-input-ocarina your-hacked-rom.z64"
