#!/bin/sh
# Developers: compile the native Linux (x86_64) PC program alone (no game data). To get a ready-to-play build, run
# ./build_linux.sh in the top folder instead (it needs no compiler and makes the data from your Assets.dat).
#   pc/compile_linux.sh [outdir]        default outdir: the current folder
# Output: <outdir>/fpdc and <outdir>/README-PC.txt. Needs gcc, make and the SDL2 development package
# (Debian / Ubuntu: apt-get install build-essential libsdl2-dev).
# Optional: DATA_DIR=<converted2> also copies the converted game data to <outdir>/data (the program's default data path).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUTDIR=${1:-$(pwd)}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

command -v sdl2-config >/dev/null 2>&1 || { echo "error: sdl2-config not found (install libsdl2-dev)" >&2; exit 1; }
make -C "$HERE" -j"$JOBS" PLATFORM=linux OUT=build/linux
mkdir -p "$OUTDIR"
cp "$HERE/build/linux/fpdc" "$OUTDIR/fpdc"
cp "$HERE/README.txt" "$OUTDIR/README-PC.txt"
if [ -n "${DATA_DIR:-}" ]; then
    [ -f "$DATA_DIR/levels.txt" ] || { echo "error: $DATA_DIR has no levels.txt (not a converted data folder)" >&2; exit 1; }
    mkdir -p "$OUTDIR/data"
    cp -R "$DATA_DIR"/. "$OUTDIR/data/"
fi
echo "built $OUTDIR/fpdc"
