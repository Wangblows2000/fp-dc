# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Rule: evidence from the running original first, then match it. Label evidence RE (read from code) / RV (original trace) / INFERRED; host tests and emulator runs are labelled as such. **Nothing is hardware-validated.**

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0). **environ.sh patch**: `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before environ_base.sh.
- mkdcdisc: third_party/mkdcdisc/builddir/mkdcdisc. Flycast: third_party/flycast/build/flycast, driven by `tools/flycast_run.sh` (x=A c=B s=X Return=Start, f/v = L/R; developer select = `key:f key:v` on the title).
- Original game: writable copy /home/claude/fprun (never the user's folder). /home/claude/fprun_user = the same game
  with the user's PC saves (Carol unlocked); for traces and reference only, never shipped or read by the port.
  Carol traces: `CHAR=2` (on foot) / bike via the fuel can; scripts tests/orig_carol, CSVs build/orig_traces/carol.
  Milla traces: `CHAR=4` (any frame; `FRAME=16` = Aqua Tunnel 1 for the dig); `<f> SETV others E 9999` makes her
  invulnerable (the host understands the same line). `tools/orig/start_orig.sh` → `tools/orig/route_dv1.sh` → `FRAME=<20 dv1 / 21 dv2 / 22 dv3 / 23 dv4> tools/orig/run_trace.sh <script> <out.csv> <frames>`. `XVT_EXTRA="name=0x<vtable+0x10>#<instance>"` logs extra objects. A teleport far from the start can be blocked by the camera bounds (sensors are clamped to them).
- Trace scripts: `<f> TP x y`, `<f> EN v`, `<f> HP v`, `<a> <b> KEYS` (a 1-frame range sends nothing; use ≥ 2 frames).
- Host-only debug script commands: `<f> MON <type>` (shield monitor ahead of Lilac), `<f> HURT <dx>`.
- User PC: `C:\Users\wobbles\Desktop\Freedom Planet` (read-only input); deliverables go to its `fpdc\` subfolder.

## Pipeline
See ASSET_PIPELINE.md (levels, UI, font, audio). Then:
```sh
for l in "21 dv1" "22 dv2" "23 dv3" "24 dv4"; do set -- $l; tools/build_level.sh $1 $2 converted2; done
make -C host && python3 tests/test_gameplay.py && python3 tests/test_frontend.py && python3 tests/test_carol.py && python3 tests/test_milla.py
Q=1 tools/orig/cmp_all.sh && python3 tools/orig/cmp_carol.py && FR=295 python3 tools/orig/cmp_milla.py   # against the original's traces
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

## Session 10 (2026-10-24): Carol, DV2 plate, boss HUD
Brief order: DV2 physics puzzle, boss HUD, full Carol, Carol performance, testing/docs. Details: RE_NOTES "Session 10"
and "PC save files", RUNTIME_MAP "Session 10", ASSET_PIPELINE "Session 10", DECISIONS D-036 to D-039,
BUILD_AND_TEST_LOG "Session 10".
- **DV2 plate:** a resting pushable block presses the plate (4971; reach 32 px above, D-036), the door sinks (RV).
- **Boss HUD:** the unused guiboss bar is gone; the hit tally sits bottom right.
- **Carol (playable on character select, Time Attack and files):** her own movement table, pounce, Wild Claw combo
  and big slash, Wild Kick, roll and ball jump, wall cling/jump, voices and sounds, tail overlay, VMU life icon.
  Bike from fuel cans: engine loop, air jump, nitro, ramming, wall ride, dismount/parked bike/remount, 3-hit wreck,
  exhaust. Red jump pads in DV2-DV4 (Lilac gets crystals from them, as in the original). 53 original traces.
- **Audio:** per-character sound groups at level load (D-037): the full set had overflowed AICA RAM in Flycast.
- **VRAM:** split atlases (D-038) for Carol's banks and the DV1-DV3 object banks.

## Session 11 (2026-10-24): Milla, 4 bpp overlays
Details: RE_NOTES "Session 11", RUNTIME_MAP "Session 11", ASSET_PIPELINE "Session 11", DECISIONS D-040 to D-044,
BUILD_AND_TEST_LOG "Session 11". Specs (build/re, not committed): spec_milla_moves.md, spec_milla_combat.md.
- **Milla (playable on character select, Time Attack and files):** Lilac's movement table, Puppy Float, Phantom Cube
  (charge, carry, throw, burst), Barrier Shield, Shield Burst, Super Shield beam with its recoil, dig state (zones only),
  voices, HUD panel, life / VMU icon, sounds as their own group. 41 original traces.
