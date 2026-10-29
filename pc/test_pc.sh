#!/bin/sh
# Smoke test of a PC build against the host harness: the classic route (title -> START GAME -> file -> CLASSIC ->
# Lilac -> Dragon Valley 1, then running right) for 1300 frames; every 20th frame of the PC build (--shot-dir,
# --host-draw: drawn only on dumped frames, as the host) is compared with the host's PPM dump of the same frame.
#   pc/test_pc.sh <fpdc | fpdc.exe> <data_dir> [workdir]
# fpdc.exe runs through wine (WINEPREFIX as set in the environment). Without DISPLAY it uses xvfb-run.
# Needs python3 and host/fpdc_host (built if missing). Exit 0 when every frame is identical or nearly so
# (< 1 % of the pixels: the i686 build's msvcrt sinf / cosf move a few edge pixels of rotated title sprites).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=$1; DATA=$2
WORK=${3:-$HERE/build/test}
case "$BIN" in /*) ;; *) BIN=$(pwd)/$BIN ;; esac
case "$DATA" in /*) ;; *) DATA=$(pwd)/$DATA ;; esac
[ -f "$DATA/levels.txt" ] || { echo "error: $DATA is not a converted data folder" >&2; exit 1; }
[ -x "$HERE/../host/fpdc_host" ] || make -C "$HERE/../host"
mkdir -p "$WORK"
rm -rf "$WORK/host" "$WORK/pc"; mkdir -p "$WORK/host" "$WORK/pc"
rm -f "$WORK/test.sav"
cat > "$WORK/route.txt" <<'EOF'
40 42 S
200 201 J
230 231 J
260 261 D
280 281 J
380 381 J
560 900 R
620 625 J
700 705 J
800 805 J
900 960 X
960 1300 R
1000 1010 J
1100 1110 J
EOF
"$HERE/../host/fpdc_host" "$DATA" 1300 "$WORK/route.txt" "$WORK/host" 20 > "$WORK/host.log" 2>&1
WRAP=""
case "$BIN" in *.exe) WRAP=wine; export WINEDEBUG=${WINEDEBUG:--all} ;; esac
run() { if [ -n "${DISPLAY:-}" ]; then "$@"; else xvfb-run -a -s "-screen 0 1280x720x24" "$@"; fi; }
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
run $WRAP "$BIN" "$DATA" --fast --frames 1300 --script "$WORK/route.txt" \
    --shot-dir "$WORK/pc" --shot-every 20 --host-draw --save "$WORK/test.sav" > "$WORK/pc.log" 2>&1 || { echo "error: the PC build failed (see $WORK/pc.log)" >&2; exit 1; }
grep "audio:\|video:\|perf:" "$WORK/pc.log" || true
python3 - "$WORK" <<'EOF'
import os, struct, sys
work = sys.argv[1]
def ppm(p):
    d = open(p, 'rb').read(); parts = d.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2]); px = parts[4]
    return w, h, [tuple(px[i:i + 3]) for i in range(0, w * h * 3, 3)]
def bmp(p):
    d = open(p, 'rb').read(); off, = struct.unpack_from('<I', d, 10)
    w, h, _, bpp = struct.unpack_from('<iiHH', d, 18); bp = bpp // 8; stride = (w * bp + 3) & ~3
    rows = []
    for y in range(abs(h)):
        r = d[off + y * stride: off + y * stride + w * bp]
        rows.append([(r[i + 2], r[i + 1], r[i]) for i in range(0, w * bp, bp)])
    if h > 0: rows.reverse()
    return w, abs(h), [c for r in rows for c in r]
same = near = diff = missing = 0; bad = []
for f in sorted(os.listdir(os.path.join(work, 'host'))):
    q = os.path.join(work, 'pc', f[:-4] + '.bmp')
    if not os.path.exists(q): missing += 1; bad.append(f[:-4] + ' missing'); continue
    a = ppm(os.path.join(work, 'host', f)); b = bmp(q)
    n = sum(1 for x, y in zip(a[2], b[2]) if x != y) if a[:2] == b[:2] else a[0] * a[1]
    if n == 0: same += 1
    elif n < a[0] * a[1] // 100: near += 1; bad.append('%s ~%d px' % (f[:-4], n))
    else: diff += 1; bad.append('%s %d px' % (f[:-4], n))
print('frames: %d identical, %d nearly (<1%% px), %d different, %d missing  %s' % (same, near, diff, missing, ' '.join(bad[:12])))
sys.exit(1 if diff or missing or same == 0 else 0)
EOF
