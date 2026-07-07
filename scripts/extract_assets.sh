#!/bin/sh
# extract_assets.sh — build the assets/ directory from YOUR OWN copy of the
# original game. No game data ships with this repository: you need the original
# Fire & Forget II (Titus, 1990) DOS files.
#
#   usage: scripts/extract_assets.sh /path/to/your/FF2/game/dir
#
# Expects in that directory: FF2EGA.EXE, *.CPT, HIGH.
# Produces: assets/dgroup.bin  (the initialized DGROUP data image,
#                               EXE file offset 0xEC10, 0xD7F0 bytes)
#           assets/font_ega.bin (the 8x8 EGA font block from the code segment,
#                               EXE file offset 0xA670, 0x2000 bytes)
#           assets/cpt/*.CPT    (the PowerPacker-compressed game screens/sprites)
#           assets/HIGH         (the high-score table file)
set -e
SRC="$1"
[ -n "$SRC" ] && [ -f "$SRC/FF2EGA.EXE" ] || {
    echo "usage: $0 /path/to/dir/containing/FF2EGA.EXE" >&2; exit 1; }

cd "$(dirname "$0")/.."
mkdir -p assets/cpt

dd if="$SRC/FF2EGA.EXE" of=assets/dgroup.bin   bs=1 skip=$((0xEC10)) count=$((0xD7F0)) 2>/dev/null
dd if="$SRC/FF2EGA.EXE" of=assets/font_ega.bin bs=1 skip=$((0xA670)) count=$((0x2000)) 2>/dev/null
cp "$SRC"/*.CPT assets/cpt/
[ -f "$SRC/HIGH" ] && cp "$SRC/HIGH" assets/HIGH || {
    # a missing HIGH file just means default high scores
    printf '' > assets/HIGH; }

echo "assets/ ready:"
ls -la assets/dgroup.bin assets/font_ega.bin assets/HIGH
ls assets/cpt
