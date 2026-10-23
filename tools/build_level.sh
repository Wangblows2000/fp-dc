#!/bin/sh
# Rebuild runtime data for one recovered level into <out_dir>/<name>/ (each level has its own
# palettes + Lilac bank because palettize.py packs per level). Shared sounds/music stay in <out_dir>.
# usage: tools/build_level.sh <frame(on_frame_N)> <name> <out_dir>
set -e
F=${1:-21}; N=${2:-dv1}; O=${3:-converted2}
mkdir -p "$O"
T="$O/$N.tmp"; rm -rf "$T" && mkdir -p "$T"
python3 tools/import_frame.py build/frames extracted/linux/images "$F" "$N" "$T"
python3 tools/palettize.py "$T" "^${N}_p[0-9]" '_par[0-9]' '^lilac' '_obj' '--flex=_ens' '--pal4=1:_p[xw]'
rm -rf "$O/$N" && mv "$T" "$O/$N"
cp converted/sfx_*.wav "$O"/ 2>/dev/null || true
cp converted/stage1.pcm "$O"/ 2>/dev/null || true
# levels.txt: one built level dir per line (read by the FP_DEBUG_SELECT menu)
( [ -f "$O/levels.txt" ] && grep -v "^$N\$" "$O/levels.txt"; echo "$N" ) | sort -u > "$O/levels.tmp" && mv "$O/levels.tmp" "$O/levels.txt"
