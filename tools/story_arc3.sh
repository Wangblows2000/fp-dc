#!/bin/sh
# Developer tool (Adventure phase 2, groups C1 / C2: the scenes of json frame 83, docs/ADVENTURE.md section 10): their scene
# scripts from the RV traces of tools/orig/story_rv.sh (build/orig_traces/adv3, never committed) -> tables/story/scripts/*.scn
# + tables/story/rows.json (committed). Frame 83 is split into one story room per scene (--room 83<CutsceneList index>,
# cropped to the scene's camera views by tools/import_frame.py): one room holding every scene overflowed the _ens atlases.
#   traces (STORY_READ=30 STORY_IDLE=600, tools/orig/story_rv.sh room:82:<g4>:<g1>):
#     a83_g0 (g4 0), a83_g1 (g4 1), a83_g8 (g4 8 -> 9 -> 12 -> 10 -> 11 -> JC1),
#     c83_g20 (g4 20 g1 0 -> frame 81 g4 0), c83_g13 (g4 13 g1 0 -> TB1), c83_g13c (g4 13 g1 2 -> 14 -> TH1), c83_g17 (g4 17 g1 0 -> 19 -> PL1), c83_g17c (g4 17 g1 2 -> frame 81 g4 0),
#     c83_g4 (g4 4 -> frame 0 g4 99 (sg_intruders' prelude, not ported) -> frame 82 g4 5 -> 6 -> frame 78 g4 2)
# Frame windows below are those traces' (RV): the scene starts at its F event or its g4 change (G line, same frame) and
# ends at the next one.
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/pol}
S=tables/story/scripts
# session 27 (cutscene polish, docs/ADVENTURE.md 12): the traces again (STORY_TAP=0 STORY_READ=30 STORY_IDLE=600 STORY_BLEND=1, scale /
# angle / layer), every conversion also with --covers (the covers frame by frame) --mv --npc-skins (Milla / Spade / Torque as NPCs)
# --pieces and the location names (--loc, RE 20934 / 20974 / 21538 / 21827 / 21912 / 21942 / 21962). The windows come from the
# trace: B <trace> <k> is its k-th scene boundary (an F event, or a g4 change in place: its G line), 0 the trace's start
C="tools/story_rv2scn.py --by-class --covers --mv --npc-skins --pieces"
B() {
    [ "$2" = 0 ] && { echo 0; return; }
    [ -f $T/$1.log ] && awk '$1=="F" { f[$2] = 1 } $1=="G" { if (p != "" && $7 != p) g[$2] = 1; p = $7 }
        END { for (x in f) print x; for (x in g) if (!((x + 1) in f)) print x }' $T/$1.log | sort -n | sed -n "$2p"
}
rv() {   # rv <story_rv2scn arguments>: skipped (with a note) when its trace is not there
    for a_ in "$@"; do case $a_ in *.log) [ -f "$a_" ] || { echo "skipped: $a_ missing"; return 0; } ;; esac; done
    python3 "$@"
}
mkdir -p $S
# C1
rv $C $T/a83_g0.log st_bad_news 83 4026 "$(B a83_g0 0)" "$(B a83_g0 1)" $S/st_bad_news.scn --room 8306 --next scene:th_torque_revealed \
  --loc 'str_royal_palace_339|str_shang_tu_340'
rv $C $T/a83_g1.log sg_brevon_plan 83 4026 "$(B a83_g1 0)" "$(B a83_g1 1)" $S/sg_brevon_plan.scn --room 8309 --next scene:th_next_morning \
  --loc str_shuigang_palace_22
rv $C $T/c83_g4.log sm_zao_dinner 83 4026 "$(B c83_g4 0)" "$(B c83_g4 1)" $S/sm_zao_dinner.scn --room 8314 --next scene:sg_intruders
rv $C $T/c83_g4.log sg_intruders 83 4026 "$(B c83_g4 2)" "$(B c83_g4 3)" $S/sg_intruders.scn --room 8315 --next scene:sm_zao_help
rv $C $T/c83_g4.log sm_zao_help 83 4026 "$(B c83_g4 3)" "$(B c83_g4 4)" $S/sm_zao_help.scn --room 8316 --next scene:zao_airship
rv $C $T/a83_g8.log st_magister_talk 83 4026 "$(B a83_g8 0)" "$(B a83_g8 1)" $S/st_magister_talk.scn --room 8320 --next scene:jail_plan \
  --loc 'str_royal_palace_339|str_shang_tu_340'
# C2
rv $C $T/a83_g8.log jail_plan 83 4026 "$(B a83_g8 1)" "$(B a83_g8 2)" $S/jail_plan.scn --room 8321 --next scene:sg_brevon_torque
rv $C $T/a83_g8.log sg_brevon_torque 83 4026 "$(B a83_g8 2)" "$(B a83_g8 3)" $S/sg_brevon_torque.scn --room 8324 --next scene:jail_fail \
  --loc str_shuigang_palace_22
rv $C $T/a83_g8.log jail_fail 83 4026 "$(B a83_g8 3)" "$(B a83_g8 4)" $S/jail_fail.scn --room 8322 --next scene:throne_apology
rv $C $T/a83_g8.log throne_apology 83 4026 "$(B a83_g8 4)" "$(B a83_g8 5)" $S/throne_apology.scn --room 8323 --next stage:jc1 --carry
rv $C $T/c83_g13.log jc_cave 83 4026 "$(B c83_g13 0)" "$(B c83_g13 1)" $S/jc_cave.scn --room 8325 --next stage:tb1 \
  --loc 'str_jade_creek_outskirts_361|str_15_minutes_later_362'
rv $C $T/c83_g13c.log jc_cave_cm 83 4026 "$(B c83_g13c 0)" "$(B c83_g13c 1)" $S/jc_cave_cm.scn --room 8325 --next scene:th_carol_help
rv $C $T/c83_g13c.log th_carol_help 83 4026 "$(B c83_g13c 1)" "$(B c83_g13c 2)" $S/th_carol_help.scn --room 8326 --next stage:th1 \
  --loc 'str_storm_drainage_tunnel_363|str_shang_tu_340'
rv $C $T/c83_g17.log tb_arrest 83 4026 "$(B c83_g17 0)" "$(B c83_g17 1)" $S/tb_arrest.scn --room 8330 --next scene:st_mission
rv $C $T/c83_g17.log st_mission 83 4026 "$(B c83_g17 1)" "$(B c83_g17 2)" $S/st_mission.scn --room 8332 --next stage:pl1
rv $C $T/c83_g20.log pl_final_message 83 4026 "$(B c83_g20 0)" "$(B c83_g20 1)" $S/pl_final_message.scn --room 8333 --next scene:sf_army
rv $C $T/c83_g17c.log tb_outside_team 83 4026 "$(B c83_g17c 0)" "$(B c83_g17c 1)" $S/tb_outside_team.scn --room 8331 --next scene:sf_army
