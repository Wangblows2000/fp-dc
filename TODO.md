# TODO (priority order)

## Session 25 status (end of session): what is left
- [ ] **Adventure cutscenes: visual polish (none of the 70 scene rows has had a full visual review against the original).**
      All play with the right dialogue, voices, triggers, skip and save, but the look is rough in places. A polish pass was
      stopped unfinished at the user's request; its uncommitted work is on GitHub branch `wip-cutscene-polish` (NOT merged:
      untested, the scene scripts there are mid-re-conversion; new script ops for layer offsets / visibility, location
      names, scale / rotation, multiply ink, dialog box position; tools/story_cmp.py and tests/test_story_visuals.py
      started). Unpolished scenes, by arc (variants _c / _m / _l share their base's issues):
  - Arc 1 (DV / RM): adv_opening (comet / pod trail explosions off-view in the trace are missing; linear fade-in),
    dv_meet, dv_crash (the ground smoke is opaque white, the original's is translucent; the white flash timing is from
    screenshots; falling leaves not played), dv_crash2, rm_gong.
  - Groups A / B (RM, Treehouse, Gong, Spade, Fortune Night, Zao's airship): rm_hideout, rm_collapse, rm_meet_milla,
    th_torque_revealed (+ 2, 3: the flashback's multiply ink), th_next_morning, np_gong_help (the totems), sm_spade,
    fn_panther_defeated, zao_airship, zao_airship2 (Zao's ship moving through the world), rm5_spade (in RM5: light rays /
    hook line INFERRED).
  - Groups C1 / C2 (Shang Tu / Shang Mu, room 83): st_bad_news, sg_brevon_plan, sm_zao_dinner, sg_intruders (its 408-frame
    starfield opening is not ported at all), sm_zao_help, st_magister_talk, jail_plan, jail_fail, throne_apology,
    sg_brevon_torque, jc_cave, jc_cave_cm, th_carol_help, tb_arrest, tb_outside_team, st_mission, pl_final_message. Scenes
    chained from another group start silent instead of keeping the previous track; only the wipes before a room change
    are played (other fades skipped); the location-name captions are missing.
  - Groups D / E (Jiang river, Thermal Base, snowfields, ending): zao_airship_ambush, jiang_river, jiang_river2,
    tb_torture, tb_team_spade, tb_brevon, sf_army (the dragon flight's sky should be black, the port keeps the mountain
    backdrop), sf_camp, ending, ending2 (restarts m_ending instead of continuing it), ending3.
  - Not ported: the frame-84 ride before Battle Glacier (the port goes straight to BG1).
- [ ] Not ported: sg_intruders' 408-frame starfield opening (frame index 0, g4 99: a non-scrolling layer 0 and no pause
      panel); the frame-84 ride before Battle Glacier (the port goes to BG1); FD1a's death taunts br_pa_07..09; SB2's
      LevelBackgroundPlane2; the Brevon mech fly-by (BG4 / JC5 / FD4b, plan in "Session 26b"); spring beams in FN3 (VRAM).
- [ ] Rules found, not changed: death on the hit that empties the petals (original: next hit); TB3's look-back freeze; TB4
      Carol's start x (173 vs 71); BG2's EffectWarning triangles. Details in "Session 26c".
- [ ] Flycast-measure the acts the small fixes grew (BG2 +512 KB, TB1 +256, FD1a +160, RM2 / SB4 +128) and keep >= 500 KB.
- [ ] JC5's sound group is ~210 KB (D-121): nothing more may be added there.
- [ ] Real hardware: nothing has been run on a Dreamcast.

## Session 26c: small fixes (tests/test_small_fixes.py)
- [x] RM2 pad images; death sounds (die sound only, Lilac 91 resident, Carol's 29 a frame later, the bike wrecked by the death
      group); metal-door sounds at A 1 / A 2 (TB, FD lab doors); BG2's fly-by plane (sprite, 102, shake); JC5 white card drawn;
      FD1a pod rain from the dialog's B 160, dialog voices br_pa_01 / 02 + subtitles; FD1b alarm cos + additive; spring beams
      (every act with springs but FN3); FD transporter nodes drawn; SB3 eyes on their 0.8 layer, only SBBackEye2 fades
- [ ] Not done: spring beams in FN3 (one row doubles its _ens atlas: +512 KB with Carol at 625 KB free); SB2's own
      LevelBackgroundPlane2_2227 (9756-9759 / 9771 / 9772, its fly-by: not looked at); FD1a's death taunts br_pa_07..09 (15288)
- [ ] Found, not fixed: the port kills on the hit that empties the petals; the original keeps the player alive at 0 petals and
      kills on the next hit (2414 needs K > 0, 2416 K == 0; RV pl_bosses p7: 60 frames at 0 petals, then 91 alone). Many modules
      read hp <= 0 as dead. TB3's chase: during the look-back (K 0) the original holds the player (RV p7_l: X 2860 from 1300 to
      1445 with Left held) while the port lets her run, so the K 1 heat-sky rule (12260) cannot be compared yet. TB4 with Carol
      starts at x 173 in the port, 71 in the original (RV tb4_door). BG2: the original shows EffectWarning triangles near the
      player at the plane's pass (RV bg2_plane f114-f130), not in the port
- [ ] INFERRED: the beam direction of every spring class but the up spring (RV: up only; by the spring's push direction); the
      plane's creation waits for the view to hold the player (the port's camera arrives a frame after a teleport)

## Session 26: late-stage gimmick gaps (build/gimmick_audit.md)
- [x] FD4a vanishing blocks drawn; PL5 / PL6 time-gate rings and TB5 conveyors complete (per-type lists sized per act); DV4 geyser
      eruption + launch - 12; yellow down-springs drawn with their press (PL1 / PL2 / SB4, FN2 / FN4); FD3a / FD3b sounds 285 / 286;
      FD3 back pillars; fd1a jumbotron + pods (+ the camera bottom 2016); TB3 / TB6 heat sky; FD3a alarm pulse; SB back-eye fade;
      BG4 arrow sign; FD4b terminals (tests/test_late_gimmicks.py)
- [x] (session 26c) the springs' EffectsSpringBeam, the jumbotron dialog's voices / subtitles, the fd1a pod rain from the dialog's
      C at B 160, the FD1b alarm's cos / additive rule

## Session 26b: gimmick gaps (build/gimmick_audit.md items 2, 4, 12)
- [x] Carol's jump pads in TB4, TH3, BG2, JC2-JC4, FN4, FN5 (RV pad dumps -> levels.c PADS_*; JC3's pad lit and inert), drawn
      with each handle's images; Lilac / Milla's crystal pop as PinballMovement 36 / 25 taken on landing; 113 at the launch
