# TODO (priority order)

## Blockers / need from user
- [ ] **GOG Linux build of Freedom Planet** (same GOG account offers it). Its `bin64/Chowdren` ELF keeps symbols. That unlocks freedom_editor-style recovery of the **real stage layouts** (`Frames::on_frame_N_init`), object classes and per-object image lists (= animation frame sets).
- [ ] Ask whether the user owns the **2014 v1.0 build** (fp-assets profile md5 `f14f24317d…`). The supplied install is v1.21.5.

## Milestone 1/6 (toolchain + bootable slice)
- [ ] Finish the kos-chain build, then build KOS (`environ.sh`, `make`)
- [ ] `make` → fpdc.elf; fix compile errors against real KOS headers
- [ ] `make cdi` → build/fpdc.cdi
- [ ] Build Flycast, boot the CDI headless under Xvfb, screenshot, check input via xdotool
- [ ] Record perf counters printed by `plat_dc.c` (update/draw µs, polys, slow frames)

## Milestone 7 (real data)
- [ ] Linux-build symbol dump → animation tables (replace hand-picked frame orders in `tools/slice_manifest.json`)
- [ ] Linux-build frame dump → stage terrain pieces + object placement → converter → `.lvl` v2 (piece-based terrain, not tiles)
- [ ] Collision from terrain-piece images (alpha → heightmaps) instead of hand tiles
- [ ] Sound name mapping (sound handles referenced from code)

## Gameplay fidelity
- [ ] Recover physics constants (D-005); compare with footage frame by frame
- [ ] Sprite rotation on slopes; loops/360° running (needs angle-mode collision: walls/ceilings)
- [ ] Real checkpoint lamp + goal sprites (placeholders now)
- [ ] Lilac moveset per original: hair whip combo, Dragon Cyclone hold, Dragon Boost aim + recharge rules
- [ ] Carol, Milla
- [ ] Health petals, life counter, HUD graphics from the archive

## Engine / platform
- [ ] Music loop points (per-track table; none in the oggs)
- [ ] ADPCM music (4:1) or libtremor to cut disc size
- [ ] Twiddled / VQ textures, per-room texture loading
- [ ] Title screen, stage select, pause menu
- [ ] Real-hardware test (CD-R / ODE)

## Known issues
- Hand-picked animation orders and SFX choices are guesses (UNVERIFIED).
- 4:3 crop of the 16:9 original (D-002).
- Host software renderer has no rotation/scaling beyond integer sprite blits.
- `.rdata` double-constant probe near the event-group strings was inconclusive (only generic integers).
