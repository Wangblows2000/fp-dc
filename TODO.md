# TODO (priority order, prompt v3)

## Checkpoint 1 remainder
- [ ] Trace damage/knockback and spring in the original; compare (`tests/orig/`)
- [ ] Boost in six directions + air charge vs original
- [ ] Energy meter: boost cost / recharge / shield rules from the original
- [ ] Attack hitboxes from the original (sizes currently reconstructed)
- [ ] Sprite rotation by ground angle

## Checkpoint 2
- [ ] HUD from original graphics (reference docs/reference/hud_dv1.png): energy meter, petals (verify max), lives, crystals, timer mm:ss:cc
- [ ] D-002: 4:3 crop vs 424-wide letterbox (decide in DECISIONS.md)
- [ ] Title + options menus from the original frames (2, 3, 4, 13)
- [ ] Music: 68 BGM, loop points (code or approximate), manifest, ADPCM vs libtremor measured

## Checkpoint 3
- [ ] DV2–DV4 objects/enemies; checkpoints; boss
- [ ] More stages through build_level.sh (dirs named in game.c SEL table)
- [ ] Real-hardware test (CD-R / ODE)
