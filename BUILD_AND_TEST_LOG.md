# Build and Test Log

All entries are real command output from this environment (Ubuntu x86_64 cloud VM, 2 cores, 7.8 GB RAM).

## 2026-10-22 Session 1

### Inputs
- Staged read-only copy of `C:\Users\wobbles\Desktop\Freedom Planet` from the user's PC via the device bridge.
- sha256 values in `original/SHA256SUMS`.

### fp-assets extraction: PASS
`python3 third_party/fp-assets/fp-assets.py -x -f ".../Assets.dat" extracted/assets` took 34.9 s and exited 0.
Output: 17,162 PNG + meta, 475 ogg + meta, 190 shader files, 18 text files.

### Toolchain (kos-chain, stable profile)
- First attempt **FAILED**: `makejobs=-j2` produced `make -j-j2` → binutils `Error 2`. Root cause: the cfg expects a bare number.
- Fix: `makejobs=2`. Rebuilt in the background (`make` in `kos/utils/kos-chain`, log `/opt/toolchains/dc/chain-build.log`).
- objc/objc++ disabled to save time.
- Result: see the next entry.

### Converter: PASS
`python3 tools/convert.py extracted/assets ".../Data/BGM" converted`
```
bank lilac     40 frames  9 anims atlas 512x512
bank objs       8 frames  3 anims atlas 256x256
tex  sky/mount/forest 512x256, rock 256x256, log 256x32
7 sfx, stage1 music 15,797,076 bytes, lvl test1 200x30 tiles 24 objects
VRAM textures total: 1589248 bytes (1.52 MB of 8 MB)
```

### Host build: PASS
`make -C host` with gcc, `-Wall -Wextra`. The only warnings (misleading indentation) were fixed.

### Host gameplay tests (`python3 tests/test_gameplay.py converted`): 17/17 PASS
Bugs found and fixed by the tests or trace:
1. Landing on the 45° slope stalled the player (gsp 0.24). Cause: the airborne wall sensor at feet−4 hit the slope face. Fix: sensor at step height, plus a snap-up landing check during airborne horizontal sub-steps. Landing now keeps gsp 5.9.
2. The gentle down-slope was placed one row high, leaving a 16 px wall. Generator fixed.
3. Ground Dragon Boost ended after one frame (the snap-landing check fired at vy=0). Boost on the ground now uses ground movement.

Visual check: frames rendered by the software renderer (`build/run1/montage.png`) show correct parallax, terrain, slopes, logs, sprites, HUD and STAGE CLEAR.

### mkdcdisc: PASS (built from source, meson/ninja, v0.0.4)
### Flycast: cloned (fa9d39a), configured with CMake/Ninja (GL, no Vulkan)

## 2026-10-22 Session 1 (continued): Dreamcast toolchain proof

