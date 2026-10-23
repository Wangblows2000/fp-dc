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

## Latest verified (2026-10-23, session 7)
- host-test: gameplay, front end and pack tests 0 failures. original-trace vs host: 76/76, strict 11311/11377 (99.4%).
- Fixed the Time Attack regression (stale objects; Makefiles now track header dependencies).
- Loading: DV1 41.1 s → 0.24 s in Flycast (hardware model ~1.2 s, INFERRED). Profiler: src/game/loadprof.c; packs: src/resources/pack.c.
- Ladders (grab, fast catch, climb, climb-over, enter from the top, jump/drop), moving/valley platforms, bridges, Ganseki Jr, push.
- Emulator: DV1-DV4 load with no VRAM exhaustion.

## Tracer probes (tools/orig/trace_orig.py, trace_fe.py)
TRACE_ADDOBJ / TRACE_CREATE / TRACE_FANIM / SHOTS / REFIND as before. `trace_fe.py` logs every object of the current frame (x, y, anim, alterables) plus globals, sounds, music and set_frame. The script keys are `LRUDJXBSE` and `SHOT`.

## Next three actions
1. Results screen (DV4 events 4560-4589): file-slot progress and deaths in the save.
2. DV3 floating platforms and waterfall logs (unmapped object types); Ganseki crystal absorption/spin; full hit-stop.
3. A trace that really exercises the fast ladder catch; hardware timing of loads.

## Known issues / INFERRED
- **INFERRED:** stage-select confirm timing (3 frames late); results layout; card timings; pause QUIT target from a file game (hub); handlebar release.
- **Not ported:** gallery (inert), finished-file stage select (idx 11), mahjong; Carol and Milla (locked, D-020).
- **Other:** FP_PROFILE overlay visible bottom-left in DC builds.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data (`git ls-files` check before commits).
