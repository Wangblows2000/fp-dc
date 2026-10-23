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

## Latest verified (2026-10-23, session 5)
- host-test: gameplay 39/39, front end 11/11.
- original-trace vs host-test: **16/16 baseline, 69/70 overall, strict 10033/10471 (95.8%)**. The only failure, boost_upright, is an unported enemy (Ganseki Jr.).
- Ported this session (RE_NOTES.md / RUNTIME_MAP.md):
  - sensor collision and horizontal speed;
  - title screen events 307-330;
  - crystal/petal pickup; monitors (crystals, petals, 1-up, invincibility);
  - hair overlay; corrected Lilac animation map (skid 16, hurt 8, fastrun 23);
  - move overlays (cyclone/boost/invincibility sparkles); cyclone energy drain;
  - DV4 music m_boss;
  - silent loading screen.
- emulator (Flycast): title, loading screen, DV1-DV4 boot and play. Audio not audible in the VM.

## Tracer probes added (tools/orig/trace_orig.py)
- `TRACE_ADDOBJ=a32bd0,a32e00` logs every object creation with its caller event.
- `TRACE_CREATE=<create_fn>` logs the caller of a create_ function.
- `TRACE_FANIM=<vtable+0x10>` logs force_animation and set_direction on a class.
- `SHOTS=f1,f2` takes screenshots at those frames.
- XVT entries can be added for runtime objects with `<f> REFIND`. They now include the object's x and y.

## Next three actions
1. Ganseki Jr. and the other DV enemies; obstacle-group objects (platforms) in the sensor test.
2. Results screen (DV4 events 4560-4589) and the Serpentine boss (DV4).
3. Thrown-item motion and the 1-up jingle length from the original (both INFERRED now); boost white trail check against `build/re/fx` shots.

## Known issues / INFERRED
Results layout, card timings, transition stagger, pause cursor and the handlebar release are INFERRED. Walk/run anim speeds fitted. Down-kick → cyclone sequences differ by ~3 frames. GALLERY not implemented; only Lilac playable. FP_PROFILE overlay is visible bottom-left in DC builds.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data (`git ls-files` check before commits).
