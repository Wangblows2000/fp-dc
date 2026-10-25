#!/bin/bash
# Gallery sounds (session 15; spec_gallery 3.2 / 7): every file is loaded on demand by the gallery (src/game/fe_gallery.inc)
# and kept loose on the disc (data/gal/, not packed).
#   - the 19 voice clips of the sound cards (ListSongFilenames type 2), mono ADPCM at 11025 Hz -> <name>.wav
#     source: <voices_dir>/eng/<xx>/<name>.ogg, else <fallback_voices_dir>/eng/<xx>/<name>.ogg (the copies under
#     /home/claude/fprun are 1-second placeholders for the clips missing from the user's install)
#   - the samples played by the sound cards that have no file (RV t_gal6: 0 1 2 3 57 188 362) and the gallery's own
#     UI sound 443 (view / close art, close card), from the extracted Assets.dat audio -> s<id>.wav: 11025 Hz
#     (s3 is 5.1 s: snd_sfx_load takes <= 65534 samples), 443 at 22050 Hz like the other menu sounds
# usage: convert_gallery.sh <extracted_audio_dir> <voices_dir> <fallback_voices_dir> <out_dir>
set -e
SRC=$1; VOX=$2; VOX2=$3; OUT=$4
HERE=$(cd "$(dirname "$0")" && pwd)
[ -x "$HERE/wav2adpcm" ] || cc -O2 -o "$HERE/wav2adpcm" "${KOS_BASE:-/opt/toolchains/dc/kos}/utils/wav2adpcm/wav2adpcm.c"
mkdir -p "$OUT"; TMP=$(mktemp -d)
echo "name,source,duration_s,bytes" > "$OUT/gal_manifest.csv"
conv() {  # src dst rate
  ffmpeg -v error -y -i "$1" -ac 1 -ar "$3" -sample_fmt s16 "$TMP/x.wav"
  "$HERE/wav2adpcm" -t "$TMP/x.wav" "$2" > /dev/null
  echo "$(basename "$2" .wav),$1,$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$1"),$(stat -c %s "$2")" >> "$OUT/gal_manifest.csv"
}
for v in ca/carol_awesome ca/carol_feelmypower mi/milla_whimper mi/milla_shiny mi/milla_superdog se/serp_madlaugh \
         li/lilac_wontgiveup li/lilac_nottoday li/lilac_hitmeifyoucan li/lilac_snakeman da/da_04_01 da/da_04_02 \
         se/serp_gotyounow se/serp_bugeyed se/serp_kungfu se/serp_annihilation sx/sx_intrusion sx/sx_still sx/sx_08b02; do
  f="$VOX/eng/$v.ogg"; [ -f "$f" ] || f="$VOX2/eng/$v.ogg"
  conv "$f" "$OUT/$(basename $v).wav" 11025
done
for i in 0 1 2 3 57 188 362; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav" 11025; done
conv "$SRC/audio_443.ogg" "$OUT/s443.wav" 22050
rm -rf "$TMP"
awk -F, 'NR>1{s+=$4} END {printf "%d gallery sounds, %.1f KB\n", NR-1, s/1024}' "$OUT/gal_manifest.csv"
