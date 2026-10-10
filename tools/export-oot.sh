#!/usr/bin/env bash
# Exports the ROM hack from the decomp checkout in oot/ into rom/, the form that is committed to this repository:
#   rom/UPSTREAM_COMMIT        zeldaret/oot commit the hack is based on
#   rom/audio-ocarina.patch    changes to vanilla files
#   rom/files/                 files added by the hack
# Run it after changing the hack in oot/ (committed or not), then commit rom/.
#   tools/export-oot.sh [out-dir]   (default: rom/)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OOT="$ROOT/oot"
OUT="${1:-$ROOT/rom}"
BASE="$(cat "$ROOT/rom/UPSTREAM_COMMIT")"

git -C "$OOT" cat-file -e "$BASE^{commit}" 2>/dev/null || { echo "oot/ does not contain commit $BASE" >&2; exit 1; }

mkdir -p "$OUT"
[ "$OUT" = "$ROOT/rom" ] || cp "$ROOT/rom/UPSTREAM_COMMIT" "$OUT/"
# Modified vanilla files (working tree against the upstream commit)
git -C "$OOT" diff --no-color --diff-filter=M "$BASE" > "$OUT/audio-ocarina.patch"
# Added files: committed since the upstream commit, or untracked (but not ignored)
rm -rf "$OUT/files"
{
    git -C "$OOT" diff --name-only --diff-filter=A "$BASE"
    git -C "$OOT" ls-files --others --exclude-standard
} | sort -u | while read -r f; do
    mkdir -p "$OUT/files/$(dirname "$f")"
    cp "$OOT/$f" "$OUT/files/$f"
done
if git -C "$OOT" diff --name-only --diff-filter=DRT "$BASE" | grep -q .; then
    echo "warning: oot/ deletes, renames or retypes vanilla files; the export only covers modified and added files" >&2
fi
echo "Exported to $OUT:"
git -C "$OOT" diff --stat --diff-filter=M "$BASE" | tail -1
(cd "$OUT/files" && find . -type f | sed 's|^\./|  + |')
