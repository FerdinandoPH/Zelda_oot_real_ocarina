#!/usr/bin/env bash
# Builds the ROM hack from oot/ and publishes it as a BPS patch against the NTSC-U 1.0 ROM.
# The baserom cannot be shared, so this runs on your machine instead of in CI.
#   tools/make-release.sh <tag>              build dist/zelda-oot-real-ocarina-<tag>.bps and upload it to the GitHub release
#   tools/make-release.sh <tag> --no-upload  only build the patch
# Push the tag first: CI creates the release with the plugin and firmware; this adds the patch to it (or creates the
# release if CI has not yet). Needs gh (logged in) for the upload. Uses `flips` from PATH or FLIPS, or builds it.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OOT="$ROOT/oot"
TAG="${1:?usage: tools/make-release.sh <tag> [--no-upload]}"
UPLOAD=1
[ "${2:-}" = "--no-upload" ] && UPLOAD=0

BASEROM="$OOT/baseroms/ntsc-1.0/baserom.z64"
BASEROM_SHA1=ad69c91157f6705e8ab06c79fe08aad47bb57ba7
ROM="$OOT/build/ntsc-1.0-audio-ocarina/oot-ntsc-1.0-compressed.z64"
FLIPS_COMMIT=ff216a75df0987047a67d7923567dc4482ce07ac
OUT="$ROOT/dist"
BPS="$OUT/zelda-oot-real-ocarina-$TAG.bps"

die() { echo "error: $*" >&2; exit 1; }

[ -f "$BASEROM" ] || die "missing $BASEROM (NTSC-U 1.0 .z64)"
[ "$(sha1sum "$BASEROM" | cut -d' ' -f1)" = "$BASEROM_SHA1" ] || die "$BASEROM is not NTSC-U 1.0 (.z64, SHA1 $BASEROM_SHA1)"

# The patch must be built from exactly the source committed in rom/
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"$ROOT/tools/export-oot.sh" "$tmp/rom" >/dev/null
diff -r "$ROOT/rom" "$tmp/rom" >/dev/null || die "oot/ differs from rom/: run tools/export-oot.sh and commit rom/"
if [ "$UPLOAD" = 1 ]; then
    git -C "$ROOT" diff --quiet HEAD -- rom || die "rom/ has uncommitted changes"
    [ "$(git -C "$ROOT" rev-parse "$TAG^{commit}" 2>/dev/null)" = "$(git -C "$ROOT" rev-parse HEAD)" ] ||
        die "tag $TAG does not exist or is not the checked-out commit"
fi

make -C "$OOT" VERSION=ntsc-1.0 AUDIO_OCARINA=1 -j"$(nproc)" compress

FLIPS="${FLIPS:-$(command -v flips || true)}"
if [ -z "$FLIPS" ]; then
    cache="${XDG_CACHE_HOME:-$HOME/.cache}/zelda-oot-real-ocarina/flips-$FLIPS_COMMIT"
    if [ ! -x "$cache/flips" ]; then
        echo "Building Floating IPS in $cache"
        rm -rf "$cache"
        git init -q "$cache"
        git -C "$cache" fetch -q --depth 1 https://github.com/Alcaro/Flips.git "$FLIPS_COMMIT"
        git -C "$cache" checkout -q FETCH_HEAD
        make -C "$cache" TARGET=cli CFLAGS=-O2 >"$cache/build.log" 2>&1 || { cat "$cache/build.log"; exit 1; }
    fi
    FLIPS="$cache/flips"
fi

mkdir -p "$OUT"
"$FLIPS" --create --bps-delta "$BASEROM" "$ROM" "$BPS" >/dev/null
"$FLIPS" --apply "$BPS" "$BASEROM" "$tmp/patched.z64" >/dev/null
cmp -s "$ROM" "$tmp/patched.z64" || die "applying $BPS does not reproduce the built ROM"
echo "$BPS ($(du -h "$BPS" | cut -f1)), patched ROM SHA1 $(sha1sum "$ROM" | cut -d' ' -f1)"

if [ "$UPLOAD" = 1 ]; then
    cd "$ROOT"
    gh release view "$TAG" >/dev/null 2>&1 || gh release create "$TAG" --verify-tag --title "$TAG" --notes ""
    gh release upload "$TAG" "$BPS" --clobber
fi
