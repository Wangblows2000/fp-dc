# Adventure phase 2, groups A and B (session 25b)

This page extends `docs/ADVENTURE.md` (read that first: inventory, runtime, data format, gating). It covers the Relic Maze
aftermath (group A: room 77 and RM5's in-stage dialogue) and Treehouse to Fortune Night to Zao's airship (group B: rooms
16, 70, 15, 78, 79 and 36). Labels as there: **RE** (decompiled), **RV** (original runtime trace), **INFERRED**.

Sources: `tools/story_arc2.sh` (the scripts from the traces), `tables/story/scenes.json` (my entries), `tests/test_adventure_arc2.py`.
Traces: `build/orig_traces/adv2` (never committed).

## 1. What the original does (RE / RV)

**The developer list, complete (RE events 262-298).** CutsceneList_42's selection K maps to frame / g4 (/ g1). K = list + 1:

| K | list | scene | frame index (json) | g4 |
|---|---|---|---|---|
| 5 | 4 | Milla's Hideout | 76 (77) | 0, g1 4 (frame 8 uses g4 4) |
| 6 | 5 | Cave Collapse | 76 (77) | 3, g1 2 |
| 8 | 7 | Meet Milla | 76 (77) | 2 |
| 9 | 8 | Torque Revealed | 15 (16) | 11 |
| 10 | 9 | Brevon's Plan | 82 (83) | 1 |
| 11 | 10 | The Next Morning | 14 (15) | 12 |
| 12 | 11 | Gong's Help | 77 (78) | 0 |
| 13 | 12 | Spade Encounter | 78 (79) | 0 |
| 14 | 13 | Robopanther Defeated | 78 (79) | 1 |
| 15 | 14 | Zao's Dinner | 82 (83) | 4 |
| 16 | 15 | Brevon Thwarts the Intruders | 0 (1) | 99 |
| 17 | 16 | Zao's Help | 82 (83) | 6 |
| 18 | 17 | Zao's Airship | 35 (36 = SB1's frame) | 99 |
| 19 | 18 | Zao's Airship (Ambushed) | 82 (83) | 7 |
| 20 | 19 | Jiang River | 36 (37 = SB2's frame) | 99 |
| 21 | 20 | Talk With Magister | 82 (83) | 8 |

(K 22-38: frame 82 g4 9-20, frame 80 g4 0 / 1, frame 81 g4 0 / 1, frame 83; groups C / D / E.)

**For the other groups (findings, not changed here):** Zao's Airship (Ambushed) is frame 82 g4 7 (RE K 19, RE 8995 / 9205:
SB1's deck scene ends there, RV r78_g1_l: the ambush, lines 95-96, then SB1 with g4 0), not SB2 with g4 99; SB2's frame with
g4 99 is Jiang River (RE K 20). The inventory's zao_airship_ambush entry (room 37 / 99, end:4) needs that correction.

**The chains (RV, the room changes of the traces):**

- Lilac, after RM (frame 8, I 2): Meet Milla (77 g4 2) -> [Gong's Bad News 83 g4 0] -> Torque Revealed (16 g4 11) -> the
  flashback (70 g4 99: Final Dreadnought's frame as the set) -> Torque Revealed part 3 (16 g4 13) -> [Brevon's Plan 83 g4 1] ->
  The Next Morning (15 g4 12) -> Gong's Help (78 g4 0) -> Spade Encounter (79 g4 0) -> FN1.
- Milla's Torque Revealed (RV b15_g11_m) has no flashback: after her lines the room goes straight to [Brevon's Plan 83 g4 1]
  (frame 82 g4 1 at 2688); th_torque_revealed_m's next is sg_brevon_plan.
- Carol / Milla, after RM: Cave Collapse (77 g4 3) -> the same room with g4 2 (RE 19077) = Meet Milla as they see it (Carol wakes
  up, line 27) -> [Gong's Bad News] -> the same chain.
- Milla, after Aqua Tunnel (I 0): Milla's Hideout (77 g4 4) -> RM1 (the wipe, sound 56, then frame 24).
- After FN (I 3): Robopanther Defeated (79 g4 1) -> [Zao's Dinner 83 g4 4] -> [Intruders, frame 0 g4 99] -> [83 g4 5] ->
  [Zao's Help 83 g4 6] -> Zao's Airship (79 g4 2) -> part 2 on Zao's deck (36 g4 99) -> [the ambush 83 g4 7] -> SB1.
- RM5 in Adventure (RE 6864: g0 >= 2, g1 < 4, ML flag 0x80 off): leaving the labyrinth starts the Kingdom Stone dialogue with
  Spade (group 0x22517, DialogData_1624, m_trouble) instead of the Classic scene; at the stone's D 14 (6680 / 6885) the player
  has control again at (3453, 938). Milla gets the Classic scene.

[...] are other groups' scenes: the steps name them (`scene:st_bad_news`, `scene:sg_brevon_plan`, `scene:sm_zao_dinner`,
`scene:zao_airship_ambush`); until they are in the data a step to them ends the run at the menu (`story next ... (not in the
table)` on the host).

**Pause (RE per room, the GUIPause row events):**

| Room / g4 | SKIP SCENE | NEXT STAGE |
|---|---|---|
| 77 g4 2 (Meet Milla) | 82 g4 0 (RE 19133) | FN1 (RE 19136) |
| 77 g4 3 / 4 (Collapse, Hideout) | 82 g4 0 (RE 19134) | FN1 (RE 19136) |
| 16 g4 < 14 (Torque Revealed 1 / 3) | 82 g4 1 (RE 3984) | FN1 (RE 3985) |
| 70 g4 99 (the flashback) | no pause menu (RV k69_g99_skip: START does nothing; panel 3) | - |
| 15 g4 > 11 (The Next Morning) | 77 g4 0 (RE 3553) | FN1 (RE 3554) |
| 78 (Gong's Help) | 78 g4 0 (RE 19312) | FN1 (RE 19313) |
| 79 g4 0 (Spade Encounter) | FN1 (RE 19698) | FN1 (RE 19698) |
| 79 g4 1 (Robopanther Defeated) | 82 g4 4 (RE 19699) | SB1 (RE 19701) |
| 79 g4 2 (Zao's Airship) | 35 g4 99 = part 2 (RE 19700) | SB1 (RE 19701) |
| 36 g4 99 (Zao's deck) | no pause menu (RV k35_g99_skip / _late: START does nothing; panel 3; RE 9205 / 9206 would give 82 g4 7 / SB1) | - |
| RM5 Adventure dialogue | no pause (the pause group is off from 6861: panel 3) | - |

RV confirmations: see section 6.

**Characters (RV b15_g11_l / _c / _m, b78_*, b28_*, b76_*).** Every room in these groups stages its scene differently for g1:
the player's character stands where Lilac stands in Lilac's run, the partners take the other places, and some lines are the
character's own (The Next Morning without Lilac's wake-up lines 21-22 for Carol; Spade Encounter's last line 17 / 18 / 19;
Robopanther Defeated's first line 20 / 21 / 22). So each scene has one script per character: `<id>` (Lilac), `<id>_c`,
`<id>_m`; the RM5 dialogue has Lilac's and Carol's; Meet Milla has all three; Cave Collapse Carol's and Milla's.

## 2. Scenes (tables/story/scenes.json)

| id (variants) | room / g4 | trigger | next ; skip ; NEXT STAGE ; panel | lines (DialogData) | status |
|---|---|---|---|---|---|
| rm_meet_milla (_c, _m) | 77 / 2 | end:2 (Lilac) | st_bad_news ; st_bad_news ; fn1 ; 2 | 3923: 28-70 (Carol / Milla: 27, 30-70) | ported (RV) |
| rm_collapse (_m) | 77 / 3 | end:2 (Carol, Milla) | rm_meet_milla (plays rm_meet_milla_c / _m) ; st_bad_news ; fn1 ; 2 | 3923: 71-75 | ported (RV) |
| rm_hideout | 77 / 4 | end:0 (Milla) | stage:rm1 ; st_bad_news ; fn1 ; 2 | 3923: 0-7 | ported (RV) |
| rm5_spade (_c) | in RM5 (json 29) | RM5 6864 (Lilac, Carol) | back (the stage goes on) ; - ; - ; 3 | 1624: 0-14 | ported (RV), in-stage |
| th_torque_revealed (_c, _m) | 16 / 11 | (chain) | th_torque_revealed2 ; sg_brevon_plan ; fn1 ; 2 | 622: 3-69 | ported (RV) |
| th_torque_revealed2 (_c, _m) | 70 / 99 | (chain) | th_torque_revealed3 ; - ; - ; 3 | 3616: 0-1 | ported (RV) |
| th_torque_revealed3 (_c, _m) | 16 / 13 | (chain) | sg_brevon_plan ; sg_brevon_plan ; fn1 ; 2 | 622: 71-91 | ported (RV) |
| th_next_morning (_c, _m) | 15 / 12 | (chain) | np_gong_help ; np_gong_help ; fn1 ; 2 | 550: 21-35 (Carol 23-35) | ported (RV) |
| np_gong_help (_c, _m) | 78 / 0 | (chain) | sm_spade ; sm_spade ; fn1 ; 2 | 3948: 0-21 | ported (RV) |
| sm_spade (_c, _m) | 79 / 0 | (chain) | stage:fn1 ; stage:fn1 ; fn1 ; 2 | 3951: 0-17 / 18 / 19 | ported (RV) |
| fn_panther_defeated (_c, _m) | 79 / 1 | end:3 | sm_zao_dinner ; sm_zao_dinner ; sb1 ; 2 | 3951: 20 / 21 / 22, 24-49 | ported (RV) |
| zao_airship (_c, _m) | 79 / 2 | (chain) | zao_airship2 ; zao_airship2 ; sb1 ; 2 | 3951: 58-62 | ported (RV) |
| zao_airship2 (_c, _m) | 36 / 99 | (chain) | zao_airship_ambush ; - ; - ; 3 | 2135: 11-32 | ported (RV) |

Lines that no RV route shows are listed in each entry's `missing` (e.g. 622: 0-2, 12-30, 48-51, 53, 56-63; 3951: 50-53).
Every scene completes once, sets its seen flag and the file's story position (story.c complete, unchanged).

## 3. Runtime additions

- **Variants** (story.c `chr_variant()`, shared with arc 3): a step or a trigger names a scene; when its chr mask does not
  hold the character, the scene with the same json frame and g4 whose mask does plays instead. A variant (`<id>_c`, `<id>_m`)
  carries `sid_of: <id>` in scenes.json: it shares its base scene's permanent number (tables/story/sids.json; seen flag and
  saved story position), so the 64 numbers are not spent on the 20 variants (import_story.py, tests/test_story_sids.py). New
  numbers: rm5_spade 40, th_torque_revealed2 41, th_torque_revealed3 42, zao_airship2 43. Triggers keep working as in phase 1 (`end:I` + chr).
- **In-stage scenes** (story.c `story_inline_begin` / `story_inline_done`, table `story/inline.txt`, next `back`): the script
  plays over the stage that is loaded (no room; actors from the stage's `_ens`, its rows built in by `story_rows(json)`); at its
  end the stage's sound group comes back and MODE_PLAY resumes. rm_mantalith.c starts it at 6864 (once: ML flag 0x80) and
  applies 6680 / 6885 when it ends (Lilac / Carol at (3453, 938), the stone and Spade risen, the camera following, m_trouble
  fading). During the scene the light rays and the hook line are drawn by rm_mantalith.c from the scene's stone (6901 / 6880
  rules, INFERRED: their scale / angle are not traced).
- **Script entries** (src/game/story.h): `a <n> <alpha>` (an actor's traced blend alpha), `fa <alpha> [1]` (the cover's alpha:
  the LevelCardFade objects at the view, frame by frame), `fw <x>` (the black wipe, drawn as the pause rows' wipe:
  `game_wipe_draw`), panel 3 (no pause menu), up to 256 actors.
- **Sound** (game.c): a scene loads only the ids its script plays that sound RAM does not hold yet (the boot set and the
  character's group are resident), from `story/sfx/s<id>.wav` (11,025 Hz, tails trimmed; a clip over 65,000 samples at a
  lower rate: s444, 18 s, at 3,610 Hz), and frees them when it ends. **Fix:** a stage's set load no longer frees ids of the
  boot set: phase 1's scene sets named shared ids (56, 8, 26, 53, 95, 311 ...), so the next stage lost them (the jump sound 8
  after Meet Gong and Neera -> RM1).
- **Story-room VRAM** (tools/import_frame.py, game.c, shields.c): a story room's `_ens` holds only the scenes' rows; a still
  anim (speed 0, the skins' pose library anim 40: 108 frames) keeps only the frames the room's scenes show (their scripts' `i` entries);
  pieces far (> 1024 px) from every scene's view are left out (rows.json `vbox`; rooms whose scenes all have one; arc 3's split rooms crop by `view` instead); no shield
  bank; no bike / cube bank for Carol / Milla (then her banks are not kept resident: the next level loads them again).
  `MAX_ATLAS` is 6 (room 79's 9,600 px street needs 5 piece atlases).
- **Host**: `story next <step>` after each completion, `story sfx ids <n> loaded <k> bytes <b>`, `vopeak <bytes>` (the two voice
  slots' peak) on the completion line, LOAD leaves a playing scene.

## 4. Tools

- `tools/orig/trace_story.py`: `STORY_BLEND=1` (blend alpha / colour on O lines), `STORY_SHOTS=f,f` (screenshots at frames),
  `STORY_TP=f:x:y` (teleport through PlayerMovementValues, logged `T`).
- `tools/story_rv2scn.py`: `--covers` (actor alpha, covers, wipe, piecewise-linear moves `l`, rows.json `vbox`), `--split-reuse` (an address that
  another class reuses is another actor: RV r76_g2_l, sparkle3 -> Carol's tail), `--keep REGEX` (classes DROP would leave
  out: Milla's skin as an NPC). `--split-reuse` keeps the rules these scripts were converted with where arc 3's `--by-class`
  differs (a visible state flagged 0x2 is a sighting: Spade's afterimages; no start-of-room drop; a parallax piece is hidden
  by its own D only). Checked after the merge with arc 3: the converter regenerates arc 3's 17 scripts and rows byte for
  byte from build/orig_traces/adv3, arc 1's except master's first-camera fix (dv_crash2 / rm_gong would lose the stale
  pre-window `c` line at 0; not regenerated here), and this group's with that fix (18 scripts lose that line).
- `tools/import_story.py`: `story/inline.txt`; no room for in-stage scenes.
- `tools/audio/convert_audio.py story`: `story/sfx/` (every scene id), manifest `story/sfx_manifest.csv`.
- `tools/make_disc.py`: `story/sfx/`.

## 5. Budgets (measured on the host build, story data built into the data dir)

**Sound RAM per scene** (the scene's set: bytes loaded beyond what is resident; + the two voice slots' peak; budget 200 KB):

Before = phase 1's runtime (session 25): the script's every id set as the stage group, loaded from `sfx/` (the character
groups at 22,050 Hz) when not resident; "all ids" is that set's whole size (phase 1's report: 302 KB for dv_crash, with the
ids it had then). After = this session: only the ids not resident, from `story/sfx` (host `story sfx ... bytes`). Voices =
the two voice slots' peak (host `vopeak`). KB = 1024 bytes; Lilac / Carol / Milla where they differ.

| Scene | all ids (before) | loaded before (L / C / M) | loaded after (L / C / M) | voices | after total (max) |
|---|---|---|---|---|---|
| adv_opening | 44.0 | 44.0 | 44.0 | (one 'o' clip) | 44 + clip |
| dv_meet | 262.9 | 134.1 / 51.2 / 147.0 | 75.4 / 37.3 / 79.6 | 27.2 | 106.7 |
| dv_crash | 249.3 | 144.6 / 70.1 / 140.1 | 84.2 / 46.8 / 85.2 | 35.3 | 120.5 |
| dv_crash2 | 13.7 | 1.0 | 1.0 | 46.5 | 47.4 |
| rm_gong | 101.5 | 23.7 / 20.3 / 23.7 | 20.9 / 20.3 / 20.9 | 47.9 | 68.8 |
| rm5_spade (in RM5) | 80.0 | 51.0 / 44.4 / - | 25.2 / 23.7 / - | 45.1 | 70.3 |
| rm_meet_milla | 6.7 | 6.7 / 0 / 6.7 | 2.7 / 0 / 6.5 | 49.2 | 55.7 |
| rm_collapse | 92.0 | - / 36.6 / 49.6 | - / 29.3 / 38.3 | 17.5 | 55.8 |
| rm_hideout | 139.2 | - / - / 51.4 | - / - / 28.2 | 59.9 | 88.1 |
| th_torque_revealed (1 / 2 / 3) | 47.1 / 0 / 12.0 | 13.0 / 0 / 12.0 | 4.5 (Milla 3.8) / 0 / 2.8 | 55.6 / 49.7 / 61.0 | 63.8 |
| th_next_morning | 36.8 | 0 | 7.9 / 0 / 0 | 41.3 | 49.2 |
| np_gong_help | 83.3 | 9.5 / 26.7 / 17.3 | 24.4 / 27.6 / 25.8 | 55.8 | 83.4 |
| sm_spade | 64.5 | 22.6 | 22.6 | 44.0 | 66.6 |
| fn_panther_defeated | 92.8 | 37.2 / 36.6 / 50.2 | 58.1 / 57.7 / 62.2 | 49.3 | 111.5 |
| zao_airship | 47.9 (s444 missing in phase 1's data) | 11.5 + s444 | 39.3 | 48.5 | 87.8 |
| zao_airship2 | 0 | 0 | 0 | 63.1 | 63.1 |

The largest scene is now Robopanther Defeated with Milla: 62.2 + 49.3 = 111.5 KB (budget 200 KB). After is larger than
before in a few rows where phase 1 left the ids it loaded resident when they were one of the boot set (freed by the next
stage then: the bug above) or where an id was missing from phase 1's data. Every set is loaded at 11,025 Hz with trimmed tails
(s444: 3,610 Hz).

Arc 3's scenes (groups C1 / C2, merged before this) with this runtime, measured the same way after the rebase (scene set +
voice peak, KB; Lilac / Carol / Milla where they differ). Arc 3 reported throne_apology 156 KB and jc_cave 134 KB of scene
sound under phase 1's runtime (+ up to 63 KB of voices):

| Scene | set (L / C / M) | voices | total (max) |
|---|---|---|---|
| st_bad_news | 19.5 | 58.8 | 78.3 |
| sg_brevon_plan | 72.4 | 50.4 | 122.8 |
| sm_zao_dinner | 0 | 61.5 | 61.5 |
| sg_intruders | 25.0 | 63.1 | 88.1 |
| sm_zao_help | 6.2 | 55.8 | 62.1 |
| st_magister_talk | 21.7 | 60.6 | 82.3 |
| jail_plan / jail_fail | 1.5 / 4.7 | 47.9 / 17.2 | 49.4 / 21.8 |
| throne_apology | 27.9 / 18.1 / 27.9 | 62.2 | 90.1 |
| sg_brevon_torque | 10.2 / 5.7 / 10.2 | 60.0 | 70.2 |
| jc_cave (Lilac) / jc_cave_cm | 33.9 / 32.9, 31.8 | 59.8 | 93.7 |
| th_carol_help | 1.5 / 1.0 / 1.5 | 46.7 | 48.2 |
| tb_arrest (Lilac) / tb_outside_team | 18.3 / 31.2, 30.1 | 31.1 / 50.1 | 81.3 |
| st_mission | 0 | 62.2 | 62.2 |
| pl_final_message | 21.6 | 62.8 | 84.5 |

The largest of all ported scenes is now sg_brevon_plan: 122.8 KB (dv_crash with Milla 120.5 KB); every scene with its voice
slots is under 200 KB.

**VRAM per room** (the host load profile's `tex` KB: every texture the room load uploads, the character's banks loaded fresh;
the shipped SB1 with Carol is 5,763 KB, measured 515 KB free on Flycast):

| Room | Lilac | Carol | Milla | scenes |
|---|---|---|---|---|
| st15 (Treehouse, day) | 3,551 | 3,695 | 3,863 | dv_crash2, th_next_morning |
| st16 (Treehouse, night) | 4,319 | 4,463 | 4,631 | th_torque_revealed 1 / 3 |
| st36 (SB1's frame: Zao's deck) | 4,627 | 4,771 | 4,939 | zao_airship2 |
| st70 (FD3a's frame: the flashback) | 2,298 | 2,442 | 2,610 | th_torque_revealed2 |
| st76 | 4,063 | 4,207 | 4,375 | dv_meet, dv_crash (phase 1: 4,854 / 5,386 / 5,222) |
| st77 | 4,514 | 4,658 | 4,826 | rm_gong, Meet Milla x3, Cave Collapse x2, Milla's Hideout |
| st78 (Northern Plateau) | 3,440 | 3,584 | 3,752 | np_gong_help |
| st79 (Shang Mu street) | 5,335 | 5,479 | 5,647 | sm_spade, fn_panther_defeated, zao_airship |
| st82 | 3,230 | 3,374 | 3,542 | adv_opening (phase 1: 4,526 / 5,048 / 4,884) |
| rm5 (with the dialogue's rows) | 4,730 | 5,258 | 5,090 | was 4,218 / 4,746 / 4,578 (Flycast: 2,060 KB free for Lilac) |
| fn3 / sb1 (shipped, for scale) | 5,125 / 5,235 | 5,653 / 5,763 | 5,485 / 5,595 | |

Every room is under SB1 with Carol (5,763 KB, 515 KB free on Flycast); ui/font adds 32 KB during a scene. Room 79 with Milla
(5,647 KB) is the largest.

## 6. RV checks of the pause rows

Traces `k<frame>_g<g4>_<row>` (tools/orig/story_rv.sh with STORY_READ=0: START at 300, Down at 330 (and 345 for the second
row), A at 360; frame numbers are the original's frame index = json room - 1):

| Trace | Scene | Row | Result (RV) |
|---|---|---|---|
| k76_g2_skip | Meet Milla (77 g4 2) | SKIP SCENE | frame 82 g4 0 at 413 (Gong's Bad News) |
| k76_g4_skip | Milla's Hideout (77 g4 4) | SKIP SCENE | frame 82 g4 0 at 413 (Gong's Bad News, the odd row of 19134 confirmed) |
| k76_g4_row2 | Milla's Hideout (77 g4 4) | NEXT STAGE | frame 29 (FN1) at 413 (RE 19136 confirmed) |
| k15_g11_skip | Torque Revealed (16 g4 11) | SKIP SCENE | frame 82 g4 1 at 413 (Brevon's Plan: the flashback and part 3 are skipped too) |
| k69_g99_skip | the flashback (70 g4 99) | START | no pause menu: the scene goes on (panel 3) |
| k77_g0_row2 | Gong's Help (78 g4 0) | NEXT STAGE | frame 29 (FN1) at 413 |
| k78_g1_skip | Robopanther Defeated (79 g4 1) | SKIP SCENE | frame 82 g4 4 at 413 (Zao's Dinner) |
| k78_g2_skip | Zao's Airship (79 g4 2) | SKIP SCENE | frame 35 g4 99 at 413 (part 2 on the deck) |
| k35_g99_skip / k35_g99_late | Zao's deck (36 g4 99) | START at 300 / at 600 (after six A taps) | no pause menu either time: the scene goes on (panel 3; RE 8965 / 8992 clear the pause group 0x21a4d, so 9205 / 9206 are not reached on these routes) |

Not traced (RE only): Cave Collapse (77 g4 3), The Next Morning (15 g4 12), Spade Encounter (79 g4 0), the NEXT STAGE rows of
16 / 79 / 36. tests/test_adventure_arc2.py plays every scene's pause rows on the host against scenes.json (which follows these tables).

## 7. INFERRED / known differences

- Cover alpha: the covers' blend alpha is traced (RV); a cover counts when it sits at the view or at 0, 0 (INFERRED: their
  scale to the view is not traced). The wipe's panel width is the pause wipe's (phase 1).
- RM5: the light rays / hook line during the dialogue follow the scene's stone (rules of 6901 / 6880, INFERRED); the player
  is placed at the RV position after it.
- Ambient random effects (dust, crystals, pigeons, totems' debris) are replayed as traced for each character (RV: their timing
  differs between runs).
- The flashback room (70) is Final Dreadnought 3a's frame built as a story room (only pieces near the view).
- Sounds over 65,000 samples are converted at a lower rate (s444).
