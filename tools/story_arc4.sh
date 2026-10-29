#!/bin/sh
# Developer tool (session 26, Adventure phase 2 groups D + E): the scene scripts of Jiang River, Thermal Base, Zao's airship
# ambush, the snowfields and the Adventure epilogue from the RV traces of tools/orig/story_rv.sh (build/orig_traces/adv4,
# never committed) -> tables/story/scripts/*.scn + their keys of tables/story/rows.json (committed). docs/ADVENTURE.md 11.
#   traces (STORY_READ=30; STORY_IDLE=0 unless noted):
#     r36_g99   room:36:99:0 (STORY_IDLE=600)  SB2 with g4 99 (frame 8 after Sky Battalion) -> 79 g4 0 (F 661) -> 82 g4 8 (F 2933)
#     r36_g99c / r36_g99m  room:36:99:2 / :4   the same for Carol / Milla (F 661, F 2277 / 2237)
#     r80_g0    room:80:0:0                     the torture (Lilac) -> 80 g4 1 (F 5082) -> 82 g4 17 (F 6766)
#     r80_g1c   room:80:1:2                     the Brevon encounter (Carol) -> 82 g4 17 (F 1886) -> 81 g4 0 (F 3889)
#     r82_g16c  room:82:16:2                    Enter Team Spade (Carol) -> 44 = TB1 (F 540; wipe 56 at 489)
#     r82_g7    room:82:7:0                     Zao's airship ambushed -> 35 = SB1 (F 472; wipe 56 at 420)
#     r81_g0c / r81_g0l  room:81:0:2 / :0      Army of Shang Tu -> 81 g4 1 (F 5774 / 5054)
#     r81_g1c / r81_g1   room:81:1:2 / :0      the encampment -> 84 (F 2330 / 3018)
#     e81_g3n   room:81:3:0                     the epilogue: 81 g4 3 -> 82 g4 21 (F 2698) -> 15 g4 14 (F 7569) -> 85 (F 9174)
#     e81_g3c / e81_g3m  room:81:3:2 / :4      the same for Carol; Milla's part 1 differs (F 2705 / 7576 / 9181)
# Frame numbers below are those traces' (RV): F events (room changes).
# Every conversion passes --mv (lossless 'mv' position paths); ending3 plays in room 15 (json 16), whose hidden
# DialogBox_525 helper is not the box: --dialogbox dialogbox_551.
# Large frames are split into one story room per scene (--room <json><CutsceneList index>, cropped to the scene's camera views
# by tools/import_frame.py, as frame 83 in tools/story_arc3.sh): st3719, st8019, st8127 / 8129, st8234 / 8235 / 8236,
# st8318 / 8328 / 8336, st1636 (a split room's _ens holds only its scenes' rows: whole, json 80 measured 5.6 MB for Carol).
set -e
cd "$(dirname "$0")/.."
T=${1:-build/orig_traces/adv4}
S=tables/story/scripts
mkdir -p $S
# ---- D: Jiang River (CutsceneList K 20 -> frame 36 g4 99, RE handle_frame_2; frame 8 I 4 -> 36 g4 99)
# part 1 in SB2's frame: the falling airship (the stage's own objects shown only at the first frame are left out)
python3 tools/story_rv2scn.py --mv $T/r36_g99.log jiang_river 37 - 0 661 $S/jiang_river.scn --room 3719 --next scene:jiang_river2 \
  --drop 'levelsbplatform|levelbackgroundplane'
