#!/bin/sh
# Developer tool (session 25b, Adventure phase 2 groups A + B, docs/ADVENTURE.md section 10): the scene scripts of the Relic
# Maze aftermath (room 77, RM5's in-stage dialogue) and Treehouse -> Fortune Night -> Zao's airship (rooms 16, 70, 15, 78, 79,
# 36) from RV traces of tools/orig/story_rv.sh (STORY_READ=30 STORY_IDLE=600 STORY_BLEND=1; build/orig_traces/adv2, never
# committed) -> tables/story/scripts/*.scn + tables/story/rows.json. One script per character where the original stages the
# room differently for g1 (the player's skin, its partners: RV b15_g11_l / _c / _m): <id> Lilac, <id>_c Carol, <id>_m Milla
# (story.c chr_variant(): same json frame and g4, the chr mask picks; scenes.json sid_of: a variant shares its base's number).
#   b76_g2_l   room:76:2:0  Meet Milla (Lilac)                       -> frame 82 g4 0 at 3084
#   b76_g3_c/m room:76:3:2 / 76:3:4  Cave Collapse (Carol / Milla) -> the room with g4 2 at 1206 (RE 19077)
#   b76_g2_c/m room:76:2:2 / 76:2:4  Meet Milla as Carol / Milla see it (Carol wakes up: line 27)
#   b76_g4_m   room:76:4:4  Milla's Hideout (Milla, after Aqua Tunnel)
#   b28_g0_l/c room:28:0:0 / 28:0:2 with STORY_TP=150:2600:880 STORY_KEYS=160-420:R  RM5 to the labyrinth exit: the Adventure
#              dialogue (0x22517) from 6864 at frame 249 to the control return at 1571 (in-stage scene, next "back")
#   b15_g11_l/c/m room:15:11:g1  Torque Revealed (16 g4 11), its flashback (70 g4 99), part 3 (16 g4 13), [Brevon's Plan, C1],
#              The Next Morning (15 g4 12), Gong's Help (78 g4 0), Spade Encounter (79 g4 0), FN1; Milla: Torque Revealed (her
#              lines) straight to [Brevon's Plan]
#   b78_g1_l/c/m room:78:1:g1  Robopanther Defeated (79 g4 1) -> [Zao's Dinner, C1]
#   b78_g2_l/c/m room:78:2:g1  Zao's Airship (79 g4 2), part 2 on deck (SB1's frame 36, g4 99) -> [the ambush, frame 82 g4 7]
# Room windows are the traces' F events (RV); fades are the LevelCardFade covers' blend alpha (--covers).
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/adv2}
S=tables/story/scripts
mkdir -p $S
C="--split-reuse --covers --keep ^playerskinmilla"
cv() {   # cv <trace> <id> <json> <dialog> <first> <end> <next> [options]
    tr=$1; id=$2; shift 2
    if [ -z "$4" ]; then echo "$id: $tr has no room change after $3 (trace too short): skipped"; return 0; fi
    python3 tools/story_rv2scn.py $T/$tr.log $id "$1" "$2" "$3" "$4" $S/$id.scn --next "$5" $C ${6:+"$6"} ${7:+"$7"}
}
ok() {   # ok <trace>: the trace is there and complete (gdb detached at its last frame)
    grep -q detached $T/$1.log.gdb 2>/dev/null
}
F() {    # F <trace> <k>: frame of the k-th room change (F event) of a trace
    awk -v k=$2 '$1=="F" { n++; if (n==k) { print $2; exit } }' $T/$1.log
}
# ---- A: Relic Maze aftermath
cv b76_g2_l rm_meet_milla 77 3923 0 "$(F b76_g2_l 1)" scene:st_bad_news
for v in c m; do                  # Carol / Milla: Cave Collapse (g4 3), then the room again with g4 2 (RE 19077), their Meet Milla
    if ok b76_g2_$v; then cv b76_g2_$v rm_meet_milla_$v 77 3923 0 "$(F b76_g2_$v 1)" scene:st_bad_news; fi
done
if ok b76_g3_c; then cv b76_g3_c rm_collapse 77 3923 0 "$(F b76_g3_c 1)" scene:rm_meet_milla; fi
if ok b76_g3_m; then cv b76_g3_m rm_collapse_m 77 3923 0 "$(F b76_g3_m 1)" scene:rm_meet_milla; fi
if ok b76_g4_m; then cv b76_g4_m rm_hideout 77 3923 0 "$(F b76_g4_m 1)" stage:rm1; fi      # -> frame 24 (RM1) at 1858, the wipe from 1807
cv b28_g0_l rm5_spade 29 1624 249 1571 back --drop 'lightray|npchookline'      # the rays / hook line: rm_mantalith.c
cv b28_g0_c rm5_spade_c 29 1624 249 1571 back --drop 'lightray|npchookline'
# ---- B: Treehouse -> Fortune Night -> Zao's airship, per character
for v in l c m; do
    s=$( [ $v = l ] && echo "" || echo _$v )
    t=b15_g11_$v
    if [ $v = m ] && ok $t; then     # Milla (RV b15_g11_m): her Torque Revealed has no flashback (frame 82 g4 1 at 2688)
        cv $t th_torque_revealed_m 16 622 0 "$(F $t 1)" scene:sg_brevon_plan
        cv $t th_next_morning_m 15 550 "$(F $t 2)" "$(F $t 3)" scene:np_gong_help
        cv $t np_gong_help_m 78 3948 "$(F $t 3)" "$(F $t 4)" scene:sm_spade
        cv $t sm_spade_m 79 3951 "$(F $t 4)" "$(F $t 5)" stage:fn1
    elif ok $t; then
        cv $t th_torque_revealed$s 16 622 0 "$(F $t 1)" scene:th_torque_revealed2
        cv $t th_torque_revealed2$s 70 3616 "$(F $t 1)" "$(F $t 2)" scene:th_torque_revealed3
        cv $t th_torque_revealed3$s 16 622 "$(F $t 2)" "$(F $t 3)" scene:sg_brevon_plan
        cv $t th_next_morning$s 15 550 "$(F $t 4)" "$(F $t 5)" scene:np_gong_help
        cv $t np_gong_help$s 78 3948 "$(F $t 5)" "$(F $t 6)" scene:sm_spade
        cv $t sm_spade$s 79 3951 "$(F $t 6)" "$(F $t 7)" stage:fn1
    fi
    t=b78_g1_$v
    if ok $t; then cv $t fn_panther_defeated$s 79 3951 0 "$(F $t 1)" scene:sm_zao_dinner; fi
    t=b78_g2_$v
    if ok $t; then
        cv $t zao_airship$s 79 3951 0 "$(F $t 1)" scene:zao_airship2
        cv $t zao_airship2$s 36 2135 "$(F $t 1)" "$(F $t 2)" scene:zao_airship_ambush
    fi
done