- [x] fd2b's vacuum air rule (J, dings, warnings, death 91 / 128, GUIAir, countdown digits) and its 6 oxygen patches (Easy /
      Casual only, sound 20); JC5's LevelRain (drops, splashes, loop 293, layer-0 tint, the bolt after 40 drops)
- [x] (session 26c) RM2's pad images, the death sounds 91 / 29, TB4's metal door 280 (real: A 2, not a teleport artefact), BG2's
      102 (the fly-by plane), the JC5 white card
- [ ] Brevon mech fly-by (BG4 wings, JC5 BrevonMechFlyby 11117 / 11118, FD4b BrevonMechBackground_2444): not done. Plan: an
      importer flag that halves an ENS row's images (2 x 2 box filter, frame size / hot spot / action point halved) and a
      draw flag that scales them x2 (as parallax PAR_ALPHA flag 2); export the fly-by rows only for 62 / 44 / 73 with that flag
      (~130 K px of the JC5 flyby -> ~33 K px), then port 11117 / 11118 (view x + width + 120, y + 100; speed 0.05 N capped at 4;
      scale from sin((20 + N) 0.05); alpha 248 - 2 N). Check Flycast VRAM >= 500 KB per character before enabling a stage.

## Session 24: Battle Glacier, Final Dreadnought, ending, builder, PC port
- [x] BG1-BG6 (flow, enemies, gimmicks, bosses) and FD rounds 1-4 (fd1a-fd4b) for Lilac / Carol / Milla; Classic route into
      BG and FD; each FD round's results; FD4 -> final cutscene -> FINAL SCORE -> credits -> Thanks for Playing -> title
