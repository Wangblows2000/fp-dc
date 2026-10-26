# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Rule: evidence from the running original first, then match it. Label evidence RE (read from code) / RV (original trace) / INFERRED; host tests and emulator runs are labelled as such. **Nothing is hardware-validated.**

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0). **environ.sh patch**: `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before environ_base.sh.
- mkdcdisc: third_party/mkdcdisc/builddir/mkdcdisc. Flycast: third_party/flycast/build/flycast, driven by `tools/flycast_run.sh` (x=A c=B s=X Return=Start, f/v = L/R; developer select = `key:f key:v` on the title).
- Original game: writable copy /home/claude/fprun (never the user's folder). /home/claude/fprun_user = the same game
  with the user's PC saves (Carol unlocked); for traces and reference only, never shipped or read by the port.
  Carol traces: `CHAR=2` (on foot) / bike via the fuel can; scripts tests/orig_carol, CSVs build/orig_traces/carol.
  Milla traces: `CHAR=4` (any frame; `FRAME=16` = Aqua Tunnel 1 for the dig); `<f> SETV others E 9999` makes her
  invulnerable (the host understands the same line). `tools/orig/start_orig.sh` → `tools/orig/route_dv1.sh` → `FRAME=<20 dv1 / 21 dv2 / 22 dv3 / 23 dv4> tools/orig/run_trace.sh <script> <out.csv> <frames>`. `XVT_EXTRA="name=0x<vtable+0x10>#<instance>"` logs extra objects. A teleport far from the start can be blocked by the camera bounds (sensors are clamped to them).
- Trace scripts: `<f> TP x y`, `<f> EN v`, `<f> HP v`, `<a> <b> KEYS` (a 1-frame range sends nothing; use ≥ 2 frames).
- Host-only debug script commands: `<f> MON <type>` (shield monitor ahead of Lilac), `<f> HURT <dx>`, `<f> BOSSHP v` (AT boss HP),
  `<f> LOAD <dir>` (start another level in the same run). Env: `FP_CARD_DBG`, `FP_DUMP_OBJDEFS`, `FP_TA`, `FP_FILE`, `FP_FILE_NEW`.
- Baseline comparisons: the trace comparers run the checkout they live in, so a `git worktree` of an older commit (with
  its own converted2 for changed levels) gives a per-trace baseline (session 13 used /home/claude/fpdc_head).
- Aqua Tunnel traces: `FRAME=16..19 CHAR=4`; run_trace.sh clears the AT checkpoint globals (`CP=keep` keeps them).
- User PC: `C:\Users\wobbles\Desktop\Freedom Planet` (read-only input); deliverables go to its `fpdc\` subfolder.

## Pipeline
See ASSET_PIPELINE.md (levels, UI, font, audio). Then:
```sh
for l in "21 dv1" "22 dv2" "23 dv3" "24 dv4" "17 at1" "18 at2" "19 at3" "20 at4" "25 rm1" "26 rm2" "27 rm3" "28 rm4" "29 rm5"; do set -- $l; tools/build_level.sh $1 $2 converted2; done
make -C host && for t in gameplay frontend carol milla aqua levels saves rm_gimmicks_b rm_claw rm_mantalith; do python3 tests/test_$t.py; done
Q=1 tools/orig/cmp_all.sh && python3 tools/orig/cmp_carol.py && FR=295 python3 tools/orig/cmp_milla.py && python3 tools/orig/cmp_at.py   # against the original's traces
source /opt/toolchains/dc/kos/environ.sh && make && make cdi        # build/fpdc.cdi (~300 MB)
make cdi DEMO=tests/play/<route>.txt CDI=build/fpdc_demo.cdi        # verification disc: scripted input ("LEVEL dv1" line starts the act)
```
Route search (host): `FP_SEARCH=1 FP_EXPLORE=4000 FP_SEARCH_OUT=out.txt FP_LEVEL=dv1 host/fpdc_host converted2 N script /dev/null 1`; `FP_SEARCH_FROM=<frame>` replays the script before that frame. Run searches from a copy of the binary.

## Session 8 (2026-10-23/24): Dragon Valley completion
Done (details in RE_NOTES "Session 8 additions" and "Shields and water", RUNTIME_MAP "Session 8"):
- Bosses/minibosses: Wretchnid (DV1), Tredd (DV3), Serpentine DV2 encounter and DV4 boss, DV4 boulder chase, DV4 results.
- Traversal: roll M6, spiral helix, DV3 ball tunnel, spinning logs, pushable block, corkscrews, loops; keystones; cards.
- Water (DV2/DV3) with swimming, air meter, drowning, bubble patches; elemental shields (5 types).
- Parallax as 4 bpp chunks; translucent/additive sprites through the PVR translucent list; second atlas for big _ens banks.
- Brief items from earlier in the session: 16:9 letterbox option (save v2), title credit, momentum change, card totals.
- Death restarts the act with one life less; game over returns to the menu.

## Session 9 (2026-10-24): rendering order, DV4 boss exit, audio, performance
Brief order: transparency, DV4 boss sequence, audio, textures/performance. Details: BUILD_AND_TEST_LOG "Session 9",
RE_NOTES "Session 9", DECISIONS D-033 to D-035.
- **Transparency / draw order (PVR):** translucent, additive, tint and invert sprites are deferred to the translucent
  list; they used to be replayed with the depth of the end of the frame, so waterfalls, water strips and tints covered
  the foreground and the HUD. Each deferred command now keeps the depth of its draw call (one fix for every level).
  Waterfall fronts are additive (RE create_waterfallfront_1037 set_shader 0x20); backs use blend alpha 0x9c (RE).
- **DV4 boss defeat:** the cinematic camera (5547) no longer runs the normal camera rules, so the view stays on the
  surface; Lilac rises as the boost ball (5567, angle/state reset so the bowl ceiling cannot hold her), resumes at
  y < 1230 (5570), lands, results tally, menu. Host test `dv4_exit_*`. Not traced in the original (RE spec §9-10 only).
- **Audio:** BGM loop seam sealed (D-033; the left-channel blow-up ~25 s into invincibility); monitors play 307 + 12,
  rocks 13 instead of Lilac's hurt sound 81 (Lilac's own hurt sounds unchanged).
- **Performance:** sensor candidate lists (D-034), hot obj_t fields, second piece atlas ≤ 1024x512 (D-035), DC frame
  profiler in the perf log. Flycast: every level at 602-603 vblanks per 600 frames (was 730 in the DV3 demo).

## Session 10 (2026-10-24): Carol, DV2 plate, boss HUD
Brief order: DV2 physics puzzle, boss HUD, full Carol, Carol performance, testing/docs. Details: RE_NOTES "Session 10"
and "PC save files", RUNTIME_MAP "Session 10", ASSET_PIPELINE "Session 10", DECISIONS D-036 to D-039,
BUILD_AND_TEST_LOG "Session 10".
- **DV2 plate:** a resting pushable block presses the plate (4971; reach 32 px above, D-036), the door sinks (RV).
- **Boss HUD:** the unused guiboss bar is gone; the hit tally sits bottom right.
- **Carol (playable on character select, Time Attack and files):** her own movement table, pounce, Wild Claw combo
  and big slash, Wild Kick, roll and ball jump, wall cling/jump, voices and sounds, tail overlay, VMU life icon.
  Bike from fuel cans: engine loop, air jump, nitro, ramming, wall ride, dismount/parked bike/remount, 3-hit wreck,
  exhaust. Red jump pads in DV2-DV4 (Lilac gets crystals from them, as in the original). 53 original traces.
- **Audio:** per-character sound groups at level load (D-037): the full set had overflowed AICA RAM in Flycast.
- **VRAM:** split atlases (D-038) for Carol's banks and the DV1-DV3 object banks.

## Session 11 (2026-10-24): Milla, 4 bpp overlays
Details: RE_NOTES "Session 11", RUNTIME_MAP "Session 11", ASSET_PIPELINE "Session 11", DECISIONS D-040 to D-044,
BUILD_AND_TEST_LOG "Session 11". Specs (build/re, not committed): spec_milla_moves.md, spec_milla_combat.md.
- **Milla (playable on character select, Time Attack and files):** Lilac's movement table, Puppy Float, Phantom Cube
  (charge, carry, throw, burst), Barrier Shield, Shield Burst, Super Shield beam with its recoil, dig state (zones only),
  voices, HUD panel, life / VMU icon, sounds as their own group. 41 original traces.
- **Differences between the brief and the original (v1.21.5, RE + RV):** the shield neither blocks nor reflects
  projectiles; the "Cube Blaster" is the thrown cube; DV has no dig zones; the cube does not press the DV2 plate (only
  pushables, the bike and the player's foot sensor do).
- **VRAM:** 4 bpp overlays and split atlases for Lilac's banks (+300 KB free with Lilac in DV1).

## Session 12 (2026-10-24): Aqua Tunnel
Brief: port Aqua Tunnel (frames 16-19, frame_17..20.json, at1..at4) end to end for Milla. Details: RE_NOTES
"Session 12", RUNTIME_MAP "Session 12", ASSET_PIPELINE "Session 12", DECISIONS D-045 to D-051, BUILD_AND_TEST_LOG
"Session 12". Specs (build/re, not committed): at_inventory.md, spec_at_flow.md, spec_at_enemies.md,
spec_at_gimmicks_a.md, spec_at_gimmicks_b.md, spec_ironworm.md, spec_chomper.md, spec_chompersnake.md,
spec_milla_monitor_carry.md.
- **Routes (D-046, reverses D-040):** only Milla reaches Aqua Tunnel: Classic / Adventure / Continue start at AT1, and
  her Time Attack shows the AT icon in Dragon Valley's slot. Lilac and Carol keep DV (host-tested through every AT act).
- **Stage:** the four acts with their parallax, water, music per checkpoint, act transitions (walk-in from x -32, AT3
  top exit, AT4 through the results and raft exit), checkpoints, camera bounds and boss locks.
- **Objects:** fireflies, slimes, whisps, aqua bricks, bomb / crate monitors, water jets and current, jelly and red
  springs, dig spots, every gimmick of spec_at_gimmicks_b (platforms, plates / doors / gate, Milla block, wall plates +
  pipes, beam, crushers, axels, swings, slime floor, rafts), the Iron Worm, the Chomper and the Chomper Snake.
- **Milla:** monitor lifting / carrying / throwing (D-051), dig rewards live in AT1.
- Traces: tests/orig_at (37 scripts), `python3 tools/orig/cmp_at.py`; host tests tests/test_aqua.py.

## Session 13 (2026-10-25): bug fixes and reusable level systems
Brief: Milla's hang + AT results backdrop, title cards, Time Attack previews, DV4 boss counter, generalised objects,
performance / assets / VMU saves, docs. Details: RE_NOTES "Session 13" and "Reusable objects and levels", RUNTIME_MAP
"Session 13", ASSET_PIPELINE "Session 13" (with the "Adding a level" checklist), DECISIONS D-052 to D-059,
BUILD_AND_TEST_LOG "Session 13", OBJECTS.md. Specs (build/re, not committed): spec_stage_cards.md,
spec_records_saves.md.
- **Fixed:** Milla's hang uses her hang animation; springs keep the facing; AT4 results show their panels; every level
  shows its own title card (also after restarts, act changes and in any load order); the Time Attack preview shows one
  stage with padlocks only on locked slots (and the menus' checker column and translucent bar now match the original);
  the DV4 boss hit tally is gone.
- **Generalised:** src/game/levels.c (all level parameters and the stage flow), engine type per Fusion object
  (class + handle), src/game/switches.c (plates / Milla block / doors), src/game/objdefs.c (object registry),
  presence-driven passes; no shared code tests a level name. Rule for new levels: RE_NOTES "Reusable objects and levels".
- **Saves:** v3, stage-indexed (records in ms for every stage, files with per-stage blocks, progress, 100 cards), one
  write at each stage end; Aqua Tunnel cards are collected and saved.

## Session 14 (2026-10-25): Relic Maze
Brief: RM1-RM5 for Lilac, Carol and Milla (DV → RM, AT → RM), every gimmick, enemy and boss, Milla's mine-cart route,
cards / checkpoints / results / music / SFX, Time Attack entry, saves; keep hardware compatibility. Details: RE_NOTES
"Session 14", RUNTIME_MAP "Session 14", ASSET_PIPELINE "Session 14", DECISIONS D-061 to D-067, BUILD_AND_TEST_LOG
"Session 14", OBJECTS.md. Specs (build/re, not committed): spec_rm_flow, spec_rm_enemies, spec_rm_gimmicks_a / _b,
spec_rm_golems, spec_rm_claw, spec_rm_mantalith, rm_inventory.
- **Flow:** level rows rm1-rm5 with data-driven exits, entries, death targets, stage flags, gates, room bounds and warps
  (D-061); Classic DV4 / AT4 results continue into RM1; RM5 ends at the front end (D-064). Continue resumes a file at
  the stage after its last completed one.
- **Content:** src/game/relic.c (enemies, pots), relic_gima.c (acts 1-2), relic_gimb.c (acts 3-5, Milla's hand-car,
  RM4 gateway, RM5 light stones / spike ceiling / chair), relic_boss.c (Eye Golem RM2, Bone Golem RM5), rm_claw.c (RM3
  Claw sequence, Lilac / Carol), rm_mantalith.c (Kingdom Stone scene, Mantalith, boss results, post-results).
- **Audio / memory:** RM stage sounds at 11025 Hz (D-063) after Flycast ran out of sound RAM with Milla / Carol.
- **Performance (D-067):** sensor / enemy / AT-pass prefilters took RM4 Carol from 636-643 to 603-605 vblanks.
- **Gaps:** Carol / Milla result voices are placeholders; Adventure cutscene, unlock / achievement records and
  Mantalith sounds 147 / 214 / 215 not ported; INFERRED items in RE_NOTES. An agent published a private working page
  (spec_rm_enemies) during research; it is still in the user's artifacts (delete only if the user asks).

## Session 17 (2026-10-25/26): Fortune Night (FN1-FN6) for Lilac, Carol and Milla
Details: RE_NOTES / RUNTIME_MAP "Session 17: ..." (flow, enemies, gimmicks A / B, bosses, the Panther), DECISIONS D-079
to D-087, BUILD_AND_TEST_LOG "Session 17", ASSET_PIPELINE "Session 17". Specs (not committed): build/re/spec_fn_flow.md,
spec_fn_enemies.md, spec_fn_gimmicks_a.md, spec_fn_gimmicks_b.md, spec_fn_bosses.md (G.12 = the Panther port findings),
fn_inventory.md.
- **Route:** RM5 -> FN1 ... FN6 -> results -> front end (Sky Battalion not ported). Title cards, stage-select icon and
  Time Attack preview (bg4 strips), cards 31 / 33 / 36 / 37 / 39, music m_stage3 / m_stage3b / m_stage3c / m_midboss /
  m_trouble / m_boss, checkpoints, FN3's scripted Serpentine exit, FN6 deaths back to FN5 (flow: src/game/fn_flow.c +
  level rows).
- **Objects:** enemies (fn_enemy.c, types 512-559), acts 1-3 gimmicks (fn_gima.c, 560-679), acts 4-5 gimmicks
  (fn_gimb.c, 680-799), UFO / bulldozer / beam chase / Serpentine / samurai (fn_boss.c, 800-859), Panther FN5 chase and
  FN6 fight with Milla's plane mode (fn_panther.c, 860-899). MAX_OT / MAX_OBJS 1024 (FN4 places 873 objects).
- **Per character:** FN3 Lilac fights Serpentine; Carol / Milla fight the samurai (the flow removes the other set);
  FN6 Lilac / Carol run on the treadmill floor, Milla rides the plane.
- **Data:** colour-aware piece atlases (D-082), FN parallax in the free top of the character palette (tools/par_top.py,
  D-080), parallax images up to 32 chunks (FN2's sky had not loaded, D-086).
- **Memory:** one stage sound group per act, trailing silence cut (D-085 / D-087). Flycast: sound RAM left after the
  stage group 137-227 KB (FN3 Milla lowest), VRAM free 625 KB (FN3 Carol, lowest of any level) to 3.6 MB (FN6).
- **Perf (Flycast):** 602-603 vblanks per 600 frames in every FN act after the first window, including FN3 Milla
  pushing a wall (618 before the bomb-arming scan fix).
- **INFERRED / UNKNOWN left:** bulldozer facing, samurai attack selector (U-E1), Panther mode-2 limb pose (U-G2),
  partner assist helpers (U-G3, only the offense helper ported), the items listed in TODO "Fortune Night".

## Session 16 (2026-10-25): hardware seams, RM3 chase, petals, vignette, VMU portraits, achievements
Details: RE_NOTES / RUNTIME_MAP "Session 16", DECISIONS D-073 to D-078, BUILD_AND_TEST_LOG "Session 16". Spec:
build/re/spec_achievements.md (not committed).
- **Tile seams (hardware report):** quad edges landed exactly on PVR sample points (vertical offset 0, x1.5 scale), so
  the first texel row could be sampled from the atlas row above. Screen offsets are now 0.25 px on both axes (D-073).
  Not verified on hardware yet (Flycast never showed the seam).
- **RM3 truck and the yellow diamond:** backdrop pieces were offset by their image hotspot; Chowdren's Backdrop::draw
  adds the hotspot back before draw_image subtracts it (RE), so backdrops sit at the instance x, y. Fixes the truck
  vs its wheels (RV screenshot of the original) and its roof collision: PushableBlock3 rests on it, can be pushed onto
  the engine and wrecks it (D-074). Also moves AT columns, teeter axles and RM3/RM4 shafts by their hotspots.
- **Life petals / crystals thrown** by monitors, the wood shield, enemies and Serpentine were created on layer 0
  (behind the terrain); now the play layer.
- **RM2 basement:** black screen-edge vignette (40 % inward, alpha follows the original's depth fade). r_rect on the DC
  now honours alpha (translucent rects had been opaque) (D-075).
- **VMU screen:** the character's four file-select portraits (FileSaveSlotCharacter_109), 6 s each by the logic tick,
  in stages and on the file select; no lives count; save file unchanged (D-076).
- **Achievements:** tracked for everything reachable in the ported stages (bosses 10-17, 18 INFERRED, 19, Card
  Collector 23-25, 33-36, 38, 42, speedrunners 43-45) with the original's "Unlocked!" pop-up after the results (D-077).
- **Earth-shield magnet:** not reproduced (the pull stops on the frame the shield breaks); waiting for the user's
  details (keystones always home in, RE 2556). No change.

## Session 15 (2026-10-25): bug fixes, Gallery, unlock cheat
Brief: damage, DV/RM rising platform, Carol's icon, the two door gates, Gallery from card progression, title cheat.
Details: RE_NOTES "Session 15", RUNTIME_MAP "Session 15", ASSET_PIPELINE "Session 15: Gallery", DECISIONS D-068 to
D-072, BUILD_AND_TEST_LOG "Session 15". Spec: build/re/fe/spec_gallery.md (not committed).
- **Damage:** not a collision bug. The user played on Casual, where the original takes no damage (RE 2414). The port
  re-read the difficulty from the save each time the front end reloaded, so after quitting to the menu a Casual game
  became Normal and damage "came back". Options are now read once per boot (D-068). Damage verified for Lilac, Carol
  (on foot / bike) and Milla.
- **Rising platform (it is RM1's switch column, not Dragon Valley):** SwitchColumnFront1_1157 is an invisible
  collision box (constructor set_visible(0), RE) whose image 43 is solid green; the port drew it. Registry flag
  OD_HIDDEN (D-069), also applied to MinibossBarrier_969 (the railings are the gates' visuals).
- **Gates (RM2 entrances to the golem room and the basement):** the gate that closes behind the player after the
  walk-in was created while the port's walk-in had left the player in front of it, sealing both entrances. The gate
  now waits until the player has passed it (INFERRED, D-070).
- **Carol's icon:** HUD and VMU life icon use guiicon direction 2 (image 395) on foot as on the bike; direction 1
  (image 112) is not Carol (RE anims).
- **Gallery:** src/game/fe_gallery.inc (CARDS / ACHIEVEMENTS / DATA, card flip / play / art view, Card Collector
  pop-up), save v4 (opened cards, 56 achievement states; v1-v3 convert), test_gallery.py (D-071).
- **Cheat (stays in final builds):** on the title screen (Press Start) or the title menu, press Left, Left, Left,
  Right, Right, Right, each within 1 s of the previous one; any other button resets it. Effect: every stage's progress
  for every character (Time Attack stages), all 100 cards (face down, so the gallery opens them), achievements 1-3 (the
  type-3 cards); the save is written with the normal save_store and the 1-up jingle plays, then the screen's music
  returns. Characters are already all selectable. Entering the gallery afterwards awards the Card Collector pop-ups.
  Records, files and options are untouched (D-072).

## Aqua Tunnel: verified vs approximated
- **Matched to original traces (RV, cmp_at, Milla):** slimes and their bursts, fireflies in the current, water jet from
  the left, jelly jump / charge, red springs right / up-right / up, bomb walk, AT2 aqua brick platforms, AT1 plates +
  floor doors, arena gate, axel makers and platforms, rotating pipes (wall plate push), rafts, slime floor damage,
  collapsing platform walked onto, Iron Worm wake → emergence → spit cycle (exact), Chomper cycle and swallow (exact),
  Chomper Snake raft ride → lunge / dive / home and the bomb catch (head exact).
- **Traced with known differences:** collapsing platform triggered by a TP + jelly launch (port early); AT2 crusher
  landing 3 frames late; spinning beam and AT3 slabs (a few frames); red spring left (decay); harness artifacts
  (a1_jet_right TP into the jet, a2_reddown / a2_bomb_stand AT2 intro, a1_crate, a1_jelly_hold_right ceiling frame,
  a1_start_right push at the widened jelly).
- **RE only (no trace yet):** Chomper bites and defeat, snake hits / defeat / results / raft exit, Iron Worm defeat and
  slam, Milla block, AT3 swings / axels, beam crush, the thrown-monitor flight (traces recording), AT1 dig rewards
  except spot 0.
- **INFERRED:** rotated mask collision model, card-frame object events (only the worm chain is pre-advanced), music
  fade length at the Chomper's defeat, raft carry during the AT4 exit, draw order of slime over the snake, acid/rock
  edge latches, shade drift.
- **Not ported:** AT4 results voices and victory pose, Spade's hitboxes (not playable), the DV shade puff name bug.

## Milla: verified vs approximated
- **Matched to original traces (RV):** walking / running, jump and release cut, float (start, rise, meter, end, refloat,
  walk-off, springs incl. Down), cube charge / cancel / appear / carry / throw / burst timing, shield (walk, decay,
  tap, hold), shield burst and its two hits on the Turretus, super shield and beam incl. every recoil trace (ground,
  up, down, air, air down, wall), rock breaking by the burst and the beam, hurt through the shield, idle.
- **Small known differences:** the cube thrown at the Turretus bursts 2 frames late (1 px of held-cube height); the
  beam's first hit is 1 frame early; flap-sound cadence (frame-time timer in the original).
- **RE only:** shield on springs / ladders, dig rewards (spots of Aqua Tunnel / Relic Maze), float under water (blocked
  by the swim rule), recoil under a ceiling (no low ceiling found near the DV1 start).
- **Not ported:** fuel cans for Milla, alternative palettes, idle voice variants. (Monitor lifting and
  Aqua Tunnel: session 12.)

## Latest verified
- host-test (session 17, final code): gameplay, front end, Carol, Milla, Aqua, levels, saves, rm_gimmicks_b, rm_claw,
  rm_mantalith, gallery, achievements, fn_enemies, fn_gimmicks_a, fn_gimmicks_b, fn_bosses, fn_panther, fn_flow: all 0
  failures. Trace comparers unchanged (cmp_all 76/76 strict 11311/11377, cmp_carol 43/53, cmp_milla 37/43 = the
  session-16 code on the same data, cmp_at 21/35).
- emulator (Flycast, session 17): FN1-FN6 with Lilac / Carol / Milla per BUILD_AND_TEST_LOG "Session 17" (perf, VRAM,
  sound RAM, no AICA errors). Original-game screenshots of FN1 / FN2 / FN4 starts match the host renders.
  **Nothing measured on real hardware.**

## Next actions
1. Real hardware: FN3 (Carol / Milla: lowest VRAM; Milla: slimmest frame time) and FN6; read `perf:` / `prof` and the
   `stage sounds ..., sound RAM largest free block` line.
2. Original traces for the FN INFERRED items (TODO "Fortune Night"): bulldozer facing T-B1, samurai selector U-E1, Panther
   U-G2 / U-G3.
3. The session 14 RM hardware check and the RM / AT traces still listed in TODO.
4. Next stage after Fortune Night (Sky Battalion, json frame 36+) via ASSET_PIPELINE "Adding a level".

## Carol: verified vs approximated
- **Matched to original traces (RV, cmp_carol, 53 traces):** on-foot movement, pounce chains, roll and roll jump, crouch
  lunge, wall cling/slide/jump/top/away, claw combo and big slash incl. hits, Wild Kick incl. air and roll kicks, energy;
  bike riding, jump, air jump, nitro, dismount, remount, claw and kick on the bike, ram, wall ride, wall jump, skid; fuel
  can pick-up; DV2 jump-pad chain and walking onto a pad. Fields per frame: X, Y, B, G, L, M, OTHERS.Y, tail
  animation and frame, skin A/L, energy, sound ids.
- **Traced with known harness differences:** 7 traces that place Carol by teleport next to the DV1 Turretus get the
  pellet hit 1-2 frames off (from frame ~146; the same as Lilac's TP traces); c_fuel_dv1 rams a Dragonfly one frame
  apart (enemy phase); b_remount and c_jumppad_dv2 differ only in G on a frame where the original's G is a display value.
- **RE only (no trace):** 3-hit wreck and debris, DV3/DV4 pad tables, death/fall with the bike, Carol under water.
- **INFERRED / not ported:** see RE_NOTES "Session 10" (sample 413 stop, claw pitch, DV4 block pad 5356, the 2443 Y12
  timer, bored-idle variants, spin-shield angle, debris direction, bike monitor smash path, dust step rounding).
  Random voices and pitches use the port's own seed, so they differ from a given original run.

## Known issues / INFERRED
- **Open (session 9):** the lower waterfall is slightly darker than the original in DV1; Tredd's rock break sound (13)
  is INFERRED from the DV1 rocks; the DV4 exit is RE-only (no original trace).
- **INFERRED:** water tint extent (drawn full width below the surface, also over left foreground rocks); shield carry-over
  between acts; spinning-log phase (global counter offset); breath bubbles' SKIN condition; stage-select confirm timing.
- **Not ported:** the original's game-over card, enemies' own water splashes; in the gallery (session 15): the frame-9
  achievement pop-up (an overlay instead), idle card twinkles, the console button-icon hints, real voices for 15 sound cards.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data or decompiled output (`git ls-files` check before commits).
