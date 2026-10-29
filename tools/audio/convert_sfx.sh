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
IDS="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 21 23 24 25 26 27 28 29 30 31 32 33 34 35 37 38 39 40 41 42 43 45 46 47 48 49 50 51 53 56 57 58 61 63 64 74 76 78 80 81 95 98 100 113 114 115 116 117 118 119 120 121 122 126 128 133 139 140 141 154 159 160 161 162 174 176 220 221 232 288 289 290 307 311 313 314 315 319 320 387 396 405 409 410 411 412 413 414 170 178 179 180 181 182 183 184 185 186 202 203 204 205 206 207 244 415 416 417 418 419 420 421 422 201 55 62 69 99 172 173 240 242 251 252 278 306 308 316 54 59 60 67 70 77 101 110 135 213 219 426"
IDS="$IDS 75 111 260 262"   # Relic Maze acts 3-5 gimmicks (relic_gimb.c): pickaxe, light stone, hand-car, elevator
IDS="$IDS 68 73 96 99 149 312"   # RM3 Claw sequence (rm_claw.c)
IDS="$IDS 111 132 134 146 148 149 151 152"   # RM5 Kingdom Stone / Mantalith (rm_mantalith.c)
echo "name,source,duration_s,bytes" > "$OUT/sfx_manifest.csv"
# Fortune Night stage groups (session 17, D-087): tails below -48 dB are cut (about 30 % of sound RAM); never for looped ids
# (62, 260, 262) or anything outside the FN groups
TRIM=" 70 75 99 127 241 288 134 137 143 163 164 165 166 167 168 169 171 187 188 191 192 193 194 195 196 197 198 199 200 208 212 216 217 218 219 223 224 225 226 227 228 229 230 231 243 249 254 256 257 258 259 261 317 371 428 430 436 103 156 237 255 264 263 265 266 267 268 269 270 271 272 273 274 438 446 71 155 445 "
conv() {  # src dst
  local AF=""; [ -n "$TRIM_ALL" ] && AF="-af areverse,silenceremove=start_periods=1:start_threshold=-48dB,areverse"
  case "$TRIM" in *" $(basename "$2" .wav | sed 's/^s//') "*) AF="-af areverse,silenceremove=start_periods=1:start_threshold=-48dB,areverse";; esac
  ffmpeg -v error -y -i "$1" $AF -ac 1 -ar $RATE -sample_fmt s16 "$TMP/x.wav"
  "$HERE/wav2adpcm" -t "$TMP/x.wav" "$2" > /dev/null
  echo "$(basename "$2" .wav),$(basename "$1"),$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$1"),$(stat -c %s "$2")" >> "$OUT/sfx_manifest.csv"
}
# Relic Maze stage set (D-047 stage sounds): converted at half rate so the set fits sound RAM next to Milla's group and the
# music stream (session 14: 380 KB at 22 kHz did not leave the stream's 64 KB buffer; snd_sfx_load also needs <= 65534 samples)
LOW=" 54 55 59 60 62 67 68 70 73 75 77 96 99 101 110 111 135 149 213 219 260 262 306 312 426 "
LOW="$LOW 132 134 146 148 151 152 "   # RM5 Mantalith (rm_mantalith.c)
for i in $IDS; do case "$LOW" in *" $i "*) R0=$RATE; RATE=11025; conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; RATE=$R0;; *) conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav";; esac; done
for v in "$VOX"/eng/li/lilac_*.ogg; do conv "$v" "$OUT/$(basename "$v" .ogg).wav"; done
# Carol / Milla stage-clear voices (RM5 results 6590-6592; game.c voice_play_results loads them only for the results), half rate
R0=$RATE; RATE=11025; for v in ca/carol_feelmypower mi/milla_yay mi/milla_superdog; do conv "$VOX/eng/$v.ogg" "$OUT/$(basename $v).wav"; done; RATE=$R0
# Fortune Night (session 17): the stage group SFX_FN holds the flow ids 61 (a Carol id, needed by everyone at FN3's exit) and
# 134 (already in IDS / LOW above); the FN6 results group voice (8545, everyone) loads on demand, half rate like the others
# Fortune Night bosses (fn_boss.c): UFO 168 / 169, bulldozer 165 / 166, Serpentine 191-200 / 317 (62 / 70 / 75 are RM ids), half rate
for i in 165 166 168 169 191 192 193 194 195 196 197 198 199 200 317; do R0=$RATE; RATE=11025; conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; RATE=$R0; done
# Fortune Night enemies / gimmicks (fn_enemy.c, fn_gima.c, fn_gimb.c): per-act groups SFX_FNA / SFX_FNB in levels.c, half rate
for i in 127 143 163 164 167 171 187 188 208 212 216 217 218 223 224 225 226 227 228 229 230 231 243 249 371 428 430; do R0=$RATE; RATE=11025; conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; RATE=$R0; done
# Fortune Night Panther (fn_panther.c, SFX_FN6; 165-200 above, 99 / 126 / 219 in IDS): assist strike 137, back cannon 254,
# laser 256 / 257, voices 258 / 259, defeat 261, plasma 436, half rate; session 18 (fn_assist.c): 241 = Milla drops the box
for i in 137 241 254 256 257 258 259 261 436; do R0=$RATE; RATE=11025; conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; RATE=$R0; done
R0=$RATE; RATE=11025; conv "$VOX/eng/group/teamlilac_madeit.ogg" "$OUT/teamlilac_madeit.wav"; RATE=$R0
# Sky Battalion (session 19, per-act groups SFX_SB1..4 in levels.c; ids played by the frame 35-38 events and not converted
# above): SB1 267-274 (events 9044-9099, Kujacker group), SB2 103 156 237 263 (flamethrower) 264 446, SB3 265, SB4 255 266 438; half rate, trimmed;
# SB2 Spade (sb_spade.c: card throw 71, card burst 445, explosion 155)
for i in 103 156 237 255 263 264 265 266 267 268 269 270 271 272 273 274 438 446 71 155 445; do R0=$RATE; RATE=11025; conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; RATE=$R0; done
# Sky Battalion boss voices (session 21, level_def_t.vox: loaded with the stage group, character lines only for that
# character): Kujacker 9012-9022 / 9163 (da_04_01-04, carol_fatchance, milla_icantiwont; lilac_never is converted above),
# Spade 9625-9628 (spade_comeon / goodenough / comeon2); Jade Creek 5's Serpentine (serp_gotyounow 11197, serp_kungfu). Real files from the user's PC install; half rate, tails trimmed.
R0=$RATE; RATE=11025; TRIM_ALL=1
for v in da/da_04_01 da/da_04_02 da/da_04_03 da/da_04_04 ca/carol_fatchance mi/milla_icantiwont sp/spade_comeon sp/spade_goodenough sp/spade_comeon2 li/lilac_never li/lilac_hitmeifyoucan se/serp_gotyounow se/serp_kungfu; do conv "$VOX/eng/$v.ogg" "$OUT/$(basename $v).wav"; done
RATE=$R0; TRIM_ALL=
# Jade Creek (session 21, stage groups SFX_JC1..5 in levels.c): ids the JC modules play that were not converted above:
# 233 (enemies / act 5), 275 (BullFrag volley), 72 (bubble spring bounce), 292 (act-5 storm bolt), 427 (act 5), 276 (JC1 Hovercop
# fly-by), 277 (Hovercop), 296-299 (Neera), 335 (Trap Hideout bike ninja skid), 336 (Laser Iris), 363 (TH3 crowd); half rate, trimmed
R0=$RATE; RATE=11025; TRIM_ALL=1; for i in 233 275 72 292 427 276 277 296 297 298 299 335 336 363; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; RATE=$R0; TRIM_ALL=
# Thermal Base (session 21c, SFX_TB1..6): 279 / 280 doors, 282-286 gimmicks (pillars, terminal, card), 302 lava fall, 321 presser, 437 / 52 /
# 303-305 / 309 / 310 enemies and bosses, 92 the TB6 end; Syntax voice sx_still (TB6; the TB3 spider's sx_lost is only a silent stub in
# the Linux data and not in the user's voice files, so it is not converted); half rate, trimmed
R0=$RATE; RATE=11025; TRIM_ALL=1; for i in 52 92 279 280 282 283 284 285 286 302 303 304 305 309 310 321 437; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done
for v in sx/sx_still; do conv "$VOX/eng/$v.ogg" "$OUT/$(basename $v).wav"; done; RATE=$R0; TRIM_ALL=
# Pangu Lagoon (session 23, SFX_PL1..6): 44 sunflora spray, 147 terreon jump, 150 / 322-329 gimmicks (time gate, super boost, bow,
# breakout, sword), 327 obelisk laser, 330-334 the Holo-Dragon; half rate, tails trimmed
R0=$RATE; RATE=11025; TRIM_ALL=1; for i in 44 147 150 322 323 324 325 326 327 328 329 330 331 332 333 334; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; RATE=$R0; TRIM_ALL=
# the end of the game (session 24, src/game/ending.c END_SFX): 425 rumble and 294 hero-ship engine (looped: tails kept), 435 lift-off
# jets and 354 hull pieces (tails trimmed); 284 (Torque's rocket) is converted above. Half rate.
R0=$RATE; RATE=11025; for i in 425 294; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; TRIM_ALL=1; for i in 435 354; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; RATE=$R0; TRIM_ALL=
# Battle Glacier (session 24, SFX_BG1..6): 344 magnet launcher, 345 / 346 Elite spawner drop / land; half rate, tails trimmed
R0=$RATE; RATE=11025; TRIM_ALL=1; for i in 344 345 346; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; RATE=$R0; TRIM_ALL=
# Final Dreadnought (session 24, SFX_FD1A..FD4B): 376 gatling-gun burst; gimmicks 358-360 (pipes), 368-370 / 372 (fd2b), 379, 388; 381 (looped: tail kept) (the other FD ids are converted above); half rate, tails trimmed
R0=$RATE; RATE=11025; TRIM_ALL=1; for i in 376 358 359 360 368 369 370 372 379 388; do conv "$SRC/audio_$i.ogg" "$OUT/s$i.wav"; done; TRIM_ALL=; conv "$SRC/audio_381.ogg" "$OUT/s381.wav"; RATE=$R0
rm -rf "$TMP"
awk -F, 'NR>1{s+=$4} END {printf "%d sounds, %.1f KB\n", NR-1, s/1024}' "$OUT/sfx_manifest.csv"
