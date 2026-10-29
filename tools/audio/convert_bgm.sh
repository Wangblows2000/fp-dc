#!/bin/sh
# Convert the original BGM (Ogg Vorbis) to AICA 4-bit ADPCM streams for the Dreamcast (D-013).
# usage: convert_bgm.sh <bgm_dir> <out_dir> [rate=32000]
# Output: <out_dir>/<track>.adp and <out_dir>/bgm_manifest.csv. The conversion is tools/audio/convert_audio.py (Python, so the
# drag-and-drop builder runs it on Windows too); this wrapper keeps the documented command.
exec python3 "$(dirname "$0")/convert_audio.py" bgm "$@"
