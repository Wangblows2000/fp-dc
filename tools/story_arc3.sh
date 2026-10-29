#!/bin/sh
# Developer tool (Adventure phase 2, groups C1 / C2: the scenes of json frame 83, docs/ADVENTURE.md section 10): their scene
# scripts from the RV traces of tools/orig/story_rv.sh (build/orig_traces/adv3, never committed) -> tables/story/scripts/*.scn
# + tables/story/rows.json (committed). Frame 83 is split into one story room per scene (--room 83<CutsceneList index>,
# cropped to the scene's camera views by tools/import_frame.py): one room holding every scene overflowed the _ens atlases.
#   traces (STORY_READ=30 STORY_IDLE=600, tools/orig/story_rv.sh room:82:<g4>:<g1>):
#     a83_g0 (g4 0), a83_g1 (g4 1), c83_g8 (g4 8 -> 9 -> 12 -> 10 -> 11 -> JC1),
#     c83_g13 (g4 13 g1 0 -> TB1), c83_g13c (g4 13 g1 2 -> 14 -> TH1), c83_g17 (g4 17 g1 0 -> 19 -> PL1),
#     c83_g4 (g4 4 -> frame 0 g4 99 (sg_intruders' prelude, not ported) -> frame 82 g4 5 -> 6 -> frame 78 g4 2)
# Frame windows below are those traces' (RV): the scene starts at its F event or its g4 change (G line, same frame) and
# ends at the next one.
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/adv3}
S=tables/story/scripts
C=tools/story_rv2scn.py
mkdir -p $S
# C1
python3 $C $T/a83_g0.log st_bad_news 83 4026 0 1175 $S/st_bad_news.scn --room 8306 --next scene:th_torque_revealed
python3 $C $T/a83_g1.log sg_brevon_plan 83 4026 0 2102 $S/sg_brevon_plan.scn --room 8309 --next scene:th_next_morning
python3 $C $T/c83_g4.log sm_zao_dinner 83 4026 0 1694 $S/sm_zao_dinner.scn --room 8314 --next scene:sg_intruders
python3 $C $T/c83_g4.log sg_intruders 83 4026 2102 3319 $S/sg_intruders.scn --room 8315 --next scene:sm_zao_help
python3 $C $T/c83_g4.log sm_zao_help 83 4026 3319 4215 $S/sm_zao_help.scn --room 8316 --next scene:zao_airship
python3 $C $T/c83_g8.log st_magister_talk 83 4026 0 2801 $S/st_magister_talk.scn --room 8320 --next scene:jail_plan
# C2
python3 $C $T/c83_g8.log jail_plan 83 4026 2801 4014 $S/jail_plan.scn --room 8321 --next scene:sg_brevon_torque
python3 $C $T/c83_g8.log sg_brevon_torque 83 4026 4014 5160 $S/sg_brevon_torque.scn --room 8324 --next scene:jail_fail
python3 $C $T/c83_g8.log jail_fail 83 4026 5160 6094 $S/jail_fail.scn --room 8322 --next scene:throne_apology
python3 $C $T/c83_g8.log throne_apology 83 4026 6094 7207 $S/throne_apology.scn --room 8323 --next stage:jc1 --carry
python3 $C $T/c83_g13.log jc_cave 83 4026 0 2198 $S/jc_cave.scn --room 8325 --next stage:tb1
python3 $C $T/c83_g13c.log jc_cave_cm 83 4026 0 1201 $S/jc_cave_cm.scn --room 8325 --next scene:th_carol_help
python3 $C $T/c83_g13c.log th_carol_help 83 4026 1201 1662 $S/th_carol_help.scn --room 8326 --next stage:th1
python3 $C $T/c83_g17.log tb_arrest 83 4026 0 1231 $S/tb_arrest.scn --room 8330 --next scene:st_mission
python3 $C $T/c83_g17.log st_mission 83 4026 1231 2882 $S/st_mission.scn --room 8332 --next stage:pl1