- [x] Whole-game transition matrix (test_route_matrix), enemy view box (BOX1), FD boss sprite-name collision fixed
- [x] PC port (branch pc-port): Linux x86_64 + Windows i686 (Wine-tested); drag-and-drop Dreamcast builder (master)
- [ ] Real hardware: VRAM on fd3b with Carol (768 KB free in Flycast), BG4 / fd1b; sound RAM BG3 (205 KB) and FD1B (198 KB group)
- [ ] Not ported: the shoot-'em-up lead-in to BG (frame 84), BG6 closing scene, Absolution's lasers killing
      enemies, BG6 iris enemies / shield bubbles / card trails; FD4b space backdrops + background mech (VRAM), the turret's
      aimed / acid volleys, Brevon's grab (state 10) and weapon-arm throws (14-15), finisher moves, voices, intro Elites;
      fd1a pod rain at the dialog step (INFERRED self-start at x 800);
      Adventure dialogs; the epilogue (frames 81 / 82)
- [ ] INFERRED (see build/re/spec_bg_*.md / spec_fd_*.md): BG background-mech spacing, Sparky plasma, ride wall stops;
      gatling barrel formulas, pounder limiter turns; morpher form-G angles, fd2b attack order and arena camera; mutated
      Milla body motion; suit damage values; PC port: real controllers / sound devices / native Windows untested
- [ ] FD4b's mech duplicates bg_boss_mech.c with FD numbers: share the code

## Pangu Lagoon (session 23)
- [x] PL1-PL6 for Lilac: route from TB6, Lilac-only (TA slot / g1 = 0), enemies, gimmicks, stone doors, Holo-Dragon, results
- [ ] Not verified: a full start-to-finish run of each act (sections tested with teleports); enemy timings are RE only (no RV)
- [ ] INFERRED: wall-stinger respawn home, stinger wrap width (G16 = view width), terreon pale frames drawn additive, spinball
      chain 3/4 radius, vanishing-block first cycle, door walk speed / fade, Pangu's arms (13431/13432), flame damage 10, the
      ending's backdrop; achievements 13437-13446 not ported; PLWiiUSign / Adventure dialog not exported
- [ ] PL5 is the heaviest act on Flycast (~11.5 ms update)

## Session 22 targeted fixes
- [x] TB4 bomb switches (3 destroyers per switch, RE 11957/11958), TB6 boss contact only in the dive + dive softlock, TB rails /
      FN zip-line grips (whole-image hang boxes), hurt-state 3 s cap, C -> X labels + pad mapping, 200-crystal lives with m_1up,
      TH3 camera after the basher
- [ ] TH3 pillars for Milla: no change made (matches the original; Milla passes in host tests) — get the exact spot from the user
- [ ] Spike-sphere contact rule (TB6, RE 12710) not fully matched; the 1UP pop-up / sparkles not ported; lit-block spiral every
      4th frame (INFERRED, perf)

