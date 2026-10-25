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

## Session 8 (2026-10-23/24)
- **Original traces recorded** (scripts tests/orig_extra_*.txt): tredd 1-3, snake_t1, dv4_chase, dv4_results, dv4_boss,
  spiral_dv1, spiralb_dv1, balltunnel_dv3, pblock_dv2, plus four DV3 water traces (spec_water.md §7).
- **Host vs original:** Tredd 1100/1100 frames; DV2 snake 271/271; results timings exact; spiral bottom ride, DV3 ball
  tunnel, pushable block exact per frame (TP offset 2-4 frames); water entry/walk/swim/leap/re-entry values exact.
- **Host tests:** gameplay 0 failures (new: spiral, ball tunnel, pushable block, card persistence, shields, water,
  drowning), front end 0. cmp_all 76/76, strict 11311/11377 (99.4%).
- **DC build:** 0 warnings. Flycast: DV1-DV4 load and render from the developer select; pvr free after load
  2046/1211/1326/1144 KB (16:9). Found and fixed: parallax `<lv>_par.bin` was not copied onto the disc; translucent
  sprites were drawn opaque on the PVR (punch-through list ignores vertex alpha).
- **Sound:** 72 original ids + 16 voices loaded at boot (~1.25 MB SPU RAM for the ids). Not audible here (no audio device).
- **Route search:** DV1 cleared incl. Wretchnid (f6039); consecutive DV2-DV4 search in progress.

## Session 9 (2026-10-24): rendering order, DV4 boss exit, audio, performance
Labels: original-trace (RV), RE, host-test, emulator (Flycast, emulated time). **Nothing measured on real hardware.**
No audio device in the VM: audio was checked through the host sound-id trace, the DC stream log and an offline
decoder model, not by ear.

- **Transparency / draw order:** cause: deferred translucent commands were replayed with the end-of-frame depth.
  Fix: per-command depth. Flycast DV1/DV2 waterfalls and water strips now stay behind the foreground and the HUD.
  Remaining: the lower DV1 waterfall is slightly darker than the original (open).
- **DV4 boss exit:** host test dv4_exit_rises_to_surface (46 air / 101 grounded frames on the surface),
  dv4_exit_camera_on_surface, dv4_exit_results_then_menu (FP_SN_AUTOKILL). RE only; no original trace of the exit.
