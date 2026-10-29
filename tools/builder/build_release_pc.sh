#!/bin/bash
# Maintainers: build the PC versions' files of the builder's release and record them in tools/builder/downloads_pc.json.
#   tools/builder/build_release_pc.sh [out_dir=build/builder_release]
#   fpdc-win32.exe, SDL2.dll, SDL2-LICENSE.txt   pc/build_win32.sh (mingw-w64 i686; the official SDL 2.30.11 mingw package),
#                                                the program stripped
#   fpdc-linux-x86_64                            pc/Makefile with SDL 2.30.11 built from source as a static library (its X11,
#                                                Wayland, ALSA, PulseAudio back ends are loaded at run time), compiled with
#                                                zig cc for glibc 2.28 (runs on any Linux x86_64 with glibc 2.27+), stripped
# then rewrites the entries of these files in downloads_pc.json (sha256, size) and SHA256SUMS. Publish the files with the
# rest of the release (tools/builder/build_release.sh), commit downloads_pc.json.
# Needs: mingw-w64 (i686), make, curl, python3 with pip (zig is installed into build/release_work by pip: ziglang 0.13.0),
# the X11 / Wayland / ALSA / PulseAudio development headers (Debian / Ubuntu: libx11-dev libxext-dev libwayland-dev
# libxkbcommon-dev libasound2-dev libpulse-dev) so that SDL has those back ends. Host-only tool (FP_HOST_ENV).
set -eu
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(mkdir -p "${1:-$REPO/build/builder_release}" && cd "${1:-$REPO/build/builder_release}" && pwd)
W=$REPO/build/release_work; mkdir -p "$W"
SDL_VER=2.30.11
SDL_SHA=8b8d4aef2038533da814965220f88f77d60dfa0f32685f80ead65e501337da7f
J=$(nproc)

echo "== Windows (i686)"
sh "$REPO/pc/build_win32.sh" "$W/win32" > "$W/pc_win32.log" 2>&1
i686-w64-mingw32-strip -o "$OUT/fpdc-win32.exe" "$W/win32/fpdc.exe"
cp "$W/win32/SDL2.dll" "$OUT/SDL2.dll"; cp "$W/win32/SDL2-LICENSE.txt" "$OUT/SDL2-LICENSE.txt"

echo "== Linux (x86_64, static SDL2, glibc 2.28 target)"
[ -d "$W/zig/ziglang" ] || python3 -m pip install -q --target "$W/zig" ziglang==0.13.0
mkdir -p "$W/zbin"
for t in cc ar ranlib; do
  printf '#!/bin/sh\nexec env PYTHONPATH="%s" python3 -m ziglang %s %s "$@"\n' "$W/zig" "$t" "$([ $t = cc ] && echo '-target x86_64-linux-gnu.2.28')" > "$W/zbin/z$t"
  chmod +x "$W/zbin/z$t"
done
F=$W/SDL2-$SDL_VER.tar.gz
[ -f "$F" ] || curl -fsSL -o "$F" https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz
[ "$(sha256sum "$F" | cut -d' ' -f1)" = $SDL_SHA ] || { echo "bad checksum: $F"; exit 1; }
rm -rf "$W/SDL2-$SDL_VER" "$W/sdl2z" && tar xzf "$F" -C "$W"
(cd "$W/SDL2-$SDL_VER" && CC="$W/zbin/zcc" AR="$W/zbin/zar" RANLIB="$W/zbin/zranlib" \
   CFLAGS="-O2 -idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu" LDFLAGS="-L/usr/lib/x86_64-linux-gnu" \
   ./configure --prefix="$W/sdl2z" --disable-shared --enable-static > "$W/sdl2z.log" 2>&1 \
   && make -j"$J" >> "$W/sdl2z.log" 2>&1 && make install >> "$W/sdl2z.log" 2>&1)
grep -E "Audio drivers|Video drivers" "$W/sdl2z.log"
rm -rf "$REPO/pc/build/linux-release"
make -C "$REPO/pc" -j"$J" PLATFORM=linux OUT=build/linux-release CC="$W/zbin/zcc" SDL2_CONFIG="$W/sdl2z/bin/sdl2-config" \
     LIBS="$("$W/sdl2z/bin/sdl2-config" --static-libs) -lm" > "$W/pc_linux.log" 2>&1
strip -o "$OUT/fpdc-linux-x86_64" "$REPO/pc/build/linux-release/fpdc"; chmod 755 "$OUT/fpdc-linux-x86_64"
objdump -T "$OUT/fpdc-linux-x86_64" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1

echo "== downloads_pc.json, SHA256SUMS"
python3 - "$OUT" "$REPO/tools/builder/downloads_pc.json" <<'EOF'
import hashlib, json, os, sys
out, table = sys.argv[1:3]
t = json.load(open(table))
for e in t['files']:
    p = os.path.join(out, e['name'])
    e['sha256'] = hashlib.sha256(open(p, 'rb').read()).hexdigest(); e['size'] = os.path.getsize(p)
    print('%-24s %s' % (e['name'], e['sha256']))
with open(table, 'w') as f: json.dump(t, f, indent=1); f.write('\n')
EOF
(cd "$OUT" && sha256sum $(ls | grep -v -E '^(SHA256SUMS|RELEASE_NOTES.md)$') > SHA256SUMS)
echo "done: $OUT"
