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
- [ ] Continue a file at the stage after its last stage (only AT / DV exist: Continue starts the route's first stage)
- [ ] Unlock flags (records x2: all-cards unlocks, gallery) are not stored; the gallery is not ported

## Front end / presentation
- [ ] GALLERY (Carol playable since session 10, Milla since session 11)
- [ ] Card/transition timings from a higher-rate capture

## Platform
- [ ] Real-hardware test (CD-R / ODE)
- [ ] Remove the FP_PROFILE overlay from release builds
