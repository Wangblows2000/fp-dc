#!/bin/sh
# Developer tool (session 25): the first Adventure arc's scene scripts from the RV traces of tools/orig/story_rv.sh
# (build/orig_traces/adv, never committed) -> tables/story/scripts/*.scn + tables/story/rows.json (committed).
#   traces: room:81:2:0 -> c81_g2.log, room:75:0:0 -> c75_g0.log, room:75:1:0 -> c75_g1.log (STORY_READ=30 STORY_IDLE=600)
# Frame numbers below are those traces' (RV): F events (room changes), the character select (slot blink 1454, the choice
# 1916), the wipes (sound 56 at 1968 / 5478, the frame change 51 frames later), the opening's fade (1745-1840) and caption
# (1890-2060, screenshots), the white flash of the opening's crash (the crashed ship shows at 920; white in the shots 960 /
# 1020, fading 1080 / 1140, gone 1200: 920 + 120 held + 140 fading, INFERRED from the screenshots, the cover is not traced).
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/adv}
S=tables/story/scripts
mkdir -p $S
python3 tools/story_rv2scn.py $T/c81_g2.log adv_opening 82 - 0 2092 $S/adv_opening.scn --next scene:dv_meet \
  --flash 920,120,140 --fadeout 1745,95 --caption '1890,2060,str_three_days_later_330|str_in_a_valley_to_the_east_331' --drop 'bossholodragon|caravanwheel'
python3 tools/story_rv2scn.py $T/c75_g0.log dv_meet 76 3903 0 2019 $S/dv_meet.scn --next stage:dv1 \
  --chsel 1454,1916 --fadeout 1968,51 \
  --rows characterslot_3911:0:0,characterslot_3911:0:1,character1_219:7:0,character2_220:7:0,character1_219:0:0,character2_220:0:0
python3 tools/story_rv2scn.py $T/c75_g1.log dv_crash 76 3903 0 2533 $S/dv_crash.scn --next scene:dv_crash2
python3 tools/story_rv2scn.py $T/c75_g1.log dv_crash2 15 550 2533 3578 $S/dv_crash2.scn --next scene:rm_gong
python3 tools/story_rv2scn.py $T/c75_g1.log rm_gong 77 3923 3578 5529 $S/rm_gong.scn --next stage:rm1 --fadeout 5478,50