## Gameplay evidence still missing
- [x] Turretus firing pattern / pellet / damage (RE, session 4)
- [x] Handlebar grab and hold (RE); [ ] release and moving along the bar (events 4057-4066)
- [ ] Sensor system port: landing-angle routine (1590: boost_air_right f172), ground angle (1603), push-out loops (turretus_landon)
- [ ] Down-kick/cyclone moving_frames (3-frame difference) and 1-frame late landings in dk_* sequences
- [x] Invulnerability length (RE: 60 frames)
- [ ] DV1 pole-robot mini-boss (x ≈ 14400), Serpentine boss (DV4)
- [ ] Results screen from the located events 4560-4589 + loop_tally_19 (layout currently INFERRED)
- [ ] Turretus hit by attack (events 4776/4777), pellet destroy handlers (col_7_20, col_20_20)
- [ ] Jump animation frame chosen by vertical speed (animation agreement 29%)
- [ ] Parallax layer offsets (backgrounds differ from the original at the DV1 start)
- [ ] 1 px vertical seam seen in Flycast near screen x 240 in DV1 (tile edge under the 1.5x scale?)

## Aqua Tunnel (session 12)
- [ ] Original traces still to record: Chomper bites with the lure (Lilac-style whip / Milla burst), Chomper defeat,
      snake bomb / cube hits and defeat, Iron Worm kill and slam side, AT3 swings / axels, the beam crush, AT2 Milla block
- [ ] a1_collapse (TP + jelly launch) triggers the platform early; a2_crusher landing 3 frames late; a2_beam and
      a3_crusher small differences; harness-artifact traces a1_jet_right, a2_reddown, a2_bomb_stand, a1_crate
- [ ] Card frames: the original runs object events during the stage card (the port freezes them, also in DV); only
      the Iron Worm chain is pre-advanced (62 frames)
- [ ] Draw order: slime floor in front of the snake (4653 move_front), rocks / head move_back (INFERRED)
- [ ] DV shade effect name (effectsshade_436 vs effectshade_436), see ASSET_PIPELINE session 12
- [ ] Original traces for the session-13 items that are RE only: Aqua Tunnel card pickups (ids 21-30, numbering
      INFERRED), a story clear of DV4 / AT4 (SaveData x3 y1..3 and MenuSequence across frame 8, ML.Z at the results)
- [ ] AT4 results voices (mi_milla_yay / thatwasfun) and the victory pose (OTHERS.Y 48)

## Session 13 (bug fixes, reusable systems)
- [x] Milla's hang animation (RE 2461, RV m_hang_dv1); Lilac / Carol unchanged (test_levels)
- [x] Aqua Tunnel results backdrop (results images for every stage that places the clear card; test_levels)
- [x] Title cards per stage (STAGE_CARDS; cards per level / character / load order / restart in test_levels)
- [x] Time Attack previews (solid masks, tiling), checker column and translucent menu bar (RV screenshots)
- [x] DV4 boss hit tally removed (D-059)
- [x] Level table, class+handle types, shared switches, object registry, presence-driven passes (D-052 to D-056)
- [x] Save v3 for every stage (records, files, progress, cards; v1/v2 conversion; test_saves)
- [ ] Records x8..x12 and flag G31 of records.dat (meaning UNKNOWN, spec_records_saves)
- [x] Continue a file at the stage after its last stage (session 14: frame 8 routing, lives / shield / counter)
- [x] Gallery (session 15): records x20 opened state and x2 achievements in save v4; only Card Collector (23-32) and the cheat's 1-3 are set by the port
- [ ] Achievements other than Card Collector are never earned in play (records x2 events not ported)

## Relic Maze (session 14)
- [x] RM1-RM5 for Lilac, Carol (on foot / bike) and Milla: routes, exits / entries / death targets, stage flags,
      checkpoints, cards, music, enemies, gimmicks, Eye / Bone Golem, Claw sequence (Lilac / Carol), Milla's hand-car,
      Kingdom Stone scene, Mantalith, boss results, saves (host tests; Flycast: every act loads and runs a demo window)
- [ ] Original traces for the RE-only items (spec_rm_* "Traces to record"): golems, Claw, Mantalith, hand-car end (U5),
      RM5 room warps, the Eye Golem camera release (D-066), shockwave ageing during the Bone Golem (RE_NOTES UNKNOWN)
