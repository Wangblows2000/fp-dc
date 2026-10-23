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

## Latest verified (2026-10-23, session 6)
- host-test: gameplay 39/39, front end 24/24. original-trace vs host: 69/70, strict 10033/10471 (95.8%), 16/16 baseline.
- Front end after Press Start ported from the original's events (RE_NOTES "Front end after Press Start", RUNTIME_MAP "Front-end screens in the port"). The code is in `src/game/fe_hub.c` (title menu, file/mode select, delete, options), `frontend.c` (Press Start, character select, shared wipe/tiles) and `fe_stsel.inc` (Time Attack stage select).
- Pause from DV1 events 4878-4881/4296-4305 (`game.c pause_*`).
- VMU: `src/game/save.c` (FPDC.SAV, 515 bytes, 5 blocks) and `src/game/vmu.c` (LCD crystal in menus, life icon in stages).
- Front-end tracer: `tools/orig/run_fe.sh <script> <out.jsonl> <n> <watch offsets> [goto idx]` → `tools/orig/cmp_fe.py <orig.jsonl> <host_stdout>`. The host run uses FP_TRACE_SID=1 FP_TRACE_SND=1 FP_FE_TRACE=1. `tools/re/member_map.py` (gdb) maps a frame's member offsets to classes.
- Emulator (Flycast, lean CDI): Classic route, Time Attack route, options persisted across reboot, pause, VMU LCD. Audio not audible in the VM.

## Tracer probes (tools/orig/trace_orig.py, trace_fe.py)
TRACE_ADDOBJ / TRACE_CREATE / TRACE_FANIM / SHOTS / REFIND as before. `trace_fe.py` logs every object of the current frame (x, y, anim, alterables) plus globals, sounds, music and set_frame. The script keys are `LRUDJXBSE` and `SHOT`.

## Next three actions
1. Results screen (DV4 events 4560-4589): write file-slot progress and deaths to the save at stage completion. Today only options and Time Attack records are saved.
2. Ganseki Jr. and the other DV enemies; the Serpentine boss.
3. The stage-select confirm 3-frame offset (f539); the stage-select BG parallax; the controls panel if a non-console layout is ever wanted.

## Known issues / INFERRED
- **INFERRED:** stage-select confirm timing (3 frames late); results layout; card timings; pause QUIT target from a file game (hub); handlebar release.
- **Not ported:** gallery (inert), finished-file stage select (idx 11), mahjong; Carol and Milla (locked, D-020).
- **Other:** FP_PROFILE overlay visible bottom-left in DC builds.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data (`git ls-files` check before commits).
