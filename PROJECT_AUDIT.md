# Project Audit: evidence-driven overhaul (2026-10-22)

## 1. Baseline before the overhaul (git tag `m1-dc-boot`)
The first implementation built and booted on Dreamcast, but it was **not faithful**:

| Area | Baseline | Why it was wrong |
|---|---|---|
| Level | Hand-made ASCII tile level (`data/test1.txt`) | Not Freedom Planet geometry at all |
| Collision | 16 px tile heightmaps (45° / 22.5° only) | FP terrain is free-form art with pixel masks; curves, quarter-pipes and walls were impossible |
| Movement | Floor-only slopes; curves acted as walls | FP runs up curves and walls (Sonic-Worlds-style engine) |
| Animation | ~40 frames picked *by eye* from the Windows archive, guessed order and timing | Real tables (frames, speeds, loops, both facings) exist in the binary |
| Objects | Placeholder crystal/hornet/rects | Original classes, placements and animations exist |
| Data source | Windows v1.21.5 (stripped exe) | No symbols, so level and animation data couldn't be recovered |

## 2. What the Linux build made recoverable (CONFIRMED by tools in `tools/re/`)
Linux `bin64/Chowdren` (sha256 `ff514984…`) is **not stripped** (89,974 symbols).

| Resource | Status | How |
|---|---|---|
| 87 frames (levels/menus) with size and every object placement (class, x, y, layer) | **Recovered**, 0 unresolved coordinates in all 87 frames | `dump_frames.py`: objdump of `Frames::on_frame_N_init`, tracking edi/esi/edx before `create_*` / `add_*_object` |
| Animation tables for 4,208 classes (5,592 of 5,596 directions resolved to image ids) | **Recovered** | `dump_anims.py`: decoded `anim_*`/`direction_*` structs + `get_internal_image_direct` calls |
| Terrain pieces: image id + collider type for 1,713 backdrop classes | **Recovered** | `dump_backdrops.py` (inlined creators) |
| Collision semantics | **Recovered**: 0xac=9 + InstanceBox vtable means solid box; 0xac=8 + BackdropCollision vtable means solid pixel mask; 11 means ladder; none means decor; blend alpha 0 means invisible solid | Collider vtables resolved by symbol |
| Layer scroll coefficients | Recovered: all 1.0 except layer 6 (0.0) | `Layer::init` args in frame init |
| Parallax | Positions/images recovered (`levelparallaxpiece*`); **scroll behaviour not recovered** (event code) | |
| Assets.dat (Linux, md5 `9d00124a…`, unknown to fp-assets) | **Extracted**: offsets table located at 33,770; 16,885 images (zlib), 475 sounds, 1 font, 77 shader pairs, 18 files | `tools/fmt_linux_9d00124a.json` |
| Physics constants | **Not recovered.** Sonic power-of-two constants are referenced 0 times; values live in compiled event code | |
| Behaviour (enemies, gimmicks, boss) | **Not recovered** (compiled events) | |

## 3. Comparison table (Phase 3)

Only rows with evidence are filled; "UNVERIFIED" marks reconstruction.

| System | Original evidence | Before | After (this overhaul) | Verification |
|---|---|---|---|---|
| Level layout | Frame 21 placements (2,395 objects, 14,816×1,864 px) | Invented tiles | **Exact original placement** of 1,890 pieces + 234 objects | Full-frame render `build/frame21.png` matches FP Dragon Valley art; host and Flycast screenshots |
| Collision | Collider types + piece alpha | 16 px heightmaps | **Pixel masks** of original pieces (482 unique 16×16 patterns) | `col_*.png` mask renders; tests `passes_curve`, `no_fall_out` |
| Slopes/curves | Quarter-pipe piece `dvfloorcurvesmallright` in the level | Treated as a wall (stalled at x≈952) | **4-mode 360° ground collision**: rides to 90° and launches off the lip | Test `quarter_pipe_ridden` (max angle 1.57 rad, speed > 4 throughout) |
| Animation | `playerskinsash_341`: 42 animations, both facings, Fusion speeds/loops | Guessed frames | **Recovered frames/speeds/loops/back_to**, separate left/right art, Fusion-style 0-100 speed player | Test `anims_change`; visual frames |
| Moves | SDA KB: cyclone once per airtime, not after boost; boost 6 directions; invincible during boost; charging in air stops Lilac; upward air drag | Partly | Rules implemented as stated; timings UNVERIFIED | Tests `dragon_boost`, `dragon_cyclone`, `attack_and_recover` |
| Movement physics | None numeric (KB qualitative only) | Placeholders | **Still placeholders** (acc 0.125, top 6, grav 0.21875, jump 6.5) | Jump apex 92.7 px measured in test; **not compared to the original** |
| Camera | None recovered | Simple follow | Unchanged | — |
| Objects/enemies | Placements + animations of Turretus, Dragonfly, crystals, petals, spikes, springs, monitors, rocks, sign | Placeholders | Original art and placement; behaviour reconstructed (UNVERIFIED) | Test `turretus_destroyed` |
| Target platform | — | Booted (old content) | **Flycast boot with recovered level**; pad input moves Lilac; update 70 µs/frame after streaming fix | BUILD_AND_TEST_LOG |

## 4. Uncertain or open
- Frame 21 is identified as Dragon Valley by its piece set (`dv*`) and start point. That it is **Act 1** is inferred from frame order (21→22→23), not proven.
- Animation *labels* (which anim number is "hurt" etc.) are by eye; frames, speeds and loops are exact.
- The original game runs in this VM (Xvfb + llvmpipe) and can be put into frame 20 via gdb, but the stage doesn't progress without menu-set state, so **no physics measurement from the original yet**.
- Reference videos couldn't be viewed from this environment (see REFERENCE_ANALYSIS.md).

## 5. Prioritised correction plan
1. **Physics fidelity:** get the original playable in the VM (drive menus, or find the character/stage globals), then log Lilac's position per logic frame with gdb under scripted input. Fit acc/dec/top/gravity/jump/air drag and replace the placeholders. Add trace-comparison tests (original vs port).
2. Sprite rotation with ground angle (FP rotates Lilac on curves); PVR quad rotation.
3. Object behaviours from event code, where they matter for traversal: springs (5 kinds in DV1), `levelvalleyplatform1` (moving/jump-through platforms), `halfcorkscrew*`, `layerchangeswitch` (two collision layers for loops), breakable rocks as solids.
4. Parallax scroll factors (event code) and camera rules.
5. Remaining Dragon Valley frames (22, 23), then later stages via `tools/build_level.sh <frame> <name>`.
