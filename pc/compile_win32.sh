#!/bin/sh
# Developers: compile the Windows PC program alone (fpdc.exe + SDL2.dll, no game data) with mingw-w64: on Linux, or on
# Windows in MSYS2 (pc\compile_win32.bat runs this there). To get a ready-to-play build, drag Assets.dat onto
# build_win32.bat / build_linux.sh in the top folder instead (they need no compiler).
#   pc/compile_win32.sh [outdir]        default outdir: $FPC_OUTDIR (set by compile_win32.bat), else the current folder
# Output: <outdir>/fpdc.exe, SDL2.dll, SDL2-LICENSE.txt, README-PC.txt.
# The compiler, first found: $WIN_CC; i686-w64-mingw32-gcc; gcc when it targets i686-w64-mingw32 (MSYS2 MINGW32);
# /mingw32/bin/gcc; then the 64-bit ones (x86_64-w64-mingw32-gcc, gcc targeting x86_64-w64-mingw32, /mingw64/bin/gcc,
# /ucrt64/bin/gcc), which give a 64-bit fpdc.exe (the released build is the 32-bit one). Also needs make (or
# mingw32-make), curl (or wget) and tar. Linux: apt-get install mingw-w64 make curl.
# The official SDL2 mingw development package (fixed release, checksum verified) is downloaded once into pc/build/deps
# (never committed). SDL2_DEVEL=<SDL2-x.y.z dir> uses an already unpacked package instead.
# Optional: DATA_DIR=<converted data dir> also copies the converted game data to <outdir>/data.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OUTDIR=${1:-${FPC_OUTDIR:-$(pwd)}}
# a Windows path (C:\...) from compile_win32.bat: to the MSYS2 form
case "$OUTDIR" in [A-Za-z]:\\*|[A-Za-z]:/*) command -v cygpath >/dev/null 2>&1 && OUTDIR=$(cygpath -u "$OUTDIR") ;; esac
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
SDL_VER=2.30.11
SDL_SHA256=0590db0a47b564aab92499ed03eaa75db9aee17c371080caa3b05df9d49d9ff9
SDL_URL=https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz

machine() { "$1" -dumpmachine 2>/dev/null || true; }
pick() {   # pick <arch>: the first compiler for <arch>-w64-mingw32
    if command -v "$1-w64-mingw32-gcc" >/dev/null 2>&1; then echo "$1-w64-mingw32-gcc"; return; fi
    if command -v gcc >/dev/null 2>&1 && [ "$(machine gcc)" = "$1-w64-mingw32" ]; then echo gcc; return; fi
    for g in /mingw32/bin/gcc /mingw64/bin/gcc /ucrt64/bin/gcc; do
        [ -x "$g" ] || [ -x "$g.exe" ] || continue
        case "$(machine "$g")" in "$1"-w64-mingw32) echo "$g"; return ;; esac
    done
}
if [ -z "${WIN_CC:-}" ]; then
    WIN_CC=$(pick i686)
    [ -n "$WIN_CC" ] || WIN_CC=$(pick x86_64)
fi
if [ -z "$WIN_CC" ]; then
    echo "error: no mingw-w64 gcc found (looked for i686-w64-mingw32-gcc, x86_64-w64-mingw32-gcc, and gcc targeting" >&2
    echo "  *-w64-mingw32; the gcc on the PATH targets '$(machine gcc)')." >&2
    echo "  MSYS2: run  pacman -S --needed make tar curl mingw-w64-i686-gcc  in an MSYS2 shell. Linux: apt-get install mingw-w64." >&2
    echo "  To just play: drag Assets.dat onto build_win32.bat in the top folder (no compiler needed)." >&2
    exit 1
fi
ARCH=$(machine "$WIN_CC"); ARCH=${ARCH%%-*}
case "$ARCH" in i686|x86_64) ;; *) echo "error: $WIN_CC targets '$(machine "$WIN_CC")', not i686 / x86_64-w64-mingw32" >&2; exit 1 ;; esac
MAKE=make; command -v make >/dev/null 2>&1 || MAKE=mingw32-make
command -v "$MAKE" >/dev/null 2>&1 || { echo "error: make not found (MSYS2: pacman -S make)" >&2; exit 1; }
echo "compiler: $WIN_CC ($ARCH)"

if [ -z "${SDL2_DEVEL:-}" ]; then
    DEPS=$HERE/build/deps
    SDL2_DEVEL=$DEPS/SDL2-$SDL_VER
    if [ ! -f "$SDL2_DEVEL/$ARCH-w64-mingw32/bin/SDL2.dll" ]; then
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
        tar xzf "$TGZ" -C "$DEPS" "SDL2-$SDL_VER/$ARCH-w64-mingw32" "SDL2-$SDL_VER/LICENSE.txt"
    fi
fi
SDL_DIR=$SDL2_DEVEL/$ARCH-w64-mingw32
[ -f "$SDL_DIR/include/SDL2/SDL.h" ] || { echo "error: no SDL2 $ARCH package in $SDL2_DEVEL" >&2; exit 1; }

"$MAKE" -C "$HERE" -j"$JOBS" PLATFORM=win32 OUT="build/win32-$ARCH" WIN_CC="$WIN_CC" SDL2_DIR="$SDL_DIR"
mkdir -p "$OUTDIR"
cp "$HERE/build/win32-$ARCH/fpdc.exe" "$OUTDIR/fpdc.exe"
cp "$SDL_DIR/bin/SDL2.dll" "$OUTDIR/SDL2.dll"
[ -f "$SDL2_DEVEL/LICENSE.txt" ] && cp "$SDL2_DEVEL/LICENSE.txt" "$OUTDIR/SDL2-LICENSE.txt"
cp "$HERE/README.txt" "$OUTDIR/README-PC.txt"
if [ -n "${DATA_DIR:-}" ]; then
    [ -f "$DATA_DIR/levels.txt" ] || { echo "error: $DATA_DIR has no levels.txt (not a converted data folder)" >&2; exit 1; }
    mkdir -p "$OUTDIR/data"
    cp -R "$DATA_DIR"/. "$OUTDIR/data/"
fi
echo "built $OUTDIR/fpdc.exe ($ARCH, + SDL2.dll $SDL_VER). It needs a data folder next to it: drag Assets.dat onto"
echo "build_win32.bat in the top folder for a complete build."