- kos-chain finished (EXIT 0): `sh-elf-gcc (GCC) 15.2.0`.
- KOS `make`: libkallisti.a built. utils/dcbumpgen first failed (`jpeglib.h` missing); fixed with `apt-get install libjpeg-dev`, then rebuilt to EXIT 0.
- **KOS env ordering bug at commit 5f834c1:** `env/gcc/environ_toolchain.sh` composes `KOS_LDFLAGS` using `KOS_LD_SCRIPT` before
  `env/gcc/environ_dreamcast.sh` defines it. As a result, no linker script/startup was used: "cannot find entry symbol start" and a 25 KB ELF (stock `hello` example affected too).
  Workaround in `/opt/toolchains/dc/kos/environ.sh`: export `KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before sourcing environ_base.sh. `hello.elf` is then 2.0 MB / 1,218 symbols.
- `source $KOS_BASE/environ.sh && make` → fpdc.elf, **0 warnings**. sh-elf-size: text 244,412, data 5,804, bss 80,104.
- `make cdi` (mkdcdisc 0.0.4) → build/fpdc.cdi (740 MB: data-track padding is on by default).
- **Flycast fa9d39a (Xvfb, llvmpipe, HLE BIOS): BOOTS.** Serial: `KallistiOS v2.3.0 [dreamcast/pristine]`. All assets load from /cd/data and the scene renders identically to the host build (build/fc_shot1_crop.png).
  Perf sample at frame 600: update 132,822 µs (!), draw 868 µs, 301 polys, 121/600 slow frames. Suspect: music `snd_stream_poll` → blocking fread from CD inside the update timing window.
  This is emulator timing; real hardware not tested.

## 2026-10-22 Session 2: evidence-driven overhaul
- Linux build staged; RE tools: dump_frames (11.8 s, 87 frames, 0 unresolved), dump_anims (4,208 classes; 4 of 5,596 directions unresolved), dump_backdrops (1,713 classes).
- Linux Assets.dat extracted with custom format (12 s).
- First collision import used a wrong "8 = platform" theory: Lilac fell through and stalled. Corrected after resolving collider vtables (InstanceBox vs BackdropCollision).
- Bugs found and fixed: invisible overlay pieces drawn black (flag not read by the loader); quarter-pipe detach at 45° (stale host binary; clean rebuild shows riding to 90°); parallax loader probing a missing file.
- `python3 tests/test_gameplay.py converted2` → **14/14 PASS** (spawn, accel, cap, jump 92.7 px, variable jump 52.7 px, quarter-pipe max angle 1.57, passes curve, no fall-out, Turretus kill, boost, attack, cyclone, anim changes).
- DC: `make` 0 warnings; `make cdi` 26 MB (`-N`: no padding). Flycast: renders DV1 with PAL8 twiddled textures correctly.
- Perf (Flycast): before the music thread, update spikes of 132.8 ms and 121/600 slow frames. After moving `snd_stream_poll` to a KOS thread: **update 70 µs, 10/600 slow frames (startup only)**.
- Input: Flycast keyboard → port A works after `xdotool windowfocus --sync` + click; Lilac runs right under pad input (build/fc_in4c.png).
- Original game: boots; jump to frame 20 loads DV geometry but the stage doesn't progress (menu state missing). **No physics measured from the original yet.**

- **Incident:** commits 3947e7a/21eaf68 accidentally included converted game data (converted2/, .gitignore only covered converted/). Never published. Rewritten locally to e60ab9d, .gitignore now covers converted*/ and __pycache__, reflog expired and gc run. Verified the largest remaining git objects are source files.

## 2026-10-22 — move set + level select
- player.c rewritten from traces: **16/16 original-vs-fpdc sequences pass** (`Q=1 FR=220 tools/orig/cmp_all.sh`). Fixes found by comparison: jump uses raw angle N (apex 94→90 on run_jump), cyclone continues after release (airtime 96→100), landing-frame friction, crouch stop under speed 2, boost keeps slope force, floor mode extended to ~47° (MODE_EDGE 0.82) after a 45° slope→flat glitch.
- Host tests 16/16 (quarter-pipe and boost tests updated to the evidence; added boost_tap_nothing).
- Frame identification: 70 frames screenshotted in the original via set_frame (game survived all jumps).
- FP_DEBUG_SELECT: host (FP_SELECT=1) and **Flycast**: select DV3 → plays; pause → X → select DV4 → plays (VRAM reset + reload works). CDI 45 MB with DV1–DV4. Perf: draw paced at 16.6 ms, update ≤80 µs.

## 2026-10-22 Session 3: Section 2 fixes, HUD, front end, audio, Section 6
Evidence labels: **original-trace** (gdb trace of the running Linux original), **host-test** (fpdc host build, scripted), **emulator** (Flycast). Nothing here is hardware-validated.

- Host gameplay tests: **33/33 PASS** (`python3 tests/test_gameplay.py`; new: grounded cyclone, cancels, boost 6 directions + left orientation, slope/air rotation, energy_starts_empty, invuln_window, spring_up_vy, spring_upleft_vel, spring_side_gsp).
- Front-end tests: **9/9 PASS** (`python3 tests/test_frontend.py`: menus, dev select only by L+R within 2 s on the title, three title→level→title cycles with no stale state, music restarts per cycle; valgrind 0 errors / 0 definitely lost).
- Comparisons (`tools/orig/cmp_all.sh`): **16/16 baseline**, **52/70 overall**. The comparator now skips the ±3 frames around mid-sequence teleports, and evaluates each segment between teleports separately (all must pass).
- Fixes found by comparison this session: landing look-ahead (the original lands with ground up to 4 px below and snaps down: +2 passes); a landed boost keeps 10 (no friction on the landing frame: +2); springs (3 new sequences, 2 pass; the third diverges at the unimplemented hang gimmick); skid only on turning round; ceiling-contact frame.
- Energy meter now starts empty (original-trace + hud_dv1.png); `energy_starts_empty` checks the first charge at frame 249.
- DC: `make FP_DEBUG_SELECT=1` 0 warnings; `make cdi` → build/fpdc.cdi 348 MB (DV1–DV4, ui/, 67 ADPCM BGM, 60 sfx). The disc rule now copies ui/, bgm/ and sfx/.
- **Emulator (Flycast):** press start → title menu → L+R → developer select → DV1 with HUD; title → file select ("Press START to begin a new game.", NORMAL) → options (Normal / 100 % / 70 %) → Time Attack → Dragon Valley stage select → STAGE 1 Dragon Valley card → level with HUD. Audio is not judged from screenshots. One run lost all key input (an Xvfb/focus race); restarting the display fixed it.

### Failing comparisons: first failing frame
| Sequence | First out of tolerance (fpdc vs original) | Cause |
|---|---|---|
| boost_air_right | B @f100 (0.000 vs 12.000) | verified: landing on a 38° up-slope at boost speed, f172 (gsp 7.897 vs 11.878); formula not recovered |
| boost_upleft | B @f60 (0.000 vs -0.664) | suspected: pre-boost walk start differs by a frame |
| dk_kick_cyc_hold | B @f134 (0.000 vs 1.386) | suspected: kick-phase speed decay (moving_frames off by ~3) |
| dk_kick_then_cyc | B @f98 (5.000 vs 4.631) | suspected: kick-phase B (5.000 vs 4.631) |
| dk_kick_then_cyc2 | B @f134 (0.000 vs 1.386) | as dk_kick_cyc_hold |
| dk_kick_then_cyc3 | B @f134 (0.000 vs 1.386) | as dk_kick_cyc_hold |
| dk_wind_then_cyc | B @f98 (5.000 vs 4.631) | as dk_kick_then_cyc |
| energy_pickups_fullhp | X @f91 (80.715 vs 68.715) | suspected: boost fires one frame early after crystal pickups |
| gc_run_down_jump | B @f142 (5.000 vs 5.074) | suspected: slope term during grounded cyclone (B 5.000 vs 5.074) |
| gcyc_steer_back | X @f125 (317.147 vs 322.482) | suspected: steering back during grounded cyclone |
| hurt_boost_petal | B @f188 (0.000 vs -2.200) | verified: Turretus shot not implemented |
| hurt_then_petal | B @f188 (0.000 vs -2.200) | verified: Turretus shot not implemented |
| hurt_turretus | B @f188 (0.000 vs -2.200) | verified: Turretus shot not implemented |
| rot_run_left | Y @f50 (546.250 vs 353.739) | suspected: old script without TP, start position/terrain differs |
| snd_hurt_pickups | B @f188 (0.000 vs -2.200) | verified: Turretus shot not implemented |
| snd_moves | B @f150 (2.000 vs 0.000) | suspected: landing 1 frame different after a cyclone |
| spring_yellow_upleft | X @f61 (5338.311 vs 5361.000) | verified: hanging gimmick (M41) not implemented |
| turretus_jumpinto | B @f60 (0.000 vs 0.664) | suspected: Turretus contact/bounce not implemented |

## 2026-10-22/23 Session 4: recovering logic from the executable
Labels: original-trace, host-test, emulator. Nothing hardware-validated.

- **Tooling:** Ghidra 11.4.2 headless on an unanalysed import (`tools/re/decomp.sh`, ~10 s per function); gdb hardware watchpoints and `WHO` probes in the tracer; call graph and qualifier tools. Output stays in `build/re/` (never committed).
- **Trace hygiene:** probes drained the original's petals and lives (globals that survive the stage restart), so eight re-recorded traces briefly started at 0 petals and showed deaths. The tracer now resets petals to 14 and lives to 5 at frame 0, and those eight were re-recorded from a fresh game. Two sequences that had "passed" before (turretus_jumpinto, turretus_landon) fail on the valid traces.
- **Comparisons:** **57/70** (was 52 at the start of the session), **16/16 baseline**; strict **2524 / 10471 exact frames (24.1%)**, new ratchet `tests/orig/strict_baseline.txt` with no per-sequence regressions. Fixed this session: hurt_turretus, hurt_then_petal, hurt_boost_petal, snd_hurt_pickups, spring_yellow_upleft, dk_wind_then_cyc.
- **Host tests:** gameplay **38/38** (new: turretus_pellet_hit, camera_snaps, camera_lookahead, hang_grab, max_fall_15_25); front end **9/9**; valgrind on the title→level cycles: 0 errors, 0 definitely lost.
- **Exact matches (original-trace vs host-test):** Turretus D = 1 at row 162 and shot at row 187 (identical animation counter values); the pellet hit frame and knock-back velocities; 30/30 pellet positions (replayed PinballMovement model); camera after a teleport.
- Animation agreement (`tools/orig/anim_match.py`): animation number 87%, number + frame 29%. The Chowdren counter rule is applied, but the jump animation frame is driven by vertical speed (not ported) and the intro shifts the idle phase.

### Failing comparisons: first failing frame
| Sequence | First out of tolerance (fpdc vs original) | Cause |
|---|---|---|
| boost_air_right | B @f100 (0.000 vs 12.000) | verified: landing angle routine (event 1590) not ported: N 29.74 vs ours 36.87 at f172 |
| boost_upleft | B @f60 (0.000 vs -0.664) | suspected: start-slope angle (original 4.0856 deg = atan 1/14) |
| dk_kick_cyc_hold | B @f134 (0.000 vs 1.386) | as dk_kick_then_cyc2 |
| dk_kick_then_cyc | B @f100 (5.355 vs 4.844) | verified: ground angle at the jump (~352 deg in the original) differs |
| dk_kick_then_cyc2 | B @f134 (0.000 vs 1.386) | suspected: friction stop timing on the slope (angle) |
| dk_kick_then_cyc3 | B @f134 (0.000 vs 1.386) | as dk_kick_then_cyc2 |
| energy_pickups_fullhp | X @f91 (80.715 vs 68.715) | suspected: boost fires one frame early after crystal pickups |
| gc_run_down_jump | B @f142 (5.000 vs 5.074) | suspected: ground angle at the jump |
| gcyc_steer_back | X @f125 (317.119 vs 322.482) | suspected: steering back during the grounded cyclone |
| rot_run_left | Y @f50 (546.250 vs 353.739) | suspected: old script without TP (start position) |
| snd_moves | B @f150 (2.000 vs 0.000) | suspected: landing one frame different after a cyclone |
| turretus_jumpinto | B @f60 (0.000 vs 0.664) | suspected: landing/collision after the teleport below the rock (sensor system) |
| turretus_landon | X @f50 (1200.681 vs 1233.000) | verified: the original pushes Lilac up out of the rock she is teleported into (loop_player01up/down push-out not ported) |

## 2026-10-23 Session 5: sensors, title, items, monitors, loading screen, move overlays
Labels: original-trace, host-test, emulator. Nothing hardware-validated. No audio device in the emulator VM, so audio was checked only through the host music/SFX trace, not by ear.

- **Comparisons (original-trace vs host-test):** **69/70** (boost_upright fails: a Ganseki Jr. enemy hits Lilac in the original; that enemy isn't ported), **16/16 baseline** (re-recorded), strict **10033 / 10471 exact frames (95.8%)**, up from 24.1%. Sensor collision (events 1584-1611) and horizontal speed (1621-1634) are ported.
- **Host tests:** gameplay **39/39** (new: cyclone_energy_drain), front end **11/11** (new: loading_silences_audio, loading_screen_crystal_bottom_right).
- **Host checks (scripted):**
  - Monitors in DV1: crystal monitor (4824, 432) throws 7 crystals and 3 are collected by walking through.
  - The invincibility monitor (6224, 856) starts m_invincibility; stage music resumes after 1800 frames.
  - The 1-up monitor (7952, 912) plays m_1up and resumes after 240 frames (INFERRED length).
- **Bugs found:**
  - The monitor slot was freed before the reward, so thrown items spawned at x = 0.
  - The DC stream volume was set before `snd_stream_start`, which resets it to 255.
- **Audio overlap root cause:** blocking loads starve the stream refill thread while the AICA loops its ring buffer. The fix is no stream during loads, plus the silent loading screen.
- **Emulator (Flycast, build/fpdc.cdi):**
  - The title animation runs and Press Start leads to the menu.
  - The loading screen shows the crystal bottom-right in two successive shots with different frames.
  - DV1, DV2, DV3 and DV4 load from the developer select and play. Crystals were collected in DV3 (counter 198), and pause works.
  - Act transitions DV1→DV4 were checked on the host only, not played through in the emulator.

## 2026-10-23 Session 6: front end from the original, pause, VMU saves and LCD
Labels: original-trace, host-test, emulator. Nothing hardware-validated. Audio not audible in the emulator VM.
- **Host tests:** front end **24/24** (rewritten: Classic route, Time Attack route, options save/size/reload, pause open/clamp/freeze/quit, VMU crystal + life icon, three title→level→pause-QUIT cycles); gameplay **39/39**.
- **Comparisons:** cmp_all **69/70, strict 10033/10471 (95.8%)**, unchanged (no regression).
- **Front-end comparisons (cmp_fe, original vs host, same input script):**
  - **Classic route (r_classic):** all sound ids and frame changes match frame for frame.
  - **Time Attack route (r_ta):** 7 sound frames and 4 frame changes match. The stage-select confirm cursor is 3 frames late (first failing frame **f539**: original A = 2, host 0 until f542). f641 is a trace-boundary artefact.
  - **Pause (t_pause, original only):** 63 at open, 64 per move, 56 on QUIT, 51 frames to the frame change. The host test checks the same ids and a 50-frame wipe.
- **Save:** 515-byte payload, 5 blocks on the VMU including the 3-frame icon.
- **Emulator (Flycast, build/fpdc_lean.cdi, `rend.FloatVMUs`):**
  - **Classic route:** title → hub → FILE 1 → mode → character select (Carol dimmed with a padlock, Milla padlocked) → Lilac → DV1.
  - **Pause:** open; cursor moves; resume.
  - **Time Attack route:** character select → stage select (Shang Mu shown, Dragon Valley startable) → START?/YES → DV1 with the timer → pause QUIT → back to stage select.
  - **Options:** music volume 100 → 70, OKAY. `FPDC.SAV` appears on VMU A1 (parsed: 5 blocks, 3 icons, crystal icon, payload FPS v1 music 70). After a reboot, Options shows 70 %.
  - **VMU LCD:** crystal in the menus, life head + "×5" in DV1.
- **Found in the emulator and fixed:** Carol's lock overlay was a translucent rect on the host but opaque black on PVR (r_rect forces alpha in the PT list). Now the portrait is dimmed with vertex colour.
- **Not checked:** audio by ear; the BIOS file manager view of the save icon (the HLE BIOS has none; the icon was decoded from the VMU image instead); real VMU hardware.

## 2026-10-23 Session 7: TA regression, loading pipeline, ladders, platforms, Ganseki
Labels: original-trace, host-test, emulator. Nothing hardware-validated. Audio not audible in the VM.
- **Time Attack regression (Lilac and enemies invisible, jump played the spring sound):** stale object files compiled
  against the old `game_t` layout were linked with new ones (the Makefile had no header dependencies), so fields
  were read at the wrong offsets. Fix: `-MMD -MP` dependency tracking in both Makefiles; clean rebuild.
- **Loading (Flycast, emulated time; same stages, same disc layout apart from packing):**

| Load | Before | After |
|---|---|---|
| Front end | 17.7 s | 0.05 s |
| Character select | 17.2 s | 0.07 s |
| TA stage select | 8.9 s | 0.03 s |
| DV1 | 41.1 s | 0.24 s |

  Caveat: Flycast does not rate-limit GD-ROM DMA, so the "after" times are too short. A model using GD-ROM seek and
  transfer rates estimates ~1.2 s for DV1 on hardware (INFERRED, not measured).
- **Comparisons:** cmp_all **76/76**, strict **11311/11377 (99.4%)** (was 10033/10471). Exact: boost sequences, cyclone,
  ladder_grab_climb, ladder_down_jump, ladder_top_enter, push_wall. ladder_fast_grab 91/151 exact, within tolerance (1 px Y).
- **Host tests:** gameplay 0 failures, front end 0, pack 0. Host renders of the ladder traces show the climb, climb-over
  and climb-down sprites.
- **Emulator:** DV1-DV4 load from the developer select with no VRAM exhaustion (largest free 191/191/895/191 KB).
