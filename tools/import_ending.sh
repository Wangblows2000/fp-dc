#!/bin/bash
# The end-of-game scenes (session 24, src/game/ending.c): the space ending (frame index 83 = frame_84.json) and the credits
# (frame index 85 = frame_86.json) as UI scenes, plus the credits text (tools/import_ending.py).
# usage: tools/import_ending.sh <data_dir>    (e.g. converted2; writes <data_dir>/end/: end_*, cred_*, credits.txt; its own disc pack end.pak, so the front end's ui.pak stays as it was)
# Only the animations the port draws are exported (-k): the cast stands in its first pose, the played character waves at the
# end (anim 37 direction 1, RE 22777 / 22782-22784); the full-screen shockwave frames (426x240 x 18) and the black Fade
# objects are drawn as rectangles instead (ending.c).
set -e
D=${1:-converted2}
cd "$(dirname "$0")/.."
export PYTHONDONTWRITEBYTECODE=1
# frame index 83: space, the Dreadnought, the lift-off jets, the hero ship, Brevon's mech, debris, dust, the explosions it creates
python3 tools/import_ui.py build/frames extracted/linux/images "$D/end" end 84 84+effectsexplosion06_435 \
  -x '^(shockwave|levelcardfade|ringsparkle|separatormasterobjects2)$'
# frame index 85: the credits (stars, clouds, sky, the cast, Torque's rocket, MP2, Thanks for playing, CONTINUE)
P='^(npc|brevon|themagister|gongshield|lamppostfront|serpentinechestplate|playerskinspade|playerskintorque)'
python3 tools/import_ui.py build/frames extracted/linux/images "$D/end" cred 86 \
  -x '^(fade|separatormasterobjects2|lamppostback|playerskinsashhair)$' \
  -k "$P=0.0" -k '^playerskin(sash|carol|milla)(tail)?$=37.1' -k '^star$=0.0'
python3 tools/import_ending.py "${FP_BIN:-/home/claude/fprun/bin64/Chowdren}" build/re "$D/end"
cp "$D/ui/font.spr" "$D/ui/font.tex" "$D/end/"   # the credits font in the ending's own pack (end.pak: one pack per load)
