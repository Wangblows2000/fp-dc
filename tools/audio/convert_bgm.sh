#!/bin/bash
# Convert the original BGM (Ogg Vorbis) to AICA 4-bit ADPCM streams for the Dreamcast (D-013).
# usage: convert_bgm.sh <bgm_dir> <out_dir> [rate=32000]
# Output: <out_dir>/<track>.adp (headerless, byte-interleaved stereo for snd_stream_start_adpcm) and
#         <out_dir>/bgm_manifest.csv (track, src_bytes, src_rate, duration_s, adp_bytes, rate, loop).
# Loop: the original loops every track whole-file (SD: ChowdrenAudio has no loop points; RESEARCH_LOG).
set -e
SRC=$1; OUT=$2; RATE=${3:-32000}
HERE=$(cd "$(dirname "$0")" && pwd)
# wav2adpcm is built locally from KallistiOS utils/wav2adpcm (not committed)
[ -x "$HERE/wav2adpcm" ] || cc -O2 -o "$HERE/wav2adpcm" "${KOS_BASE:-/opt/toolchains/dc/kos}/utils/wav2adpcm/wav2adpcm.c"
mkdir -p "$OUT"; TMP=$(mktemp -d)
echo "track,src_bytes,src_rate,duration_s,adp_bytes,rate,loop" > "$OUT/bgm_manifest.csv"
for f in "$SRC"/*.ogg; do
  n=$(basename "$f" .ogg)
  ffmpeg -v error -y -i "$f" -ac 2 -ar $RATE -sample_fmt s16 "$TMP/$n.wav"
  "$HERE/wav2adpcm" -n -i -t "$TMP/$n.wav" "$OUT/$n.adp" > /dev/null
  python3 "$HERE/adpcm_loop_seal.py" "$OUT/$n.adp" > /dev/null   # clean whole-file loop seam (D-033)
  rm -f "$TMP/$n.wav"
  read sr dur < <(ffprobe -v error -show_entries stream=sample_rate:format=duration -of csv=p=0 "$f" | tr "\n" " "; echo) || true
  echo "$n,$(stat -c %s "$f"),$sr,$dur,$(stat -c %s "$OUT/$n.adp"),$RATE,whole" >> "$OUT/bgm_manifest.csv"
done
rmdir "$TMP"
awk -F, 'NR>1{s+=$5; d+=$4} END {printf "%d tracks, %.1f MB ADPCM, %.0f s\n", NR-1, s/1e6, d}' "$OUT/bgm_manifest.csv"
