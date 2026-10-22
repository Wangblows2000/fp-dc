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

## Latest verified (2026-10-22, session 3)
- host-test: gameplay 33/33, front end 9/9 (incl. 3 title→level→title cycles, valgrind clean).
- original-trace vs host-test: **16/16 baseline**, **52/70 overall**; first failing frames in BUILD_AND_TEST_LOG.
- emulator (Flycast): full front-end path, developer select, DV1 with HUD, Time Attack card.
- Section 2 findings: grounded cyclone, cancels, no 8 s boost cooldown (energy meter gates the boost; petals don't affect it), left boost orientation, rotation. Details in RUNTIME_MAP.
- Section 6 findings: springs (all three kinds), skid/turn rule, landing look-ahead, ceiling frame, invulnerability ≤ 61 frames.

## Next three actions
1. Turretus firing (range/period) → the four hurt_* sequences; then Turretus contact/bounce.
2. Hanging gimmick (M41) and the steep up-slope boost landing (boost_air_right f172).
3. DV1 mini-boss and the DV4 Serpentine; observe the original's real results screen (replace the INFERRED layout).

## Known issues / INFERRED
Results layout, card timings, transition stagger, pause cursor are INFERRED. Invulnerability 60 is an estimate under a measured bound. Walk/run anim speeds fitted. Down-kick → cyclone sequences differ by ~3 frames. GALLERY not implemented; only Lilac playable. FP_PROFILE overlay is visible bottom-left in DC builds.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data (`git ls-files` check before commits).
