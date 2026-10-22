#!/bin/sh
# Rebuild runtime data for one recovered level. usage: tools/build_level.sh <frame> <name> <out_dir>
set -e
F=${1:-21}; N=${2:-dv1}; O=${3:-converted2}
rm -rf "$O.tmp" && mkdir -p "$O.tmp"
python3 tools/import_frame.py build/frames extracted/linux/images "$F" "$N" "$O.tmp"
python3 tools/palettize.py "$O.tmp" "^${N}_p[0-9]" '_par[0-9]' '^lilac2' '_obj'
cp converted/sfx_*.wav "$O.tmp"/ 2>/dev/null || true
cp converted/stage1.pcm "$O.tmp"/ 2>/dev/null || true
rm -rf "$O" && mv "$O.tmp" "$O"
