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

## Session 15
- [x] Casual difficulty kept across front-end reloads (the "damage" report); RM1 switch column; RM2 entrance gates;
      Carol's life icon; Gallery; title cheat
- [ ] Original trace of the RM1 → RM2 walk-in (tracer must follow a frame change) to replace D-070
- [ ] Other set_visible(0) classes: audit when they are ported (list in RE_NOTES "Session 15")
- [ ] RM2 basement darkness: drawn as an opaque grey box with a black surround; shader 20 (U4) unknown
- [ ] Real voices for 15 gallery sound cards (placeholders from /home/claude/fprun)

## Front end / presentation
- [x] GALLERY (session 15: fe_gallery.inc, test_gallery.py)
- [ ] Card/transition timings from a higher-rate capture

## Platform
- [ ] Real-hardware test (CD-R / ODE)
- [ ] Remove the FP_PROFILE overlay from release builds