- [ ] Real Carol / Milla result voices (only placeholders available here)
- [ ] RM5 Adventure dialog cutscene (D-064), HUD slide / timer pause in the RM5 scene, Mantalith sounds 147 / 214 / 215
- [ ] Unlock and achievement records (7119, 6900, 7116 / 7117; D-065)
- [ ] Fortune Night (frame 29) and later stages: RM5 currently ends at the front end

## Fortune Night (session 17)
- [x] Acts FN1-FN6 for Lilac / Carol / Milla: flow, parallax, enemies, gimmicks A / B, minibosses, Serpentine, samurai,
      Panther (FN5 chase, FN6 fight), cards, title cards, TA entry, per-act sound groups
- [ ] Original traces for the INFERRED items: bulldozer facing (T-B1), samurai attack selector (U-E1), Panther mode-2
      limb pose (U-G2), partner assist helpers (U-G3: defense / projectile helpers not ported), rolling arch entry, lift
      second firing, trolley capture, restroom entry
- [ ] Not ported: EffectWarning2 off-screen missile tracker, boss HUD slides, Serpentine escape timer stop / Adventure
      dialog / beam rotation; Hard-only Serpentine missile untested
- [ ] Real hardware: FN3 / FN5 / FN6 perf and memory (emulator: 603 vblanks per 600 frames)
- [ ] FN3 with Carol / Milla has the least VRAM left of any level (see BUILD_AND_TEST_LOG "Session 17")

## Sky Battalion (session 19)
- [x] SB1-SB4 for Lilac, Carol, Milla: flow (hub pads, fly-in, ship openings, night), enemies, gimmicks, object gaps, bosses
      (Kujacker, Spade / Chopper, Satellite, Grappler), cards, title cards, stage select / TA, sounds, route FN6 -> SB1
- [x] DV3 bamboo spike traps (pairing, timing, damage); Quick Backdrop tiling (RM3 cave wall for every character)
- [ ] Play-through from each act's opening to its boss arena per character (skipped by request; host route searches
      reached SB2 Lilac, SB3 all three; SB4 not re-run after the chain-wall fix)
- [ ] INFERRED / not ported: Kujacker strike spoke cadence + 20 damage, head/neck geometry, tail targets returning, Milla's
      tornado shield exemption, player blown off the deck; plane boarding after real cannon kills; dialogue / voices,
      m_trouble before Spade, HUD shift, CAM.H -50 (9008 / 10081), copilot palettes; Adventure cutscene branches (g4 99)
- [ ] Gimmick guesses: hover-platform flame follow, cannon bullet removal, lightning cooldown, fan debris, chain-wall mask
      span, crusher limits / trigger, moving-block trigger, SB3 back eyes fading; Magnetizer stays dormant (RE: never in view)
- [ ] Hardware: SB1 with Carol has the least VRAM of any level (515 KB free after load; a carried shield takes 256 KB more)
- [x] Session 20 fixes: Kujacker feather glow / boss music, FN -> SB invisible player, ship exits (beaten cannon bases),
      ship draw order (json layer 1 behind layer 2)
- [ ] Decision pending: RM3 dirt filler (x 976-7488, y 800-1568) and FN5 (x 11904-15200, y 240-512) are solid now (original's
      obstacle flag)

## Fortune Night playtest fixes (session 18)
- [x] FN6: projectiles, standing (mode 2) pose, stray debris, left edge, HUD slide, partner assists (Carol bike / Lilac
      float / both for Milla), plane petal delivery, closing cast + results; FN5 arena floor; FN1 UFO armour; FN5 dance timing;
      rolling arches; pellet sound; shield sprites across acts; gallery voices; FN2 / FN4 ceiling springs; CPU cuts
- [ ] Flycast / hardware: FN1 UFO clang + floor break, rolling arches (FN1 x ~1750 spring), FN5 samurai arena exit,
      FN6 full fight to the results per character, pellet hit / miss voices, DV3 -> DV4 shield carry