- **Sounds (original trace vs host):** original monitor break = 21 (hit), 307, 12 on one frame (traces mon, inv);
  host now 21 30 307 12 (30 = the port's attack hit, unchanged). Rocks: original 13; host 13 x7 in the spiral trace
  replay, no 80/81/82. Lilac's hurt (player_damage) unchanged. Tests: invincibility_monitor_shatter_sound,
  rock_break_sound_not_hurt.
- **Invincibility left-channel loudness:** model of the AICA decoder (no leak) over two loops of m_invincibility.adp:
  before, the 2nd loop had rms 14595 vs 6978 (low-nibble channel) and 18910 clipped samples (12918 on the other);
  after sealing (adpcm_loop_seal.py) the 2nd loop is bit-identical to the 1st on both channels. All 67 tracks sealed
  (+5..66 bytes each). Flycast DC log: `music: loop bgm/m_invincibility.adp (762464 bytes)` at the full sealed length.
- **Performance (Flycast, demo discs, 600-frame windows; vblanks = pvr_get_vbl_count, 600 = no frame missed):**

| Scene (demo) | Build | update avg | player avg / max | vblanks per 600 frames |
|---|---|---|---|---|
| DV3 standing (demo43) | session 8 (perf line) | 18.5 ms | — | — ("slow" 599/600) |
| DV3 standing | FP_PERF_BASELINE (old sensor scan) | 18.1 ms | 11.5-12.8 / 26.6 ms | 730-840 |
| DV3 standing | session 9 | 8.8 ms | 2.6-3.0 / 6.6 ms | 603 |
| DV1 route (DV1 + DV2 start) | FP_PERF_BASELINE | — | 5.3-6.9 / 28.4 ms | 603 |
| DV1 route | session 9 | ~3.5 ms | 1.9-2.4 / 13.9 ms | 603 |
| DV2 route (from act start) | FP_PERF_BASELINE | — | 5.0-6.3 / 14.9 ms | 603 |
| DV2 route | session 9 | ~4 ms | 2.0-2.6 / 8.3 ms | 603 |
| DV4 to boss (chase, boss) | FP_PERF_BASELINE | — | 3.8-4.6 / 16.6 ms | 602-603 |
| DV4 to boss | session 9 | ~3-5 ms | 1.1-1.9 / 6.5 ms | 602-603 |

  The first window of every run includes boot and loading (662-673). draw (CPU part) 1.5-2.1 ms, list submit < 0.11 ms,
  ~190-230 polygons; the PVR is not the limit. Peak single sections: objects_update 10-14 ms on the frame a monitor
  opens the invincibility music (music_play opens a file) and in the DV4 boss.
- **Residual jitter (emulator, open):** about once per second a frame period is 19.2 ms instead of 16.7 in every scene,
  including with ~12 ms idle per frame and with music disabled; the time appears in whichever section runs then, so
  it is not game code. Net effect 2-3 extra vblanks per 600 frames. Needs a real-hardware check.
- **Stable 60 fps claim:** emulator only — 600 frames rendered in 602-603 vblanks in every measured window of DV1-DV4
  including the DV4 boulder chase and boss and the DV1→DV2 route. Not measured on hardware.
- **VRAM (pvr free after load, Flycast):** DV1 2046 → 2302 KB, DV4 1144 → 1656 KB, DV3 1709 KB, DV2 1467 KB
  (piece atlases after the first capped at 1024x512, D-035). Host renders before/after pixel-identical: DV1+DV2 route
  280 frames, DV3 30, DV4 94.
- **Regression:** gameplay 0 failures, front end 0, cmp_all 76/76, strict 11311/11377 (99.4%), unchanged by every
  performance change. DC build 0 warnings. Session 9 disc (lean BGM) boots to the file menu in Flycast.

## Session 10 (2026-10-24): DV2 plate, boss HUD, Carol
Labels: original-trace (RV), RE, host-test, emulator (Flycast, emulated time). **Nothing measured on real hardware.**
No audio device in the VM: sounds were checked through the host and original sound-id traces, not by ear.

- **DV2 plate / boss HUD:** host tests dv2_block_presses_plate, dv2_door_sinks_when_pressed (door 1016 → 1144 at
  8 px/frame); DV4 hit tally bottom right, no guiboss bar.
- **Carol against the original (tools/orig/cmp_carol.py, 53 traces, 5682 compared frames):** X 99.3 %, Y 99.2 %,
  B 98.4 %, G 98.1 %, L 99.8 %, M 99.8 %, OTHERS.Y 99.7 %, tail anim 99.7 %, tail frame 97.7 %, skin A 99.5 %,
  energy 100 %. 43 traces match X/Y/B/G on every frame; the other 10 diverge only from a known cause: 7 teleport traces
  get the Turretus pellet hit 1-2 frames off (frame ~146), c_fuel_dv1 rams a Dragonfly one frame apart, b_remount and
  c_jumppad_dv2 differ in G on a display-only frame. Sound ids: misses/extras are random voice and claw-pitch picks
  (unsynced seed) and the pellet timing.
- **Host tests:** test_carol 19/19 (energy, pounce, claw, kick, roll, wall cling/jump, fuel can, bike air jump, ride,
  dismount/remount, 3-hit wreck, nitro, DV2 pad chain, Lilac's pad crystals, Lilac unchanged); test_gameplay 72/72,
  test_frontend 30/30.
- **Lilac regression:** cmp_all 76/76, strict 11311/11377 (99.4 %), identical to session 9. Host renders of Lilac and
  Carol (DV1-DV4, 20 frames each) pixel-identical before/after the atlas splits.
- **Found in the emulator and fixed:**
  - AICA crash at boot (ARM CPUUndefinedException) with Carol's sounds added: 1812 KB of samples. Fix: per-character
    sound groups (D-037); largest free block 389 KB (Lilac) / 351 KB (Carol).
  - DV2 with Carol had 123 KB of VRAM free after load. Fix: split atlases (D-038).
  - Bike exhaust invisible (particles always drawn from the object atlas); now drawn from their bank, translucent,
    drifting (RE EffectsDust01).
- **Performance (Flycast, demo discs, 600-frame windows after the boot window; vblanks 600 = no frame missed):**

| Scene (demo) | VRAM free after load | update (player + obj + plat + gimmick) | draw CPU | vblanks / 600 |
|---|---|---|---|---|
| DV1 Lilac run | 2238 KB | ~4.5 ms (2.7 + 0.7 + 0.9 + 0.2) | 1.6 ms | 603, 603 |
| DV1 Carol on foot | 1981 KB | ~4.3 ms (2.6 + 0.7 + 0.8 + 0.2) | 1.6 ms | 603 (621 in the window with an act restart) |
| DV1 Carol on the bike | 1981 KB | ~3.7-4.4 ms (2.2-2.7 + 0.7 + 0.7-0.9 + 0.2) | 1.6 ms | 603, 603 |
| DV2 Lilac (pads area) | 1403 KB | ~5.7 ms (3.1 + 1.0 + 1.0 + 0.5) | 1.7 ms | 603 |
| DV2 Carol (pad chain, then wall) | 1147 KB | ~6.2 ms (3.0 + 1.7 + 0.9 + 0.5) | 1.6 ms | 603 |

  draw = game_draw incl. waiting for the PVR (8.8-11.2 ms of it is the wait). The boot window is 664-675 vblanks
  for every disc. Carol costs about the same CPU as Lilac; the DV2 runs end in different places (Lilac stops at
  x 2983, Carol at 3350), which explains the object-update difference. Peak player section ≤ 6.6 ms in all runs.
- **DC build:** 0 warnings. Emulator only; real hardware still untested.

## Session 11 (2026-10-24): Milla, 4 bpp overlays
Labels: original-trace (RV), RE, host-test, emulator (Flycast, emulated time). **Nothing measured on real hardware.**
No audio device in the VM: sounds were checked through the sound-id traces, not by ear.

- **Milla against the original (tools/orig/cmp_milla.py, FR=295, 41 traces, 9170 compared frames):** X 95.8 %, Y 99.4 %,
  B 98.8 %, G 98.4 %, L 99.8 %, M 99.6 %, OTHERS.Y 99.6 %, animation index 98.3 %, direction 99.3 %, cube state 99.7 %,
  cube position 98.5 %, float meter 99.9 %, energy 100 %. 36 traces match X/Y/B/G on every frame, including every
  recoil trace (ground neutral / up / down, air neutral / down, into a wall). The other 5: 4 teleport traces next to
  the DV1 Turretus without invulnerability (the pellet hit is 1-2 frames off, as for Lilac and Carol) and one jump at
  the level's left edge (B differs by 0.5 for one frame). Known small differences: the cube thrown at the Turretus
  bursts 2 frames later in the port (1 px of held-cube height); the beam's first hit 1 frame earlier; flap sound 170
  cadence (the original's timer uses frame time).
- **Host tests:** test_milla 33/33 (select + Time Attack, energy, float rise / duration / sounds / release, no double
  jump, cube charge / cancel / carry / throw / burst, shield, burst, super beam, recoil -7 / down / up / wall, hits on
  the Turretus, rock breaking, hurt voices, cube drop, dig only in zones, HUD frames, Lilac / Carol unchanged);
  test_gameplay 72/72, test_frontend 30/30, test_carol 19/19.
- **Lilac / Carol regression:** cmp_all 76/76, strict 11311/11377 (99.4 %, unchanged); cmp_carol 43 ok / 10 known
  (unchanged). Host renders (DV1-DV4, 20 frames per character) pixel-identical before/after the 4 bpp overlays.
- **Found in the emulator and fixed:** a 300 ms frame (objects section) when the beam was near rocks: the rotated
  hitbox scanned all 10240 beam pixels against every rock / monitor; now only the overlap rectangle (D-042).
- **Performance (Flycast, demo_milla_dv1: float, cube, throw, shield, burst, super beam aimed down, run):** windows
  after boot 603 and 602 vblanks per 600 frames; player 2.4-2.6 ms avg (max 5.7), objects 0.66-0.82 ms avg, draw CPU
  ~1.6 ms. VRAM free after load: DV1 Lilac 2541 / Carol 1981 / Milla 2181 KB; DV2 Lilac 1707 / Carol 1179 / Milla
  1347 KB. Sound RAM largest free block: Lilac 380, Carol 351, Milla 375 KB.
- **DC build:** 0 warnings. Emulator only; real hardware still untested.


## Session 12 (2026-10-24): Aqua Tunnel
Labels: original-trace (RV), RE, host-test, emulator (Flycast, emulated time). **Nothing measured on real hardware.**
No audio device in the VM: sounds were checked through the sound-id traces, not by ear.

- **Baseline at the session start:** test_gameplay 72/72, test_frontend 30/30, test_carol 19/19, test_milla 33/33;
  cmp_all 76/76, strict 11311/11377 (99.4 %); cmp_carol 43/53 exact; cmp_milla 36/41 exact.
- **End of session:** the same numbers for every DV suite (gameplay 72/72, frontend 30/30, carol 19/19, milla 33/33,
  cmp_all 76/76 strict 11311/11377, cmp_carol 43/53); cmp_milla 37/42 (new m_lift_throw_dv1 matches X/Y/B/G/L/M/OTHERS.Y
  on every frame); new test_aqua 19/19 (Milla-only routes, act transitions AT1→AT2→AT3→AT4, worm head holds the gate
  open, Chomper swallow, snake defeat → results → raft exit → front end, Lilac and Carol through every act).
- **Aqua Tunnel against the original (tools/orig/cmp_at.py, Milla, 35 traces):** 21 match X/Y/B/G on every compared
  frame and their tracked objects (slimes, fireflies, doors, gate, plates, pipes, rafts, slime floor, worm head,
  Chomper head and lure, snake head, carried / thrown bomb). The other 14 are listed in HANDOFF "Aqua Tunnel: verified
  vs approximated" (TP + jelly-launch artifacts, small timing differences, AT2 intro).
- **Performance (Flycast, lean disc, demo `LEVEL atN` + hold right, Milla; 600-frame windows after the boot window):**
  AT1 603, AT2 603, AT3 602, AT4 603 vblanks per 600 frames. Averages: AT1 player 3.4 ms / objects 1.45 / draw 8.8;
  AT2 4.1 / 2.4 / 7.1; AT3 1.8 / 0.56 / 13.6; AT4 4.7 / 0.73 / 9.3. VRAM free after load: AT1 2570, AT2 1672, AT3 3347,
  AT4 2816 KB. Sound RAM largest free block with Milla and the AT group: 324 KB. No AICA crash.
- **DC build:** 0 warnings.

## Session 13 (2026-10-25): bug fixes, reusable objects and levels, save v3
Labels: original-trace (RV), RE, host-test, emulator (Flycast, emulated time). **Nothing measured on real hardware.**

- **Baseline method:** a git worktree of the session-12 HEAD (/home/claude/fpdc_head) with its own Aqua Tunnel data
  built by its importer; the trace comparers (cmp_all, cmp_carol, cmp_milla, cmp_at) now run the checkout they live in,
  so both sides were compared per trace (sound columns aside). Earlier "21/35" for cmp_at at the session start was
  measured with stale data; the session-12 HEAD gives 20/35 with fresh data.
- **After every refactoring step (levels.c, class+handle types, switches.c, AT gates removed, stage flow as data,
  registry, presence gating, save v3):** every trace line identical to the HEAD baseline: cmp_all 76/76, strict
  11311/11377 (99.4 %); cmp_carol 43/53; cmp_milla 38/43 (37/42 at HEAD + the new m_hang_dv1, which matches X/Y/B/G/L/M/OTHERS.Y on all 104 frames); cmp_at 21/35 (a2_redup, which
  needs the variant types in the object groups, matches again: HEAD 20). One intermediate regression (a2_redup, the new
  AquaBrickPlatform_803 type missing from the obstacle group) was caught by this diff and fixed; the registry then
  replaced the hand-written lists (identical for all 256 types, FP_DUMP_OBJDEFS).
- **Host suites (end):** test_gameplay 72/72, test_frontend 30/30 (save checks moved to v3), test_carol 19/19,
  test_milla 33/33, test_aqua 19/19, new test_levels 23/23 (DV1 completed by an input-only route, cards for Lilac / Carol / Milla in DV1 and AT1, load orders
  AT→DV→AT and DV→AT→DV2→AT3, restart, act change, Milla / Lilac / Carol hang animations, results panels drawn in DV4
  and AT4, registry vs importer, next acts, AT1 checkpoint restart, UI masks / translucent layers), new test_saves
  19/19 (Time Attack record of stage 1 for Lilac in ms, files untouched by Time Attack, size 1292 bytes, slower run
  keeps the time, runs under 10 s ignored, story block / last stage / progress / records, only its file, AT = stage 0
  in Milla's slot, deaths in the block, AT1 card id 21 saved and reloaded, damaged save ignored, v2 → v3 conversion).
- **Save model vs the user's PC saves (RV-file, tests/check_user_saves.py, read in place, nothing copied):** 23/23:
  in all five files the totals equal the sums of the per-stage blocks (rows 6+5s..8+5s), the last stage has a block,
  rows 9+5s / 10+5s and the scratch rows are 0; records.dat is 20x100, its crystal records are at least every file's
  stage block for the file's character, the progress rows are stage indices.
- **Visual checks (host renders, RV against original screenshots taken with tools/orig/goto_frame.sh):** AT1 card
  "STAGE 1 / Aqua Tunnel", DV1 card "Dragon Valley" for all three characters; AT4 and DV4 results panels; Time Attack
  previews for Lilac / Carol (Dragon Valley) and Milla (Aqua Tunnel) with padlocks only on locked slots; checker column
  and translucent bottom bar in Character Select and Stage Select match build/orig/orig_chsel_s13.png and
  orig_tasel_s13.png; HUD of DV1-DV4 for Lilac, Carol and Milla, DV4 Serpentine fight without a bottom-right counter.
- **Bosses:** DV4 Serpentine (FP_SN_AUTOKILL) and AT4 Chomper Snake (BOSSHP 0) defeats run their results and exits in
  the host tests; the bosses are not beaten by normal play in automated tests (limitation, unchanged).
- **DC build:** 0 warnings.
