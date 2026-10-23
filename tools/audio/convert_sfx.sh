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
IDS="4 5 8 9 17 21 25 26 27 28 37 53 56 63 64 81 100 174 176 220 288 396 405"
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