- [ ] Player core: the hurt pose ends on landing (player.c); the original keeps 4 grounded hurt frames at B -1.7 (RV t3,
      Milla's FN6 knockback 34 vs 42 px)
- [ ] FN3 Serpentine (fn_boss.c): 7911 (dash) runs before the state machine; the original runs 7828 first (Hard missile
      range 300-400 vs > 300 px); rapid bullets' "C = E mod 2 + 2" in FN3's 7877 is probably C = E + 2 as in FN6
- [ ] FN5 samurai digs slower than the original (attack selector U-E1 still INFERRED)
- [ ] FN6 leftovers: closing-pose sprites / Serpentine on the floor / dust (8765-8771), defeat during mode 2, Carol's
      exhaust dust (8675), achievement 21's rider branch, petals' treadmill ride timing, missile layer change (8660)
- [ ] FN3 HUD slides (7821 / 7868) and EffectWarning2 (7889-7891); Serpentine escape timer stop / Adventure dialog / beam
      rotation (documented, not ported)
- [ ] Yellow down springs: runtime check (host scripts cannot launch the player upward into them)
- [ ] Real Carol / Milla result voices: milla_yay / gasp / thatwasfun exist in the PC install (not integrated yet)

## Session 16
- [x] Seams (offsets), RM3 truck / diamond, thrown petals' layer, basement vignette, VMU portraits, achievements
- [ ] Verify the seam fix on real hardware
- [x] Earth-shield magnet report: the shield (any type) kept working but was invisible in acts without a shield monitor (session 18, D-089)
- [ ] Achievements needing unported stages (spec_achievements out-of-scope list); 18 Switch Skipper needs a trace

## Session 15
- [x] Casual difficulty kept across front-end reloads (the "damage" report); RM1 switch column; RM2 entrance gates;
      Carol's life icon; Gallery; title cheat
- [ ] Original trace of the RM1 → RM2 walk-in (tracer must follow a frame change) to replace D-070
- [ ] Other set_visible(0) classes: audit when they are ported (list in RE_NOTES "Session 15")
- [ ] RM2 basement darkness: drawn as an opaque grey box with a black surround; shader 20 (U4) unknown
- [x] Real voices for 15 gallery sound cards (session 18: genuine clips from the PC install, D-093)

## Front end / presentation
- [x] GALLERY (session 15: fe_gallery.inc, test_gallery.py)
- [ ] Card/transition timings from a higher-rate capture

## Platform
- [ ] Real-hardware test (CD-R / ODE)
- [ ] Remove the FP_PROFILE overlay from release builds

## Session 20 (bug-fix addendum)
- [x] FN6 Milla: Serpentine / the panther gone after the flash (8749 every frame), closing camera on the floor (8764)
- [x] FN -> SB: the player was not drawn after FN6 in every later level (fn_assist state reset per level)
- [x] SB Kujacker: feather glow (cyan / yellow / flash), boss music kept through the night fade
- [x] SB2-SB4 exits: a beaten cannon base flies off (no invisible wall); SB draw order (airships behind Zao's sail)
- [x] RM: hang sensors invisible (red boxes); RM2 start plate pressed (axle tunnel softlock); hand-car bumper front / rear
- [ ] Flycast / hardware: FN -> SB1 -> other levels for each character on a disc (see BUILD_AND_TEST_LOG "Session 20")
- [ ] 9145 (the Kujacker's fourth head hit, dir 4: red INFERRED) not traced; FN1-FN3 json-1 objects (firework launchers,
      strobe lights) still drawn with the play-layer objects (not reported; same layer rule as D-100 if they are visible)
- [ ] The user's "RM1" switch room was identified as the port's rm2 basement (axle tunnel, plates); if another switch was
      meant, it is still open

## Jade Creek (session 21)
- [x] JC1-JC5 for Lilac, Carol, Milla: flow / route, enemies, gimmicks, bosses (Hovercop fly-by / chase, Neera, subtanker ride,
      JC5 ambush, Tredd, Serpentine 2, gunship), results, save, Time Attack, stage groups; SB boss voices / HUD
- [ ] Not ported: Brevon mech fly-by scene (needs half-resolution ENS rows; plan in "Session 26b"), the ice death (PlayerIceDeath),
      Adventure dialogue / cutscenes, the bubble hitting enemies (10445)
- [ ] INFERRED: Elite constants, bubble-spring bounce constants, slab carry, subtanker bob phase, meteor speed, ring bounce
      outside observed cases, JC1 Hovercop y fit, JC3 park x 8084, buoy period (+3), plant first shot (-3), JC4 first volley
- [ ] Not play-tested end to end: every act was checked in sections (teleports) and the act chain, not by a full run
- [ ] SB2 Spade carry-in HUD hide (9615 / 9702) not ported

## Session 21d fixes
- [x] JC5 gate: Neera's ice ambush (11043-11072) + subtanker start collision; TB1 shrimp room camera release; TH pillars solid
- [ ] INFERRED (JC5 ambush): Neera's start height, hidden until she drops, frame-box overlaps, ML.C 1600, Hovercop anim 7 not
      exported, rig removal 1200 px off view; subtanker collision limited to x < 1024 (json layer 4)

## Thermal Base (session 21c)
- [x] TB1-TB6 for Lilac / Carol / Milla: route (Lilac JC5, others TH3), enemies, gimmicks, key cards / doors, water levers,
      RoboShrimp, wall-pusher room, RoboSquid, Syntax Spider + lava chase, TB5 end elevator, TB6 Syntax arena, results, save
- [ ] Not verified: a full play-through of each act; which locked doors are progression gates (TB2's door at x 1952 has no card
      before it; a route search passed it); host tests teleport between sections
- [ ] Not ported: Syntax's TB3 voice (sx_lost is a silent stub in the Linux data, not in the user's files), NPCSyntaxEye,
      seeker breaking a monitor, tank-bot cooldown cut at MV.M 9, Adventure-only branches
- [ ] INFERRED: TB2 lift rider range 32 px (RE 31), fire-platform start phases (TB3 / TB5), crank phase origin, presser crush,
      lava-fall end, key-card HUD look (text), debris motion (push-up, saucer, squid), tank pellets 3 frames late, spider's hook
      descent / spike damage, TB6 arm ring during the spin, HUD shift rate
- [ ] Perf: TB5 update ~9.3 ms (Flycast); fine, but the heaviest act
- [x] Session 21e: switch buttons make RE 11957 / 11958's three destroyers (TB4's left switch wall now blows; RV btn4l), the
      destroyer creeps 2 px a frame column by column (no INFERRED spread); TB6's hitbox hurts only as the dive ball (RE 12636
      N == 5; RV x3_l), the dive's legs hold O (RE 12614-12634, RV x2_l; the third round could softlock)
- [ ] TB3's two switches (6912,2848 / 7056,2976) and TB4's right one (7096,544) hang in mid-air: only Carol's cling presses
      them (the same in the original: RV btn4r, Lilac slides past); TB6's dive starts ~10 frames before RV x2_l (N 3 timing)

## Trap Hideout (session 21b)
- [x] TH1-TH3 for Carol / Milla: route from JC5, enemies, traps, Laser Iris, Crystal Basher, arena + Spade, results, save, TA
- [ ] Not ported: RocketPuncherShield's second hit (13679) and barrel bounce (col_6_57), arena petal drop (14055 / 14056), the
      crowd's sound during Spade, a mid-fight voice on channel 27 (RV a1_c 1330), EnemyNinja_3193 finale prop / Adventure dialog
- [ ] INFERRED: laser start phase (G.frame), sweep beam length +-1 px, Iris beam length (~14 px), far sweep beams / bike
      ninjas frozen off view (cost), Spade's J rate, buoy-like anim phases; no full-act play-through, only sections + chains
- [ ] Perf: TH3 update ~10.4 ms (Flycast, ~25 slow frames / 600), TH1 9 ms after the fixes (33 ms before)
