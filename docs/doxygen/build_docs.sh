#!/usr/bin/env bash
# Generates the Doxygen HTML documentation:
#   out/plugin/html/index.html  mupen64plus-input-ocarina
#   out/n64/html/index.html     code added/modified in the oot ROM hack
# Needs doxygen in PATH (sudo apt install doxygen) or DOXYGEN=/path/to/doxygen.
set -euo pipefail
cd "$(dirname "$0")"
DOXYGEN=${DOXYGEN:-doxygen}
ROOT=../..
OOT=$ROOT/oot

# --- Stage the ROM hack sources: only what Audio Ocarina adds or changes ---
STAGE=staging/n64
rm -rf staging out
mkdir -p $STAGE/include $STAGE/src/code $STAGE/pages
cp $OOT/include/audio_ocarina_*.h $STAGE/include/
cp $OOT/src/code/audio_ocarina_*.c $STAGE/src/code/
cp pages/n64_main.md $STAGE/pages/
cp $ROOT/docs/architecture.md $STAGE/pages/
# The investigation report is a local working document, not part of the published repository
[ -f $ROOT/docs/investigation.md ] && cp $ROOT/docs/investigation.md $STAGE/pages/
# Doxygen misparses code spans in table rows that start with ' or hold <placeholders>: turn them into HTML in its copies
mkdir -p staging/plugin
for d in $STAGE/pages staging/plugin; do
    python3 -c '
import html, re, sys
for line in open(sys.argv[1], encoding="utf-8"):
    if line.startswith("|"):
        line = re.sub(r"`([^`]*)`", lambda m: "<code>" + html.escape(m.group(1), quote=False) + "</code>", line)
    sys.stdout.write(line)
' $ROOT/protocol/protocol.md > $d/protocol.md
done

python3 - "$OOT" "$STAGE" <<'PY'
import re, subprocess, sys
oot, stage = sys.argv[1], sys.argv[2]

# 1. Extract the top-level "#if AUDIO_OCARINA" blocks of z_message.c that define new code (the long ones);
#    the short hooks inside vanilla functions go to the hooks page as diffs.
lines = open(f"{oot}/src/code/z_message.c", encoding="utf-8").read().split("\n")
out, i = [], 0
while i < len(lines):
    if lines[i].strip() == "#if AUDIO_OCARINA":
        depth, j = 1, i + 1
        while depth:
            s = lines[j].strip()
            if s.startswith("#if"):
                depth += 1
            elif s.startswith("#endif"):
                depth -= 1
            j += 1
        if j - i > 30:
            out.append(f"// ---- src/code/z_message.c, lines {i + 1}-{j} ----")
            out += lines[i:j] + [""]
        i = j
    else:
        i += 1
header = """/**
 * @file z_message_audio_ocarina.c
 *
 * Extract of src/code/z_message.c: the Audio Ocarina UI code added to the message system (prompt and status
 * texts drawn with the game font, octave indicator and, in free play, the staff of accepted notes).
 * In the real source this block sits inside z_message.c, so it uses its static data (sFontWidths, ...).
 * The calls from vanilla functions are listed in the \\ref aoc_hooks page.
 */
"""
open(f"{stage}/src/code/z_message_audio_ocarina.c", "w", encoding="utf-8").write(header + "\n".join(out))

# 2. Hooks page: git diff of the modified vanilla files, without the block extracted above.
def git(*a):
    return subprocess.run(["git", "-C", oot, *a], check=True, capture_output=True, text=True).stdout
files = [f for f in git("diff", "--name-only").split() if f]
page = ["# Hooks in vanilla code {#aoc_hooks}", "",
        "Changes made to existing files of the decompilation (`git diff` against the upstream commit "
        f"`{git('rev-parse', '--short', 'HEAD').strip()}`). Every change is guarded by `#if AUDIO_OCARINA`, "
        "so a normal build still matches the original ROM.", ""]
for f in files:
    diff = git("diff", "-U3", "--", f)
    hunks = re.split(r"(?m)^(?=@@ )", diff)[1:]
    kept = [h for h in hunks if h.count("\n+") <= 60]
    skipped = len(hunks) - len(kept)
    added = sum(h.count("\n+") for h in hunks)
    page += [f"## {f}", "", f"{len(hunks)} hunk(s), {added} line(s) added."]
    if skipped:
        page += ["", "The large block of new functions is shown in z_message_audio_ocarina.c; "
                 "only the hooks are listed here."]
    page += ["", "```"] + [h.rstrip("\n") for h in kept] + ["```", ""]
open(f"{stage}/pages/hooks.md", "w", encoding="utf-8").write("\n".join(page))
PY

for set in plugin n64; do
    mkdir -p out/$set
    "$DOXYGEN" Doxyfile.$set
    mv warnings.log out/$set/warnings.log
    echo "$set: $(realpath out/$set/html/index.html)"
done
