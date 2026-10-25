#!/bin/bash
# Convert original sounds to Dreamcast sound-effect WAVs (AICA 4-bit ADPCM, mono, for snd_sfx_load).
# usage: convert_sfx.sh <extracted_audio_dir> <voices_dir> <out_dir> [rate=22050]
# Which sounds: the IDs the original plays (Media::play_id, traced: RUNTIME_MAP "Sound IDs") and Lilac's
# gameplay voices. Writes <out_dir>/sfx_manifest.csv.
set -e
SRC=$1; VOX=$2; OUT=$3; RATE=${4:-22050}
HERE=$(cd "$(dirname "$0")" && pwd)
# wav2adpcm is built locally from KallistiOS utils/wav2adpcm (not committed)
[ -x "$HERE/wav2adpcm" ] || cc -O2 -o "$HERE/wav2adpcm" "${KOS_BASE:-/opt/toolchains/dc/kos}/utils/wav2adpcm/wav2adpcm.c"
mkdir -p "$OUT"; TMP=$(mktemp -d)
IDS="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 21 23 24 25 26 27 28 29 30 31 32 33 34 35 37 38 39 40 41 42 43 45 46 47 48 49 50 51 53 56 57 58 61 63 64 74 76 78 80 81 95 98 100 113 114 115 116 117 118 119 120 121 122 126 128 133 139 140 141 154 159 160 161 162 174 176 220 221 232 288 289 290 307 311 313 314 315 319 320 387 396 405 409 410 411 412 413 414 170 178 179 180 181 182 183 184 185 186 202 203 204 205 206 207 244 415 416 417 418 419 420 421 422 201 55 62 69 99 172 173 240 242 251 252 278 306 308 316"
IDS="$IDS 75 111 260 262"   # Relic Maze acts 3-5 gimmicks (relic_gimb.c): pickaxe, light stone, hand-car, elevator
echo "name,source,duration_s,bytes" > "$OUT/sfx_manifest.csv"
conv() {  # src dst
  ffmpeg -v error -y -i "$1" -ac 1 -ar $RATE -sample_fmt s16 "$TMP/x.wav"
  "$HERE/wav2adpcm" -t "$TMP/x.wav" "$2" > /dev/null
  echo "$(basename "$2" .wav),$(basename "$1"),$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$1"),$(stat -c %s "$2")" >> "$OUT/sfx_manifest.csv"
}
for i in $IDS; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done
for v in "$VOX"/eng/li/lilac_*.ogg; do conv "$v" "$OUT/$(basename "$v" .ogg).wav"; done
rm -rf "$TMP"
awk -F, 'NR>1{s+=$4} END {printf "%d sounds, %.1f KB\n", NR-1, s/1024}' "$OUT/sfx_manifest.csv"
