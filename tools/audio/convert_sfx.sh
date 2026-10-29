#!/bin/sh
# Convert original sounds to Dreamcast sound-effect WAVs (AICA 4-bit ADPCM, mono, for snd_sfx_load).
# usage: convert_sfx.sh <extracted_audio_dir> <voices_dir> <out_dir> [rate=22050]
# The list of sounds (ids, voices, rates, trims; add a new stage's sounds there) is SFX in tools/audio/convert_audio.py
# (Python, so the drag-and-drop builder runs it on Windows too); this wrapper keeps the documented command.
# VOX_PC: the Windows install's Data/voices (the lines missing from the Linux copy).
exec python3 "$(dirname "$0")/convert_audio.py" sfx "$@"