# the river branches on g1 (RV r36_g99 / r36_g99c / r36_g99m: Lilac lines 0-24 + 33, Carol 0-10 + 25-33, Milla 0 + 11-24 + 33)
python3 tools/story_rv2scn.py --mv $T/r36_g99.log jiang_river2 80 3965 661 2933 $S/jiang_river2.scn --room 8019 --next scene:st_magister_talk
python3 tools/story_rv2scn.py --mv $T/r36_g99c.log jiang_river2c 80 3965 661 2277 $S/jiang_river2c.scn --room 8019 --next scene:st_magister_talk
python3 tools/story_rv2scn.py --mv $T/r36_g99m.log jiang_river2m 80 3965 661 2237 $S/jiang_river2m.scn --room 8019 --next scene:st_magister_talk
# ---- D: Zao's airship ambushed (CutsceneList K 19 -> frame 82 g4 7: aboard the airship, lines 95-96; wipe 56 at 420, F 472 -> 35 = SB1)
python3 tools/story_rv2scn.py --mv $T/r82_g7.log zao_airship_ambush 83 4026 0 472 $S/zao_airship_ambush.scn --room 8318 --next stage:sb1 --fadeout 420,51
# ---- D: Thermal Base (frame 8 I 6 -> frame 80 g4 0 for Lilac, g4 1 for Carol / Milla). Lilac: the torture (F 5082 -> 80 g4 1),
# then her version of the Brevon encounter (F 6766 -> 82 g4 17); Carol: the Brevon encounter (F 1886 -> 82 g4 17)
python3 tools/story_rv2scn.py --mv $T/r80_g0.log tb_torture 81 3971 0 5082 $S/tb_torture.scn --room 8127 --next scene:tb_brevon_l
python3 tools/story_rv2scn.py --mv $T/r80_g0.log tb_brevon_l 81 3971 5082 6766 $S/tb_brevon_l.scn --room 8129 --next scene:tb_arrest
python3 tools/story_rv2scn.py --mv $T/r80_g1c.log tb_brevon 81 3971 0 1886 $S/tb_brevon.scn --room 8129 --next scene:tb_outside_team
# Enter Team Spade: frame 8 I 8 (Trap Hideout, Carol / Milla) -> frame 82 g4 16 (CutsceneList K 29), F 540 -> 44 = TB1
python3 tools/story_rv2scn.py --mv $T/r82_g16c.log tb_team_spade 83 - 0 540 $S/tb_team_spade.scn --room 8328 --next stage:tb1 --fadeout 489,51
# ---- E: the snowfields (frame 81 = json 82). Army of Shang Tu (g4 0; entered from Pangu Lagoon's Final Message 82 g4 20 for
# Lilac, RV r82_g20 F 2654, and from Outside Thermal Base 82 g4 17 for Carol / Milla, RV r80_g1c F 3889) -> the encampment
# (g4 1; F 5054 Lilac / 5774 Carol) -> frame 84 (the ride to Battle Glacier; F 3018 Lilac / 2330 Carol). Lilac's versions
# differ (lines; RV r81_g0l / r81_g1 against r81_g0c / r81_g1c / r81_g0m). The holo dragon follows the PL6 rig (--rig).
python3 tools/story_rv2scn.py --mv $T/r81_g0c.log sf_army 82 3989 0 5774 $S/sf_army.scn --room 8234 --next scene:sf_camp --rig
python3 tools/story_rv2scn.py --mv $T/r81_g0l.log sf_army_l 82 3989 0 5054 $S/sf_army_l.scn --room 8234 --next scene:sf_camp_l --rig
python3 tools/story_rv2scn.py --mv $T/r81_g1c.log sf_camp 82 3989 0 2330 $S/sf_camp.scn --room 8235 --next stage:bg1 --rig
python3 tools/story_rv2scn.py --mv $T/r81_g1.log sf_camp_l 82 3989 0 3018 $S/sf_camp_l.scn --room 8235 --next stage:bg1 --rig
# ---- E: the Adventure epilogue (after ending.c's space scene, RE 22441: frame 81 g4 3)
python3 tools/story_rv2scn.py --mv $T/e81_g3n.log ending 82 3989 0 2698 $S/ending.scn --room 8236 --next scene:ending2 --rig
# Milla's part 1 (RV e81_g3m: Carol found, lines 103-110, then 83-91; F 2705 -> 82 g4 21 as for Lilac and Carol, e81_g3c)
python3 tools/story_rv2scn.py --mv $T/e81_g3m.log ending_m 82 3989 0 2705 $S/ending_m.scn --room 8236 --next scene:ending2 --rig
# the Magister's speech: SubtitleText_3449 shown / moved (RV e81_g3n: on at 3184, y 140; y 20 from 4018), each paragraph of
# SubtitleData_4135 set by the controller's phases (RE 22248: paragraph 13, 22309: 16; the others in that order, RV shots
# s82_g21 300-4800 read 12-18)
python3 tools/story_rv2scn.py --mv $T/e81_g3n.log ending2 83 - 2698 7569 $S/ending2.scn --room 8336 --carry --next scene:ending3 \
  --sub 3184,3424,sub4135_12,140 --sub 3424,3955,sub4135_13,140 --sub 4081,4883,sub4135_14,20 --sub 5009,5589,sub4135_15,20 \
  --sub 5589,6210,sub4135_16,20 --sub 6336,6876,sub4135_17,20 --sub 6876,7456,sub4135_18,20
python3 tools/story_rv2scn.py --mv $T/e81_g3n.log ending3 16 622 7569 9174 $S/ending3.scn --room 1636 --dialogbox dialogbox_551 --next credits
