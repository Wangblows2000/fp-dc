#!/bin/sh
# Build the native Linux (x86_64) PC version.
#   pc/build_linux.sh [outdir]          default outdir: pc/dist/linux
# Output: <outdir>/fpdc and <outdir>/README.txt. Needs gcc, make and the SDL2 development package
# (Debian / Ubuntu: apt-get install build-essential libsdl2-dev).
# Optional: DATA_DIR=<converted2> also copies the converted game data to <outdir>/data (the program's default data path).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUTDIR=${1:-$HERE/dist/linux}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

command -v sdl2-config >/dev/null 2>&1 || { echo "error: sdl2-config not found (install libsdl2-dev)" >&2; exit 1; }
make -C "$HERE" -j"$JOBS" PLATFORM=linux OUT=build/linux
mkdir -p "$OUTDIR"
cp "$HERE/build/linux/fpdc" "$OUTDIR/fpdc"
cp "$HERE/README.txt" "$OUTDIR/README.txt"
if [ -n "${DATA_DIR:-}" ]; then
    [ -f "$DATA_DIR/levels.txt" ] || { echo "error: $DATA_DIR has no levels.txt (not a converted data folder)" >&2; exit 1; }
    mkdir -p "$OUTDIR/data"
    cp -R "$DATA_DIR"/. "$OUTDIR/data/"
fi
echo "built $OUTDIR/fpdc"
