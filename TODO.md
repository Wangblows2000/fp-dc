# TODO (priority order)

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
- [ ] Not ported: Brevon mech fly-by scene (needs half-resolution ENS rows), the ice death (PlayerIceDeath), LevelRain,
      Adventure dialogue / cutscenes, the bubble hitting enemies (10445)
- [ ] INFERRED: Elite constants, bubble-spring bounce constants, slab carry, subtanker bob phase, meteor speed, ring bounce
      outside observed cases, JC1 Hovercop y fit, JC3 park x 8084, buoy period (+3), plant first shot (-3), JC4 first volley
- [ ] Not play-tested end to end: every act was checked in sections (teleports) and the act chain, not by a full run
- [ ] SB2 Spade carry-in HUD hide (9615 / 9702) not ported

## Trap Hideout (session 21b)
- [x] TH1-TH3 for Carol / Milla: route from JC5, enemies, traps, Laser Iris, Crystal Basher, arena + Spade, results, save, TA
- [ ] Not ported: RocketPuncherShield's second hit (13679) and barrel bounce (col_6_57), arena petal drop (14055 / 14056), the
      crowd's sound during Spade, a mid-fight voice on channel 27 (RV a1_c 1330), EnemyNinja_3193 finale prop / Adventure dialog
- [ ] INFERRED: laser start phase (G.frame), sweep beam length +-1 px, Iris beam length (~14 px), far sweep beams / bike
      ninjas frozen off view (cost), Spade's J rate, buoy-like anim phases; no full-act play-through, only sections + chains
- [ ] Perf: TH3 update ~10.4 ms (Flycast, ~25 slow frames / 600), TH1 9 ms after the fixes (33 ms before)
