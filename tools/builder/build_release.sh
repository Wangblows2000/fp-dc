#!/bin/bash
# Maintainers: build the prebuilt files of the drag-and-drop builder (committed in prebuilt/, D-119) and record them in
# tools/builder/downloads.json (sha256, size; fpdc.elf also src_tree = the git tree of src/ it was built from: fpbuild.py
# selftest fails when src/ has changed since, so a stale fpdc.elf cannot be committed unnoticed).
#   tools/builder/build_release.sh [--program-only] [out_dir=prebuilt]
#   --program-only: only fpdc.elf (after a change of src/); the tools below are pinned and rarely change
# Builds, from this checkout and pinned upstream sources:
#   fpdc.elf                 the Dreamcast program (KallistiOS: make, then sh-elf-strip)
#   wav2adpcm.exe            third_party/wav2adpcm (mingw-w64, static) / wav2adpcm-linux-x86_64 (gcc -static)
#   mkdcdisc.exe             mkdcdisc 2b98b0d9 (mingw-w64, its vendored libisofs, static) / mkdcdisc-linux-x86_64 (static,
#                            libisofs 1.5.6.pl01 without ACL / xattr); sources: mkdcdisc-2b98b0d9-src.tar.gz, libisofs tarball
#   ffmpeg-6.1.1-linux-x86_64  minimal static ffmpeg (LGPL: Ogg Vorbis / WAV in, 16-bit WAV out); source ffmpeg-6.1.1.tar.xz
# then SHA256SUMS, and rewrites the entries of these files in downloads.json (sha256, size). Commit every file of <out_dir>
# as an asset of the release named in downloads.json's release_url, then commit downloads.json.
# Needs: KallistiOS ($KOS_BASE or /opt/toolchains/dc/kos), mingw-w64 (posix threads) with libz-mingw-w64-dev and
# win-iconv-mingw-w64-dev, gcc, meson, ninja, nasm, git, curl. Host-only tool (FP_HOST_ENV).
set -eu
REPO=$(cd "$(dirname "$0")/../.." && pwd)
ONLY_PROGRAM=0; [ "${1:-}" = --program-only ] && { ONLY_PROGRAM=1; shift; }
OUT=$(mkdir -p "${1:-$REPO/prebuilt}" && cd "${1:-$REPO/prebuilt}" && pwd)
W=$REPO/build/release_work; mkdir -p "$W"
MKDCDISC_REV=2b98b0d9480e364e3517355648fbc3b49fb19882
FFMPEG_SHA=8684f4b00f94b85461884c3719382f1261f0d9eb3d59640a1f4ac0873616f968
ISOFS_SHA=ac1fd338d641744ca1fb1567917188b79bc8c2506832dd56885fec98656b9f25
J=$(nproc)
get() {  # url sha256 -> $W/<name>
  local f=$W/$(basename "$1"); [ -f "$f" ] || curl -fsSL -o "$f" "$1"
  [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$2" ] || { echo "bad checksum: $f"; exit 1; }
}

echo "== fpdc.elf (KallistiOS)"
KOS_ENV=${KOS_BASE:-/opt/toolchains/dc/kos}/environ.sh
rm -rf "$W/src" && mkdir "$W/src" && git -C "$REPO" archive HEAD | tar x -C "$W/src"
(cd "$W/src" && source "$KOS_ENV" && make -j"$J" > "$W/fpdc_make.log" 2>&1 && "$KOS_CC_BASE/bin/$KOS_CC_PREFIX-strip" -o "$OUT/fpdc.elf" fpdc.elf)
git -C "$REPO" rev-parse --short HEAD > "$W/fpdc_commit.txt"
git -C "$REPO" rev-parse HEAD:src > "$W/fpdc_src_tree.txt"
chmod 755 "$OUT/fpdc.elf"
if [ "$ONLY_PROGRAM" = 0 ]; then

echo "== wav2adpcm"
x86_64-w64-mingw32-gcc -O2 -static -s -o "$OUT/wav2adpcm.exe" "$REPO/third_party/wav2adpcm/wav2adpcm.c"
gcc -O2 -static -s -o "$OUT/wav2adpcm-linux-x86_64" "$REPO/third_party/wav2adpcm/wav2adpcm.c"

echo "== mkdcdisc"
[ -d "$W/mkdcdisc/.git" ] || git clone -q https://gitlab.com/simulant/mkdcdisc.git "$W/mkdcdisc"
git -C "$W/mkdcdisc" checkout -q "$MKDCDISC_REV"
git -C "$W/mkdcdisc" archive --prefix=mkdcdisc-${MKDCDISC_REV:0:8}/ -o "$OUT/mkdcdisc-${MKDCDISC_REV:0:8}-src.tar.gz" HEAD
cat > "$W/mingw64.ini" <<'EOF'
[binaries]
c = 'x86_64-w64-mingw32-gcc-posix'
cpp = 'x86_64-w64-mingw32-g++-posix'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
windres = 'x86_64-w64-mingw32-windres'
pkg-config = 'pkg-config'
[properties]
pkg_config_libdir = '/usr/x86_64-w64-mingw32/lib/pkgconfig'
sys_root = '/usr/x86_64-w64-mingw32'
[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
[built-in options]
c_link_args = ['-static', '-pthread']
cpp_link_args = ['-static', '-static-libgcc', '-static-libstdc++', '-pthread']
EOF
(cd "$W/mkdcdisc" && rm -rf bw && meson setup bw --cross-file "$W/mingw64.ini" -Dpng=disabled --buildtype=release > "$W/mkdcdisc_win.log" 2>&1 \
  && ninja -C bw mkdcdisc.exe >> "$W/mkdcdisc_win.log" 2>&1)
x86_64-w64-mingw32-strip -o "$OUT/mkdcdisc.exe" "$W/mkdcdisc/bw/mkdcdisc.exe"
get https://files.libburnia-project.org/releases/libisofs-1.5.6.pl01.tar.gz $ISOFS_SHA
cp "$W/libisofs-1.5.6.pl01.tar.gz" "$OUT/"
(cd "$W" && rm -rf libisofs-1.5.6 isofs && tar xzf libisofs-1.5.6.pl01.tar.gz && cd libisofs-1.5.6 \
  && ./configure --prefix="$W/isofs" --disable-shared --enable-static --disable-libacl --disable-xattr --disable-zlib --disable-libjte > "$W/isofs.log" 2>&1 \
  && make -j"$J" >> "$W/isofs.log" 2>&1 && make install >> "$W/isofs.log" 2>&1)
(cd "$W/mkdcdisc" && rm -rf bl && PKG_CONFIG_PATH="$W/isofs/lib/pkgconfig" meson setup bl -Dpng=disabled --buildtype=release \
  -Dc_link_args="-static -pthread" -Dcpp_link_args="-static -pthread" > "$W/mkdcdisc_lin.log" 2>&1 && ninja -C bl mkdcdisc >> "$W/mkdcdisc_lin.log" 2>&1)
strip -o "$OUT/mkdcdisc-linux-x86_64" "$W/mkdcdisc/bl/mkdcdisc"

echo "== ffmpeg (minimal, static)"
get https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.xz $FFMPEG_SHA
cp "$W/ffmpeg-6.1.1.tar.xz" "$OUT/"
(cd "$W" && rm -rf ffmpeg-6.1.1 && tar xf ffmpeg-6.1.1.tar.xz && cd ffmpeg-6.1.1 \
  && ./configure --disable-everything --disable-autodetect --disable-doc --disable-ffplay --disable-ffprobe \
     --disable-network --disable-debug --enable-static --disable-shared --extra-ldflags=-static \
     --disable-avdevice --disable-swscale --disable-postproc \
     --enable-protocol=file,pipe --enable-demuxer=ogg,wav --enable-parser=vorbis,opus,flac \
     --enable-decoder=vorbis,opus,flac,pcm_s16le,pcm_u8,pcm_s24le,pcm_s32le,pcm_f32le,pcm_mulaw,pcm_alaw,adpcm_ms,adpcm_ima_wav \
     --enable-muxer=wav --enable-encoder=pcm_s16le \
     --enable-filter=abuffer,abuffersink,aresample,areverse,silenceremove,aformat,anull,atrim,anullsrc > "$W/ffmpeg.log" 2>&1 \
  && make -j"$J" >> "$W/ffmpeg.log" 2>&1 && strip -o "$OUT/ffmpeg-6.1.1-linux-x86_64" ffmpeg)
chmod 755 "$OUT"/*-linux-x86_64 "$OUT/fpdc.elf"
fi

echo "== SHA256SUMS, downloads.json"
(cd "$OUT" && sha256sum $(ls | grep -v -E '^(SHA256SUMS|RELEASE_NOTES.md|README.md)$') > SHA256SUMS)
python3 - "$OUT" "$REPO/tools/builder/downloads.json" "$(cat "$W/fpdc_commit.txt")" "$(cat "$W/fpdc_src_tree.txt")" <<'EOF'
import hashlib, json, os, sys
out, table, commit, tree = sys.argv[1:5]
t = json.load(open(table))
for e in t['files']:
    p = os.path.join(out, e['name'])
    if 'url' in e or not os.path.isfile(p): continue
    e['sha256'] = hashlib.sha256(open(p, 'rb').read()).hexdigest(); e['size'] = os.path.getsize(p)
    if e['name'] == 'fpdc.elf': e['source'] = 'commit %s' % commit; e['src_tree'] = tree
    print('%-40s %s' % (e['name'], e['sha256']))
with open(table, 'w') as f: json.dump(t, f, indent=1); f.write('\n')
EOF
echo "done: $OUT (commit prebuilt/ and tools/builder/downloads.json together)"
