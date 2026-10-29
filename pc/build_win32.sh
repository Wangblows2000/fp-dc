#!/bin/sh
# Cross-compile the 32-bit Windows (i686) PC version with mingw-w64 (on Linux; MSYS2 / Cygwin shells work too).
#   pc/build_win32.sh [outdir]          default outdir: pc/dist/win32
# Output: <outdir>/fpdc.exe, <outdir>/SDL2.dll, <outdir>/README.txt.
# Needs make, curl (or wget), tar and the i686 mingw-w64 compiler (Debian / Ubuntu: apt-get install mingw-w64).
# The official SDL2 mingw development package (fixed release, checksum verified) is downloaded once into
# pc/build/deps (never committed). SDL2_DEVEL=<SDL2-x.y.z dir> uses an already unpacked package instead.
# Optional: DATA_DIR=<converted2> also copies the converted game data to <outdir>/data.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUTDIR=${1:-$HERE/dist/win32}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
SDL_VER=2.30.11
SDL_SHA256=0590db0a47b564aab92499ed03eaa75db9aee17c371080caa3b05df9d49d9ff9
SDL_URL=https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz

command -v i686-w64-mingw32-gcc >/dev/null 2>&1 || { echo "error: i686-w64-mingw32-gcc not found (install mingw-w64)" >&2; exit 1; }

if [ -z "${SDL2_DEVEL:-}" ]; then
    DEPS=$HERE/build/deps
    SDL2_DEVEL=$DEPS/SDL2-$SDL_VER
    if [ ! -f "$SDL2_DEVEL/i686-w64-mingw32/bin/SDL2.dll" ]; then
        mkdir -p "$DEPS"
        TGZ=$DEPS/SDL2-devel-$SDL_VER-mingw.tar.gz
        if [ ! -f "$TGZ" ]; then
            echo "downloading $SDL_URL"
            if command -v curl >/dev/null 2>&1; then curl -fsSL -o "$TGZ.part" "$SDL_URL"
            else wget -q -O "$TGZ.part" "$SDL_URL"; fi
            mv "$TGZ.part" "$TGZ"
        fi
        SUM=$( (sha256sum "$TGZ" 2>/dev/null || shasum -a 256 "$TGZ") | cut -d' ' -f1)
        if [ "$SUM" != "$SDL_SHA256" ]; then
            echo "error: checksum mismatch for $TGZ ($SUM); delete it and retry" >&2; exit 1
        fi
        tar xzf "$TGZ" -C "$DEPS" "SDL2-$SDL_VER/i686-w64-mingw32" "SDL2-$SDL_VER/LICENSE.txt"
    fi
fi
SDL_I686=$SDL2_DEVEL/i686-w64-mingw32
[ -f "$SDL_I686/include/SDL2/SDL.h" ] || { echo "error: no SDL2 i686 package in $SDL2_DEVEL" >&2; exit 1; }

make -C "$HERE" -j"$JOBS" PLATFORM=win32 OUT=build/win32 SDL2_DIR="$SDL_I686"
mkdir -p "$OUTDIR"
cp "$HERE/build/win32/fpdc.exe" "$OUTDIR/fpdc.exe"
cp "$SDL_I686/bin/SDL2.dll" "$OUTDIR/SDL2.dll"
[ -f "$SDL2_DEVEL/LICENSE.txt" ] && cp "$SDL2_DEVEL/LICENSE.txt" "$OUTDIR/SDL2-LICENSE.txt"
cp "$HERE/README.txt" "$OUTDIR/README.txt"
if [ -n "${DATA_DIR:-}" ]; then
    [ -f "$DATA_DIR/levels.txt" ] || { echo "error: $DATA_DIR has no levels.txt (not a converted data folder)" >&2; exit 1; }
    mkdir -p "$OUTDIR/data"
    cp -R "$DATA_DIR"/. "$OUTDIR/data/"
fi
echo "built $OUTDIR/fpdc.exe (+ SDL2.dll $SDL_VER)"
