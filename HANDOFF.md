# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Rule: evidence from the running original first, then match it. Label evidence RE (read from code) / RV (original trace) / INFERRED; host tests and emulator runs are labelled as such. **Nothing is hardware-validated.**

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0). **environ.sh patch**: `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before environ_base.sh.
- mkdcdisc: third_party/mkdcdisc/builddir/mkdcdisc. Flycast: third_party/flycast/build/flycast, driven by `tools/flycast_run.sh` (x=A c=B s=X Return=Start, f/v = L/R; developer select = `key:f key:v` on the title).
- Original game: writable copy /home/claude/fprun (never the user's folder). `tools/orig/start_orig.sh` → `tools/orig/route_dv1.sh` → `FRAME=<20 dv1 / 21 dv2 / 22 dv3 / 23 dv4> tools/orig/run_trace.sh <script> <out.csv> <frames>`. `XVT_EXTRA="name=0x<vtable+0x10>#<instance>"` logs extra objects. A teleport far from the start can be blocked by the camera bounds (sensors are clamped to them).
- Trace scripts: `<f> TP x y`, `<f> EN v`, `<f> HP v`, `<a> <b> KEYS` (a 1-frame range sends nothing; use ≥ 2 frames).
- Host-only debug script commands: `<f> MON <type>` (shield monitor ahead of Lilac), `<f> HURT <dx>`.
- User PC: `C:\Users\wobbles\Desktop\Freedom Planet` (read-only input); deliverables go to its `fpdc\` subfolder.

## Pipeline
See ASSET_PIPELINE.md (levels, UI, font, audio). Then:
```sh
for l in "21 dv1" "22 dv2" "23 dv3" "24 dv4"; do set -- $l; tools/build_level.sh $1 $2 converted2; done
make -C host && python3 tests/test_gameplay.py && python3 tests/test_frontend.py && Q=1 tools/orig/cmp_all.sh
source /opt/toolchains/dc/kos/environ.sh && make && make cdi        # build/fpdc.cdi (~300 MB)
make cdi DEMO=tests/play/<route>.txt CDI=build/fpdc_demo.cdi        # verification disc: scripted input ("LEVEL dv1" line starts the act)
```
Route search (host): `FP_SEARCH=1 FP_EXPLORE=4000 FP_SEARCH_OUT=out.txt FP_LEVEL=dv1 host/fpdc_host converted2 N script /dev/null 1`; `FP_SEARCH_FROM=<frame>` replays the script before that frame. Run searches from a copy of the binary.

## Session 8 (2026-10-23/24): Dragon Valley completion
Done (details in RE_NOTES "Session 8 additions" and "Shields and water", RUNTIME_MAP "Session 8"):
- Bosses/minibosses: Wretchnid (DV1), Tredd (DV3), Serpentine DV2 encounter and DV4 boss, DV4 boulder chase, DV4 results.
- Traversal: roll M6, spiral helix, DV3 ball tunnel, spinning logs, pushable block, corkscrews, loops; keystones; cards.
- Water (DV2/DV3) with swimming, air meter, drowning, bubble patches; elemental shields (5 types).
- Parallax as 4 bpp chunks; translucent/additive sprites through the PVR translucent list; second atlas for big _ens banks.
- Brief items from earlier in the session: 16:9 letterbox option (save v2), title credit, momentum change, card totals.
- Death restarts the act with one life less; game over returns to the menu.

## Session 9 (2026-10-24): rendering order, DV4 boss exit, audio, performance
Brief order: transparency, DV4 boss sequence, audio, textures/performance. Details: BUILD_AND_TEST_LOG "Session 9",
RE_NOTES "Session 9", DECISIONS D-033 to D-035.
- **Transparency / draw order (PVR):** translucent, additive, tint and invert sprites are deferred to the translucent
  list; they used to be replayed with the depth of the end of the frame, so waterfalls, water strips and tints covered
  the foreground and the HUD. Each deferred command now keeps the depth of its draw call (one fix for every level).
  Waterfall fronts are additive (RE create_waterfallfront_1037 set_shader 0x20); backs use blend alpha 0x9c (RE).
- **DV4 boss defeat:** the cinematic camera (5547) no longer runs the normal camera rules, so the view stays on the
  surface; Lilac rises as the boost ball (5567, angle/state reset so the bowl ceiling cannot hold her), resumes at
  y < 1230 (5570), lands, results tally, menu. Host test `dv4_exit_*`. Not traced in the original (RE spec §9-10 only).
- **Audio:** BGM loop seam sealed (D-033; the left-channel blow-up ~25 s into invincibility); monitors play 307 + 12,
  rocks 13 instead of Lilac's hurt sound 81 (Lilac's own hurt sounds unchanged).
- **Performance:** sensor candidate lists (D-034), hot obj_t fields, second piece atlas ≤ 1024x512 (D-035), DC frame
  profiler in the perf log. Flycast: every level at 602-603 vblanks per 600 frames (was 730 in the DV3 demo).

## Latest verified
- host-test: gameplay 0 failures (new: invincibility_monitor_shatter_sound, rock_break_sound_not_hurt, dv4_exit_*),
  front end 0. original-trace vs host: cmp_all 76/76, strict 11311/11377 (99.4%).
- emulator (Flycast, session 9 build): see BUILD_AND_TEST_LOG "Session 9" for per-level frame pacing, section times and
  VRAM. pvr free after load DV1/DV3/DV4 2302/1709/1656 KB. **Emulator only; nothing measured on real hardware.**

## Next actions
1. Real hardware: run the session 9 disc and read the `perf:`/`prof` lines over serial (or watch for stutter): the
   Flycast numbers are emulated time. Check the BGM loop by ear (m_invincibility after ~24 s, m_stage1 after ~3 min).
2. Flycast shows a +2.5 ms frame period about once per second in every scene, also with 12 ms of idle time and with
   music off, so it is not the game's CPU time; confirm on hardware whether it exists there.
3. Trace the DV4 boss exit in the original (needs the real defeat; the forced setup fired 5547 with the wrong instances).
4. Finish the consecutive DV1→DV4 route search; traces still open: shield icon timing, water tint extent, spinning log.

## Known issues / INFERRED
- **Open (session 9):** the lower waterfall is slightly darker than the original in DV1; Tredd's rock break sound (13)
  is INFERRED from the DV1 rocks; the DV4 exit is RE-only (no original trace).
- **INFERRED:** water tint extent (drawn full width below the surface, also over left foreground rocks); shield carry-over
  between acts; spinning-log phase (global counter offset); breath bubbles' SKIN condition; stage-select confirm timing.
- **Not ported:** gallery, Carol/Milla (locked, D-020), the original's game-over card, enemies' own water splashes.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data or decompiled output (`git ls-files` check before commits).