- **Differences between the brief and the original (v1.21.5, RE + RV):** the shield neither blocks nor reflects
  projectiles; the "Cube Blaster" is the thrown cube; DV has no dig zones; the cube does not press the DV2 plate (only
  pushables, the bike and the player's foot sensor do).
- **VRAM:** 4 bpp overlays and split atlases for Lilac's banks (+300 KB free with Lilac in DV1).

## Milla: verified vs approximated
- **Matched to original traces (RV):** walking / running, jump and release cut, float (start, rise, meter, end, refloat,
  walk-off, springs incl. Down), cube charge / cancel / appear / carry / throw / burst timing, shield (walk, decay,
  tap, hold), shield burst and its two hits on the Turretus, super shield and beam incl. every recoil trace (ground,
  up, down, air, air down, wall), rock breaking by the burst and the beam, hurt through the shield, idle.
- **Small known differences:** the cube thrown at the Turretus bursts 2 frames late (1 px of held-cube height); the
  beam's first hit is 1 frame early; flap-sound cadence (frame-time timer in the original).
- **RE only:** shield on springs / ladders, dig rewards (spots of Aqua Tunnel / Relic Maze), float under water (blocked
  by the swim rule), recoil under a ceiling (no low ceiling found near the DV1 start).
- **Not ported:** lifting / throwing monitors and fuel cans (D-044), Aqua Tunnel / Relic Maze themselves (Milla's
  Classic route starts there in the original: D-040), alternative palettes, idle voice variants.

## Latest verified
- host-test: gameplay 72/72, front end 30/30, Carol 19/19, Milla 33/33. original-trace vs host: cmp_all 76/76, strict
  11311/11377 (99.4%, unchanged); cmp_carol 53 traces, 43 exact in X/Y/B/G; cmp_milla 41 traces, 36 exact.
- emulator (Flycast, session 11 build): Milla DV1 at 603 / 602 vblanks per 600 frames after the boot window (float,
  cube, shields, beam); VRAM free after load DV1 Lilac 2541 / Carol 1981 / Milla 2181 KB, DV2 1707 / 1179 / 1347 KB; no
  AICA crash with any sound group. **Emulator only; nothing measured on real hardware.**

## Next actions
1. Real hardware: run the session 11 disc and read the `perf:`/`prof` lines over serial (or watch for stutter); listen
   to Carol's and Milla's sounds and the bike engine loop.
2. Milla: monitor lifting (2202-2205, 2230, 2233, 2246); Aqua Tunnel 1 as her first stage (dig spots, D-040).
3. Carol traces still to record: under water, the 3-hit wreck, death with the bike, DV3/DV4 pads (incl. the DV4 block
   pad 5356), bored idles; sync the original's cross_seed in the tracer to compare random voices.
4. Flycast shows a +2.5 ms frame period about once per second in every scene; confirm on hardware whether it exists.
5. Trace the DV4 boss exit in the original; finish the consecutive DV1→DV4 route search.

## Carol: verified vs approximated
- **Matched to original traces (RV, cmp_carol, 53 traces):** on-foot movement, pounce chains, roll and roll jump, crouch
  lunge, wall cling/slide/jump/top/away, claw combo and big slash incl. hits, Wild Kick incl. air and roll kicks, energy;
  bike riding, jump, air jump, nitro, dismount, remount, claw and kick on the bike, ram, wall ride, wall jump, skid; fuel
  can pick-up; DV2 jump-pad chain and walking onto a pad. Fields per frame: X, Y, B, G, L, M, OTHERS.Y, tail
  animation and frame, skin A/L, energy, sound ids.
- **Traced with known harness differences:** 7 traces that place Carol by teleport next to the DV1 Turretus get the
  pellet hit 1-2 frames off (from frame ~146; the same as Lilac's TP traces); c_fuel_dv1 rams a Dragonfly one frame
  apart (enemy phase); b_remount and c_jumppad_dv2 differ only in G on a frame where the original's G is a display value.
- **RE only (no trace):** 3-hit wreck and debris, DV3/DV4 pad tables, death/fall with the bike, Carol under water.
- **INFERRED / not ported:** see RE_NOTES "Session 10" (sample 413 stop, claw pitch, DV4 block pad 5356, the 2443 Y12
  timer, bored-idle variants, spin-shield angle, debris direction, bike monitor smash path, dust step rounding).
  Random voices and pitches use the port's own seed, so they differ from a given original run.

## Known issues / INFERRED
- **Open (session 9):** the lower waterfall is slightly darker than the original in DV1; Tredd's rock break sound (13)
  is INFERRED from the DV1 rocks; the DV4 exit is RE-only (no original trace).
- **INFERRED:** water tint extent (drawn full width below the surface, also over left foreground rocks); shield carry-over
  between acts; spinning-log phase (global counter offset); breath bubbles' SKIN condition; stage-select confirm timing.
- **Not ported:** gallery, the original's game-over card, enemies' own water splashes, Milla's monitor lifting.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/, tools/audio/wav2adpcm are gitignored. Never commit game data or decompiled output (`git ls-files` check before commits).
