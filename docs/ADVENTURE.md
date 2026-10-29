# Adventure mode: the story scenes

This page covers session 25's Adventure work (phase 1). It has the scene inventory, the runtime, the data format, the
first story arc (ported), and how the remaining arcs can be split. Evidence labels:

- **RE**: read from the Linux 1.21.5 program (`bin64/Chowdren`): decompiled events, constructors, string tables.
- **RV**: observed in the running original. Traces come from `tools/orig/trace_story.py` / `story_rv.sh`, run from
  `/home/claude/fprun` under the `.orig.lock`. Raw traces stay in `build/orig_traces/adv` and are never committed.
- **INFERRED**: a choice made without direct evidence. Each one is stated where it is used.

The committed sources are `tables/story/` (inventory, dialogue, scripts, rows) and `src/game/story.[ch]` +
`src/game/cutscene.[ch]` (the runtime). Nothing from `Assets.dat` or the voice files is committed.

## 1. How the original does it (RE)

**Globals**

| Global | Meaning | Values / notes |
|---|---|---|
| g0 | mode | 0 Time Attack, 1 Classic, 2 Adventure (the port's `fe_mode`) |
| g1 | character | 0 Lilac, 2 Carol, 3 Carol on the bike, 4 Milla |
| g4 | scene inside a story room | — |
| MenuSequence.I | stage just finished | 0 AT, 1 DV, 2 RM, 3 FN, 4 SB, 5 JC, 6 TB/TH, 7-8 PL/... |

Frame index = json number − 1. A "room" below is the json number (`st<json>` in the port).

**New Adventure.** The hub entry 'c1' starts a new Adventure. If records x2 y42 < 2 it goes straight to frame index 81
(json 82) with g4 = 2. Once achievement 42 is held (y42 ≥ 2), it goes through character select (frame 6) first, and
character select (RE 800) sends G0 > 1 to frame 81 with g4 = 2.

**Stage ends.** Frame 8 (handle_frame_9) routes a finished stage in Adventure by MenuSequence.I and g1:

| I | stage ended | target (frame index) | g4 |
|---|---|---|---|
| 0 | AT | 76 | 4 |
| 1 | DV | 75 | 1 |
| 2 | RM | 76 | g1 0: 2; g1 2 / 4: 3 |
| 3 | FN | 78 | 1 |
| 4 | SB | 36 (SB2) | 99 |
| 5 | JC | 82 | 13 |
| 6 | — | 80 | g1 0: 0; else 1 |
| 7 | — | 82 | g1 0: 20; else 13 |
| 8 | — | 82 | g1 0: 20; else 16 |
| 9-12 | — | 65 / 67 / 69 / 71 | — |
| 13 | — | 83 (the ending) | — |

**Scene chaining.** Inside a story room, each scene sets the next frame itself: another room, or the next stage.

**Developer list.** CutsceneList_42 is the original's developer scene list: 37 entries plus "Ending". Its debug events
263-269 map selection K to frame/g4 pairs, and those pairs are cited per scene below.

**Dialogue.** The dialogue lives in `DialogData_<handle>` Text objects, one paragraph per line, in constructor order. The
format is `"SS AA voice text"`:

- SS is the speaker, which is the DialogBox2 name-label frame: 0 ?????, 1 Lilac, 2 Carol, 3 Milla, 4 Spade, 5 Torque,
  6 Serpentine, 7 Gong, 8 Neera, 9 Magister, 10 Zao, 11 Dail, 12 Brevon, 13 Syntax, 14 King.
- AA is the speaker actor's animation. It can be `xx`.
- `voice` is `Data/voices/eng/<xx>/<id>.ogg`. A voice of `xx_xx_xx` marks a continuation with no clip.

There are no portraits. DialogBox_551's alterable E is the paragraph index, counting every paragraph. RV confirms this:
DialogData_3923 E 76 is go_01_04, "It's an ambush!". The extractor therefore keeps the `xx`-anim paragraphs (998 lines in
21 objects).

**DialogBox UI** (RE events 18606 / 18607 / 18391 / 18392 / 18414; layout RV):

- C (typed count) goes up by 1 per frame, and C + 2 characters are shown.
- Box at (213, 8); the name label at (57, 71).
- Text at (65, 8): 32 × 3 cells of font.bmp, 8 × 20.
- The arrow sits at (345, 40) and shows 9 frames after a line starts. A with the arrow shown advances, and the next line
  appears 2 frames after the press.
- Every DialogBox object is on json layers 6/7 and does not scroll.

**Pause in story rooms** (RV): the GUIPause frames are:

- Frame 1: RESUME / SKIP SCENE / QUIT.
- Frame 2: RESUME / SKIP SCENE / NEXT STAGE.
- Frame 81 (the opening) has no menu. START skips at once, and A does nothing.

A confirmed row plays sound 56; the frame changes 51 frames later.

## 2. Scene inventory

The inventory is `tables/story/scenes.json`: 39 entries, all 37 CutsceneList scenes plus the Adventure opening and
part 2 of Crash Site.

- **Room** is the json frame number.
- **Trigger** is `new` (a new Adventure), `end:I` (frame 8 after stage I), or `(chain)` (started by the previous
  scene's `next`).
- **chr** is the g1 set the scene plays for: L / C / M.
- **next ; skip ; panel** are the step after the scene, the SKIP SCENE target, and the pause menu (0 none, 1 frame 1,
  2 frame 2).
- **Lines** are the DialogData paragraph range. "clips" counts the voiced lines, and the voice prefixes name the
  speakers (li Lilac, ca Carol, mi Milla, to Torque, se Serpentine, go Gong, ne Neera ...).

Each entry in scenes.json carries `evidence` (per field, RE / RV / INFERRED) and `missing` text.

| id | list | title | room / g4 | trigger | chr | next ; skip ; panel | dialog: lines (voices) | status |
|---|---|---|---|---|---|---|---|---|
| adv_opening |  | (Adventure opening: Brevon's ship crashes in the snowfields; 'Three days later, In a valley to the East...') | 82 / 2 | new | LCM | scene:dv_meet ; scene:dv_meet ; 0 | - | ported (RV) |
| dv_meet | 1 | Dragon Valley (Meet Lilac and Carol) | 76 / 0 | (chain) | LCM | stage:dv1 ; mark:chsel ; 1 | 3903: 0-11 (12 clips: ca li) | ported (RV) |
| dv_crash | 2 | Crash Site (Meet Serpentine and Torque) | 76 / 1 | end:1 | LCM | scene:dv_crash2 ; scene:dv_crash2 ; 2 | 3903: 13-41 (29 clips: ca li se to) | ported (RV) |
| dv_crash2 | 2 | Crash Site (Meet Serpentine and Torque), part 2 (the forest: Torque's request) | 15 / 11 | (chain) | LCM | scene:rm_gong ; scene:rm_gong ; 2 | 550: 0-13 (14 clips: ca li to) | ported (RV) |
| rm_gong | 3 | Relic Maze (Meet Gong and Neera) | 77 / 1 | (chain) | LCM | stage:rm1 ; stage:rm1 ; 2 | 3923: 8-25 +76 (19 clips: ca go li ne) | ported (RV) |
| rm_hideout | 4 | Relic Maze (Milla's Hideout) | 77 / 4 | end:0 | M | route ; - ; 2 | 3923: 0-6 (6 clips: mi) | inventory |
| rm_collapse | 5 | Relic Maze (Cave Collapse) | 77 / 3 | end:2 | CM | route ; - ; 2 | 3923: 70-74 (5 clips: ca li mi) | inventory |
| st_bad_news | 6 | Shang Tu Royal Palace (Gong's Bad News) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 0-12 (13 clips: go ma ne) | inventory |
| rm_meet_milla | 7 | Relic Maze (Meet Milla) | 77 / 2 | end:2 | L | route ; - ; 2 | 3923: 26-69 (43 clips: ca li mi) | inventory |
| th_torque_revealed | 8 | Lilac's Treehouse (Torque Revealed) | 16 | (chain) | LCM | route ; - ; 2 | 622: 32-98 (62 clips: ca li mi to) | inventory |
| sg_brevon_plan | 9 | Shuigang Palace (Brevon's Plan) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 13-30 (18 clips: br da sx) | inventory |
| th_next_morning | 10 | Lilac's Treehouse (The Next Morning) | 15 | (chain) | LCM | route ; - ; 2 | 550: 21-35 (15 clips: ca go li mi to) | inventory |
| np_gong_help | 11 | Northern Plateau (Gong's Help) | 78 | (chain) | LCM | route ; - ; 2 | 3948: 0-21 (22 clips: ca go li mi to) | inventory |
| sm_spade | 12 | Shang Mu (Spade Encounter) | 79 | (chain) | LCM | route ; - ; 2 | 3951: 0-19 (20 clips: ca li mi sp to) | inventory |
| fn_panther_defeated | 13 | Fortune Night (Robopanther Defeated) | 79 / 1 | end:3 | LCM | route ; - ; 2 | 3951: 20-53 (34 clips: ca li mi se to za) | inventory |
| sm_zao_dinner | 14 | Shang Mu City Hall (Zao's Dinner) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 46-67 (22 clips: ca li mi to za) | inventory |
| sg_intruders | 15 | Shuigang Palace (Brevon Thwarts the Intruders) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 68-81 (13 clips: br se sx wa) | inventory |
| sm_zao_help | 16 | Shang Mu City Hall (Zao's Help) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 82-94 (12 clips: li mi to za) | inventory |
| zao_airship | 17 | Zao's Airship | 79 | (chain) | LCM | route ; - ; 2 | 3951: 58-62 (5 clips: ca mi za) | inventory |
| zao_airship_ambush | 18 | Zao's Airship (Ambushed) | 37 / 99 | end:4 | LCM | route ; - ; 2 | 4026: 95-96 (2 clips: ca to) | inventory |
| jiang_river | 19 | Jiang River (Route to Shang Tu) | 80 | (chain) | LCM | route ; - ; 2 | 3965: 0-33 (33 clips: ca li mi to) | inventory |
| st_magister_talk | 20 | Shang Tu Royal Palace (Talk With Magister) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 97-132 (35 clips: ca go li ma ne to) | inventory |
| jail_plan | 21 | Royal Palace Jail (Lilac's Plan) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 133-149 (17 clips: ca li mi) | inventory |
| jail_fail | 22 | Royal Palace Jail (Escape Plain Fail) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 150-160 (11 clips: ca li mi ne) | inventory |
| throne_apology | 23 | Royal Palace Throne Room (Lilac's Apology) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 161-171 (11 clips: li ma to) | inventory |
| sg_brevon_torque | 24 | Shuigang Palace (Brevon Discovers Torque) | 83 / 13 | end:5 | LCM | route ; - ; 2 | 4026: 176-178 (3 clips: br sx) | inventory |
| jc_cave | 25 | Jade Creek Cave (What Do We Do Now) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 179-202 (24 clips: ca li mi) | inventory |
| th_carol_help | 26 | Trap Hideout (Carol Seeks Help) | 83 | (chain) | CM | route ; - ; 2 | 4026: 203-209 (6 clips: ca mi) | inventory |
| tb_torture | 27 | Thermal Base (Torturing Lilac) | 81 / 0 | end:6 | L | route ; - ; 2 | 3971: 0-32 (31 clips: br li se sx to) | inventory |
| tb_team_spade | 28 | Thermal Base (Enter Team Spade) | 81 / 1 | end:6 | CM | route ; - ; 2 | 3971: 33-36 (4 clips: ca li mi) | inventory |
| tb_brevon | 29 | Thermal Base (Brevon Encounter) | 81 | (chain) | LCM | route ; - ; 2 | 3971: 37-48 (12 clips: br ca mi sx to) | inventory |
| tb_arrest | 30 | Outside Thermal Base (Lilac's Arrest) | 83 / 20 | end:7, end:8 | L | route ; - ; 2 | 4026: 210-216 (7 clips: li ne) | inventory |
| tb_outside_team | 31 | Outside Thermal Base (Carol, Milla and Torque) | 83 / 16 | end:7, end:8 | CM | route ; - ; 2 | 4026: 256-274 (19 clips: br ca da go mi se sx to) | inventory |
| st_mission | 32 | Shang Tu Royal Palace (Magister's Mission) | 83 | (chain) | LCM | route ; - ; 2 | 4026: 217-232 (16 clips: li ma ne) | inventory |
| pl_final_message | 33 | Pangu Lagoon (The Final Message) | 83 | (chain) | L | route ; - ; 2 | 4026: 233-255 (20 clips: li ma sc) | inventory |
| sf_army | 34 | Snowfields (Army of Shang Tu) | 82 | (chain) | LCM | route ; - ; 2 | 3989: 0-24 (25 clips: ca go mi to za) | inventory |
| sf_camp | 35 | Snowfield Encampment (Prepare For Final Batle) | 82 | (chain) | LCM | route ; - ; 2 | 3989: 25-80 (55 clips: ca go li mi to za) | inventory |
| ending | 36 | Ending | 82 | end:13 | LCM | menu ; - ; 2 | 3989: 81-108 (28 clips: br ca da li mi sx to) | inventory |
| game_intro | 0 | Game Intro (Meet Brevon) | 83 | (chain) | LCM | menu ; - ; 0 | 4026: 31-45 (15 clips: br da ki ma) | inventory |

**Labels.**

- **Ported scenes:** every field is RE or RV. They were traced end to end for Lilac, Carol and Milla, and the voice
  sequences, actors and timings are identical for all three (RV a75_g1_carol / a75_g1_milla).
- **Inventory rows:** the trigger and g4 are RE where frame 8 or CutsceneList names them. The line ranges of untraced
  scenes are **INFERRED** from content, and the room comes from the DialogData object's frame.

**Character variants** (RE frame 8, with chr masks in the table):

| Stage end | g1 = Lilac | g1 = Carol / Milla |
|---|---|---|
| RM end (I 2) | Meet Milla (76 g4 2) | Cave Collapse (76 g4 3) |
| AT end (I 0) | — | Milla's Hideout (76 g4 4); AT is Milla's first stage |
| I 6 | Torturing Lilac (80 g4 0) | Enter Team Spade (80 g4 1) |
| I 7 / 8 | Lilac's Arrest (82 g4 20) | the team's scenes (82 g4 13 / 16) |

The first arc has no variant: g1 only changes the stages.

**Music and sounds of the ported scenes** (RV, in the scripts as `u` / `s` / `o`):

| Scene | Music | Sound ids | Voices outside the dialogue |
|---|---|---|---|
| adv_opening | m_brevon2 from t 1029 | 222 354 390 435 | brevon_bigno |
| dv_meet | m_rescue | 16 ids | ca_01_05 |
| dv_crash | m_trouble | 19 ids | — |
| dv_crash2 | m_hub1 | 404 6 | — |
| rm_gong | m_outdoor, m_chase at t 120, m_outdoor at 132 | 10 ids | ca_03_04, li_03_04 |

**Saves.** No scene writes the save itself. The port stores the story position at each scene completion when the file
is in use (section 5).

**Missing evidence / assets.**

- 34 scenes are not traced; their `missing` field says "phase 2". The `s76_g2` trace (Meet Milla) failed to start.
- In-stage scenes (zao_airship_ambush in SB2 with g4 99; RM5's Adventure dialog DialogData_1624) belong to the stage
  modules.
- game_intro is the boot attract sequence, outside the Adventure gate, and is not ported.
- The ending already plays through `ending.c` in every story mode. Its Adventure epilogue rooms (81 / 82) are phase 2.
- dv_meet lines 0-6 and the easter-egg line 42 (li_01b05, RE 18414, a held input) never appear on the RV route.
  dv_crash2 line 20 (li_02b10) is an unused alternate take.
- The Linux install's voice clips are 3.5 KB placeholders. The Windows install has the real 1,373 clips. The builder
  converts whatever real clips it finds, and every other line plays text-only.

## 3. Runtime (src/game/story.c, cutscene.c)

**story.c** holds the gate, the story state, the triggers and `MODE_STORY`.

- **Starting a scene:** `csx_load("story/<id>.scn")`, then `game_load_level("st<room>")`, which loads the story room as
  a level with its tiles, parallax and the `_ens` bank holding the actor rows. It then sets the scene's sound set, loads
  `ui/font` and `story/dlg_<handle>.txt`, and calls `csx_begin`. Each scene loads its own assets; nothing is resident
  between scenes.
- **During a scene:** level objects are not drawn or updated. Actors are drawn in three passes (behind the play layer,
  with it, in front), and the json layers 6-7 are drawn screen-fixed.
- **Completion** is `complete(how)`: DONE_END, DONE_SKIP or DONE_STAGE. A done flag makes it run exactly once. It:
  1. stops voices and music;
  2. sets the seen flag;
  3. sets the position to id + 1;
  4. writes the file (if one is in use);
  5. takes the next step: `scene:<id>`, `stage:<dir>` (the run's first stage via `game_start_adventure`, later ones via
     `game_start_stage`), `route` (`level_route_next`), or `menu`.
- **Missing script:** a scene whose script is missing is marked seen and skipped.
- **In-scene character select** (dv_meet, RV): the script's `chsel` entry locks the slot / Lilac / Carol actors.
  Left / Right moves the selection (sound 64), the slot blinks (characterslot_3911 dir 1), and confirm sets
  `G.character` and plays 63 + 220 / 221 with portrait anim 7. The rest of the segment then plays: wipe 56, DV1.

**cutscene.c** is the scene-script player, an extension of the session-24 template, which is kept.

- Up to 128 actors (rows `s<handle>_<anim>_<dir>` in the room's `_ens` bank) with their animators, plus moves, the
  camera, parallax visibility (`pv`) and fades (black or white).
- Captions come from `story/captions.txt`, centred, with rows 20 px apart (RV).
- The DialogBox uses the original rows (`dlgbox` / `dlgname` / `dlgarrow`) and the original font.
- Ink comes from RE: `add` for set_shader 0x20, `a<alpha>` for the blend colour.
- **Segments:** a segment ends where the reader dismissed a line in the RV trace. Pressing A while a segment still runs
  flushes it (positions and images jump to its end state; no sounds or lines). A guard ends a scene 600 frames after its
  last entry if no line is showing.
- **Voices:** one clip per line, `story/vo/<voice>.wav` (mono ADPCM, 11,025 Hz, or lower for clips over 65,000
  samples). There are two slots: the current line's clip, and the next line's clip preloaded. A missing clip means the
  line is text-only.

## 4. Data format (adding a scene is mostly data)

The authoritative format is the header comment of `src/game/story.h`. In short:

- `story/story.txt` has one line per scene:
  `<id> <json room> <g4> <next> [skip=] [stage=] [panel=] [trig=new|end:I] [chr=mask]`.
  It is written by `tools/import_story.py` from scenes.json, and only for scenes that have a script.
- `story/<id>.scn`:
  - The header has `scene`, `room`, `dialog` and `title`, plus `actor <n> <class>_<handle> <layer> [ink]` lines.
  - Then come `seg <k>` blocks of timed entries:
    `<t> p|m|i|v|c|say|close|s|o|u|f|chsel|cap|capx|pv ...`, ending with `end <t>`.
- `story/dlg_<handle>.txt` holds the paragraphs. `story/captions.txt` holds the caption strings. `st<json>/` is the
  room.

**To add a scene** (phase 2):

1. Trace it from the original with `tools/orig/story_rv.sh room:<frame index>:<g4>:<g1> <log> <frames>`, using
   STORY_READ=30 and STORY_IDLE=600.
2. Convert it with `tools/story_rv2scn.py <log> <id> <json> <dialog handle|-> <first> <end> tables/story/scripts/<id>.scn`.
   The options are `--next`, `--chsel`, `--fadeout`, `--flash`, `--caption`, `--drop` and `--rows`. Add the command to
   a per-arc script like `tools/story_arc1.sh`.
3. Set the scene's fields in `tables/story/scenes.json` (trigger, next, skip, stage, panel, chr, evidence, missing).
4. Run `python3 tools/import_story.py <data dir>`, then `convert_audio.py story ...`. The builder runs both.

Unless a scene has in-stage logic, no C changes are needed.

## 5. Gating, state, save

`story_gate()` is the only check: `S.run && fe_mode == 2 && !G.time_attack && G.file_mode == 0`. `S.run` is set only by
`story_new_game` (hub NEW ADVENTURE, or character select with fe_mode 2) and `story_continue` (CONTINUE on an Adventure
file). `story_reset()` clears everything, and it is called at every entry point that leaves or starts something else:

- `fe_enter`, `fe_enter_from_stage` (menu, quit, game over);
- the hub's Time Attack exit and NEW CLASSIC;
- CONTINUE, before its own check;
- the developer select;
- every failed step.

Stage ends reach the story only through `story_stage_end(stage)`, which returns 0 unless the gate is open. Its callers
are `results.c` (the results popup's next stage; never in Time Attack) and `fe_gallery.inc` (the achievement popup).
Restarts and act changes inside a stage never call it, so a Classic or TA run cannot start a scene, and no Adventure
state survives a return to the menu.

**Story state** is the run's story position (the next scene id) plus 64 seen flags.

**Save.** The save is now v5 (`save.c`): one story byte per file and 8 bytes of seen flags, 1,377 bytes in total, still
6 VMU blocks. v3 and v4 saves load with story = 0 / no flags (tests/test_saves.py). The position is written:

- at every scene completion of a used file;
- at the stage-end save (`game_save_stage_end`).

CONTINUE resumes through frame 8 with the file's last stage.

**Pause** (MODE_STORY):

- **Opening:** START plays sound 63, pauses the music, stops all sounds and opens GUIPause frame 1 or 2.
- **Rows:** Up / Down move between rows (sound 64). Resume is START, or A on RESUME, and the scene continues where it
  stopped.
- **SKIP SCENE / NEXT STAGE:** sound 56 and a 50-frame wipe, then `complete(DONE_SKIP)` (next = the skip target) or
  `complete(DONE_STAGE)` (the `stage=` target). Both stop voices and music and set the flags exactly once; a second press
  or the scene's own end during the wipe is ignored.
- **In dv_meet**, SKIP SCENE jumps straight to the character select, with no wipe and no completion (RV k75_g0_skip700).
- **QUIT** resets the story and returns to the title (RV: frame 3, g0 0).
- **In the opening (panel 0)**, START skips at once (RV k81_g2_start).

## 6. Budgets (measured)

**VRAM** (textures one room loads for each character; FN3, a shipped act, is the largest):

| Room | Lilac | Carol | Milla |
|---|---|---|---|
| st82 | 4.42 MB | 4.93 MB | 4.77 MB |
| st76 | 4.74 MB | 5.26 MB | 5.10 MB |
| st77 | 4.15 MB | 4.67 MB | 4.50 MB |
| st15 | 3.71 MB | 4.23 MB | 4.06 MB |
| DV2 / DV4 (shipped) | — | 5.0 MB | — |
| FN3 (shipped) | — | 5.77 MB | — |

The rooms keep the character banks: they stay resident into the next stage, and every room is under acts that already
ship. ui/font adds 32 KB.

**Sound RAM** (per scene): the character group, plus the scene's sound set, plus two voice slots. The scene sets are:

| Scene | Sound set |
|---|---|
| dv_crash (largest) | 302 KB, 22 ids |
| dv_meet | 299 KB |
| rm_gong | 137 KB |
| adv_opening | 97 KB |
| dv_crash2 | 67 KB |

The two voice slots peak at 56 KB. The largest clip is 31 KB (to_01_11, 5.7 s).

**Disc** (arc 1, packed as the disc packs it): 6.35 MB in total: four rooms of 1.2-1.5 MB each, `story.pak` 162 KB, and
65 loose voice clips (0.74 MB). Extrapolated to every scene: about 10 rooms (≈ 13 MB) plus ≈ 15 MB of voices
(INFERRED).

**Voice loading** reads one loose clip per line from the disc, about 11-31 KB, preloaded one line ahead. A GD-ROM seek
may cost a frame hitch at a line change (INFERRED, not measured on hardware).

## 7. Known differences (arc 1)

| Difference | Cause / label |
|---|---|
| Comets / pods trail: some runtime-spawned explosion effects are off-view in the trace and are left out | RV view filter |
| The crash's ground smoke is opaque white in the port; the original's is translucent | INFERRED: semi-alpha art in the room's level pieces, which the level pipeline flattens |
| The white flash after the crash has its timing from screenshots | INFERRED: white from t 920, 120 frames held, a 140-frame fade |
| Falling leaves (effectfallingleaf) are not played | INFERRED: random sine paths, 36k script entries a scene |
| The fade-in ramp at scene starts is linear | INFERRED: LevelCardFade2's alpha is not traced |

## 8. Tests

`tests/test_adventure.py` (host build, data dir argument) covers:

- **Inventory:** unique ids, the 37 CutsceneList entries, 998 dialogue lines, the captions, every script line inside its
  scene's range, and the rows.
- **The arc:** a new Adventure (ADVNEW and the real menus) → opening → dv_meet → character select (Lilac, Carol) → DV1.
  The DV end chain → dv_crash → dv_crash2 → rm_gong → RM1 runs each scene once, for g1 0 / 2 / 4.
- **Gating:** a Classic stage end, a Time Attack stage end, a stale fe_mode 2 with no run, Classic and TA from the
  menus, a Classic restart then stage end, and an Adventure QUIT → Classic → stage end. None of them starts a scene.
- **Pause:** resume continues; SKIP SCENE completes once and goes to the next scene with the audio stopped and the seen
  flag set; NEXT STAGE goes to RM1; dv_meet SKIP goes to its character select; START in the opening skips; double
  confirms complete only once.
- **Save:** the position and seen flags are written, read back, and the file is v5 / 1,377 bytes.

Text-only play (no `story/vo`) ran the whole chain with the same completions.

## 9. Phase 2: proposed split of the remaining arcs

Every group follows section 4. The shared files are `tables/story/scenes.json` (each group edits only its own entries)
and `tables/story/rows.json` (`story_rv2scn.py` rewrites only its own scene's key). Each group gets its own arc script
`tools/story_arc<N>.sh` and its own `tables/story/scripts/<id>.scn`, and extends `tests/test_adventure.py` with the
group's chain. One prerequisite: the routing after a stage-end scene (`next route`) and the scenes' chaining need the
RV traces of each group. Several `next` values are `route` placeholders today.

| Group | Scenes (ids) | Rooms (json) | Notes / extra files |
|---|---|---|---|
| A: Relic Maze aftermath | rm_hideout, rm_meet_milla, rm_collapse | 77 | character variants (end:0 Milla; end:2 Lilac vs Carol / Milla); RM5's in-stage Adventure dialog (DialogData_1624) touches the RM stage module |
| B: Treehouse to Fortune Night | th_torque_revealed, th_next_morning, np_gong_help, sm_spade, fn_panther_defeated, zao_airship | 15, 16, 78, 79 | room 15 is shared with dv_crash2 (parallax `pv` per scene) |
| C1: Shang Tu / Shuigang / Shang Mu (before Jade Creek) | st_bad_news, sg_brevon_plan, sm_zao_dinner, sg_intruders, sm_zao_help, st_magister_talk | 83 | DialogData_4026 |
| C2: Shang Tu after Jade Creek | jail_plan, jail_fail, throne_apology, sg_brevon_torque, jc_cave, th_carol_help, tb_arrest, tb_outside_team, st_mission, pl_final_message | 83 | end:5 / end:7 / end:8 variants; DialogData_4026 |
| D: Jiang River and Thermal Base | jiang_river, tb_torture, tb_team_spade, tb_brevon, zao_airship_ambush | 80, 81, (SB2 g4 99) | the ambush is an in-stage scene of SB2 (stage module); end:6 variants |
| E: Final battle and epilogue | sf_army, sf_camp, ending | 82 (+ 81 / 82 epilogue) | room 82 is shared with adv_opening; the epilogue goes before / around `ending.c` (end:13) |
