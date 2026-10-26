#!/bin/sh
# Rebuild runtime data for one recovered level into <out_dir>/<name>/ (each level has its own
# palettes + Lilac bank because palettize.py packs per level). Shared sounds/music stay in <out_dir>.
# usage: tools/build_level.sh <frame(on_frame_N)> <name> <out_dir>
set -e
F=${1:-21}; N=${2:-dv1}; O=${3:-converted2}
mkdir -p "$O"
T="$O/$N.tmp"; rm -rf "$T" && mkdir -p "$T"
python3 tools/import_frame.py build/frames extracted/linux/images "$F" "$N" "$T"
# Carol's banks (session 10) get their own palette for bank 2, which replaces Lilac's when Carol plays (pal2c.pal).
# Overlays with <= 15 colours (Lilac's hair, Carol's tail) are 4 bpp blocks at the start of that group (session 11).
mkdir -p "$T/carol" && mv "$T"/carol*.tex "$T/carol/"
python3 tools/palettize.py "$T/carol" '^carol' --gid=2 '--pal4=0:^caroltail' && mv "$T/carol/pal2.pal" "$T/pal2c.pal"
# Milla (session 11): same scheme, pal2m.pal
mkdir -p "$T/milla" && mv "$T"/milla*.tex "$T/milla/"
python3 tools/palettize.py "$T/milla" '^milla' --gid=2 '--pal4=0:^millatail|^millafx' && mv "$T/milla/pal2.pal" "$T/pal2m.pal"
python3 tools/palettize.py "$T" "^${N}_p[0-9]" '_par[0-9]' '^lilac' '_obj' '--flex=_ens' '--flex=_shl' '--pal4=1:_p[xw]' '--pal4=2:^lilachair'
mv "$T/carol/"*.tex "$T/" && rmdir "$T/carol"
mv "$T/milla/"*.tex "$T/" && rmdir "$T/milla"
# parallax sets with more than 15 colours per image (left in ARGB1555 by palettize, Fortune Night): PAL8 in the free top of
# the character group 2 (pal2 / pal2c / pal2m); no-op for every other level (session 17)
python3 tools/par_top.py "$T" "^${N}_p[xw]"
rm -rf "$O/$N" && mv "$T" "$O/$N"
cp converted/sfx_*.wav "$O"/ 2>/dev/null || true
cp converted/stage1.pcm "$O"/ 2>/dev/null || true
# levels.txt: one built level dir per line (read by the FP_DEBUG_SELECT menu)
( [ -f "$O/levels.txt" ] && grep -v "^$N\$" "$O/levels.txt"; echo "$N" ) | sort -u > "$O/levels.tmp" && mv "$O/levels.tmp" "$O/levels.txt"
