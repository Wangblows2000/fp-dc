# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Rule: evidence from the running original first, then match it. Label evidence as original-trace / host-test / emulator. **Nothing is hardware-validated.**

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0). **environ.sh patch**: `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before environ_base.sh.
- mkdcdisc: third_party/mkdcdisc/builddir/mkdcdisc. Flycast: third_party/flycast/build/flycast, driven by `tools/flycast_run.sh` (x=A c=B s=X Return=Start, f/v = L/R). If keys stop registering, kill the :95 Xvfb and retry.
- Original game: writable copy /home/claude/fprun (never the user's folder). `tools/orig/start_orig.sh` (Xvfb :93; exports `DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent`, without it traces crawl) → `tools/orig/route_dv1.sh` → `tools/orig/run_trace.sh <script> <out.csv> <frames>` / `run_all.sh`. Don't kill gdb while breakpoints are set (the game crashes; restart route + goto_frame 20).
- Trace scripts: `<f> TP x y`, `<f> EN v`, `<f> HP v`, `<a> <b> KEYS`. Sounds go to `<trace>.snd`. Press inputs after ~f40 (intro input lock). Start new scripts with `5 TP 68.715 440`.
- User PC: `C:\Users\wobbles\Desktop\Freedom Planet` (read-only input); deliverables go to its `fpdc\` subfolder.

## Pipeline
See ASSET_PIPELINE.md (levels, UI, font, audio). Then:
```sh
make -C host && python3 tests/test_gameplay.py && python3 tests/test_frontend.py && Q=1 tools/orig/cmp_all.sh
source /opt/toolchains/dc/kos/environ.sh && make && make cdi        # build/fpdc.cdi (~350 MB)
```
Boot flow: Press Start → title (START GAME / TIME ATTACK / GALLERY / OPTIONS / QUIT). Developer level select: L+R within 2 s on the title screen only.

## Latest verified (2026-10-23, session 4)
- host-test: gameplay 38/38, front end 9/9 (valgrind clean).
- original-trace vs host-test: **16/16 baseline, 57/70 overall, strict 2524/10471 exact frames** (ratchet in tests/orig/strict_baseline.txt). First failing frames are in BUILD_AND_TEST_LOG.
- Recovered from the executable (RE_NOTES.md): damage + 60-frame invulnerability, Turretus + pellet (shot and hit on the original's exact frames), camera, animation counter rule, angle smoothing, slope force, landing formula, 4-decimal rounding (physics now in doubles), fall cap 15.25, handlebar grab, landing reach from the bottom sensor.
- emulator (Flycast): unchanged since session 3 for the front end; DV1 with the new camera checked.

## How to recover more logic (see RE_NOTES.md "Method")
`tools/re/decomp.sh 'Frames::event_func_N()'` (Ghidra project build/re/ghidra/fp: create once with `analyzeHeadless build/re/ghidra fp -import <Chowdren> -noanalysis`), `tools/re/annot.py`, tracer `WATCH` / `WHO` commands, `tools/re/callgraph.py`. Re-record any trace after probes from a fresh game (`start_orig.sh` + `route_dv1.sh`).

## Next three actions
1. **Port the sensor system** (`loop_player01detectsensor*`, `loop_player01up/down/slopes*/wallcollision`, angle routines in events 1590 and 1603). It explains most remaining differences: landing angle, start-slope angle, push-out from solids.
2. **Results screen** from the located block (DV4 events 4560–4589, `loop_tally_19`): timeline, positions, tally, sounds.
3. Turretus hit-by-attack (events 4776/4777: A = 8 health), pellet destroy handlers (`col_7_20`, `col_20_20`), handlebar release/move (4057–4066), jump animation frame selection.

## Known issues / INFERRED
Results layout, card timings, transition stagger, pause cursor and the handlebar release are INFERRED. Walk/run anim speeds fitted. Down-kick → cyclone sequences differ by ~3 frames. GALLERY not implemented; only Lilac playable. FP_PROFILE overlay is visible bottom-left in DC builds.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data (`git ls-files` check before commits).
