#!/bin/sh
# Developer tool (session 25): the first Adventure arc's scene scripts from the RV traces of tools/orig/story_rv.sh
# (build/orig_traces/pol, never committed) -> tables/story/scripts/*.scn + tables/story/rows.json (committed).
#   traces: room:81:2:0 -> c81_g2.log, room:75:0:0 -> c75_g0.log, room:75:1:0 -> c75_g1.log
#   (STORY_TAP=0 STORY_READ=30 STORY_IDLE=600 STORY_BLEND=1; session 27: the same F / A / I frames as session 25's traces)
# Frame numbers below are those traces' (RV): F events (room changes), the character select (slot blink 1454, the choice
# 1916). Session 27 (cutscene polish, docs/ADVENTURE.md 12): every conversion passes --by-class (an address reused by another
# class is another actor: dv_crash's dust and bullets were drawn as sparkles), --covers (the traced blend alpha: the fade
# covers frame by frame, the opening's white flash LevelCardFade_423 direction 8, the wipes; actor alpha), --mv (lossless
# position paths) and --npc-skins (Carol's parked bike ObjectBike_459 in dv_meet); --loc the location names (RE 3322, 18771).
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/pol}
S=tables/story/scripts
C="--by-class --covers --mv --npc-skins --pieces"
mkdir -p $S
python3 tools/story_rv2scn.py $C $T/c81_g2.log adv_opening 82 - 0 2092 $S/adv_opening.scn --next scene:dv_meet \
  --caption '1890,2060,str_three_days_later_330|str_in_a_valley_to_the_east_331' --drop 'bossholodragon|caravanwheel'
python3 tools/story_rv2scn.py $C $T/c75_g0.log dv_meet 76 3903 0 2019 $S/dv_meet.scn --next stage:dv1 \
  --chsel 1454,1916 \
  --rows characterslot_3911:0:0,characterslot_3911:0:1,character1_219:7:0,character2_220:7:0,character1_219:0:0,character2_220:0:0
python3 tools/story_rv2scn.py $C $T/c75_g1.log dv_crash 76 3903 0 2533 $S/dv_crash.scn --next scene:dv_crash2
python3 tools/story_rv2scn.py $C $T/c75_g1.log dv_crash2 15 550 2533 3578 $S/dv_crash2.scn --next scene:rm_gong \
  --loc 'str_dragon_valley_10|str_lilac_s_treehouse_113'
python3 tools/story_rv2scn.py $C $T/c75_g1.log rm_gong 77 3923 3578 5529 $S/rm_gong.scn --next stage:rm1 --loc str_ancient_temple_304
