#!/usr/bin/env bash
# Creates the decomp checkout in oot/ with the ROM hack applied: fetches zeldaret/oot at rom/UPSTREAM_COMMIT,
# applies rom/audio-ocarina.patch and copies rom/files/. The result is on branch audio-ocarina, uncommitted.
#   tools/setup-oot.sh [dir]   (default: oot/; it must not exist yet)
# The clone is shallow; run `git fetch --unshallow` inside it for the full upstream history.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIR="${1:-$ROOT/oot}"
BASE="$(cat "$ROOT/rom/UPSTREAM_COMMIT")"

if [ -e "$DIR" ]; then
    echo "$DIR already exists" >&2
    exit 1
fi
git init -q -b main "$DIR"
git -C "$DIR" remote add origin https://github.com/zeldaret/oot.git
git -C "$DIR" fetch -q --depth 1 origin "$BASE"
git -C "$DIR" checkout -q -b audio-ocarina FETCH_HEAD
git -C "$DIR" apply --whitespace=nowarn "$ROOT/rom/audio-ocarina.patch"
cp -r "$ROOT/rom/files/." "$DIR/"
echo "oot/ is ready at zeldaret/oot ${BASE:0:9} + Zelda OoT Real Ocarina."
echo "Next: put your NTSC-U 1.0 ROM at $DIR/baseroms/ntsc-1.0/baserom.z64 and run"
echo "  make -C $DIR setup VERSION=ntsc-1.0"
