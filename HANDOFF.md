# HANDOFF

## Objective
Native Dreamcast (KallistiOS) port of Freedom Planet. A custom C engine plays data **recovered from the user's Linux build** (the original Chowdren runtime is compiled C++ and proprietary). Working to prompt v3: evidence from the running original first, then match it.

## Environment (ephemeral cloud VM; rebuild if missing)
- KOS git 5f834c1 at /opt/toolchains/dc/kos, toolchain /opt/toolchains/dc/sh-elf (GCC 15.2.0). **environ.sh patch**: `export KOS_LD_SCRIPT=-T$KOS_BASE/utils/ldscripts/shlelf.xc` before environ_base.sh.
- mkdcdisc: third_party/mkdcdisc/builddir/mkdcdisc. Flycast: third_party/flycast/build/flycast, driven by `tools/flycast_run.sh` (x=A c=B s=X Return=Start).
- Original game: writable copy /home/claude/fprun (never the user's folder). `tools/orig/start_orig.sh` (Xvfb :93, 852x480, full window visible) → `tools/orig/route_dv1.sh` (real menus to DV1) → `tools/orig/run_all.sh` (traces) / `tools/orig/goto_frame.sh N` (jump + screenshot).
- User PC: `C:\Users\wobbles\Desktop\Freedom Planet` (read-only input); deliverables go to its new `fpdc\` subfolder.

## Pipeline
```sh
B="/mnt/user-data/uploads/Freedom Planet/game/bin64/Chowdren"
python3 third_party/fp-assets/fp-assets.py -x -f ".../game/Assets.dat" -m tools/fmt_linux_9d00124a.json extracted/linux
python3 tools/re/dump_frames.py "$B" build/frames; python3 tools/re/dump_anims.py "$B" build/frames/anims.json
python3 tools/re/dump_backdrops.py "$B" build/frames/backdrops.json
python3 tools/convert.py extracted/assets ".../Data/BGM" converted          # legacy: sfx_*.wav + stage1.pcm
for a in "21 dv1" "22 dv2" "23 dv3" "24 dv4"; do sh tools/build_level.sh $a converted2; done   # converted2/<lvl>/
make -C host && python3 tests/test_gameplay.py && Q=1 tools/orig/cmp_all.sh
source /opt/toolchains/dc/kos/environ.sh && make FP_DEBUG_SELECT=1 && make FP_DEBUG_SELECT=1 cdi
```

## Latest verified (2026-10-22)
- **original-trace vs host-test: 16/16 sequences pass** (run/release/skid/jumps/running jump/slopes+quarter-pipe/ground+air attack/uppercut/high uppercut/down kick/cyclone/boost tap/hold/running boost/crouch).
- Host tests 16/16. DC build 0 warnings. **Emulator (Flycast):** debug level select → DV3, DV4 (and back via pause → X) load and play.
- All stage frames identified by their title cards (RUNTIME_MAP "Frame index table").

## Next three actions
1. Damage/knockback + spring sequences in the original (needs a route to an enemy/spring; `goto_frame` + walk) → compare; energy meter rules (boost cost/recharge).
2. Checkpoint 2: HUD from original graphics per docs/reference/hud_dv1.png (energy meter, petals, lives, crystals, timer), title/options menus, D-002 crop decision.
3. Music library pipeline (68 BGM, loop points, ADPCM vs libtremor), then objects/enemies for DV2–DV4.

## Known issues
Boost directions other than flat ground untested vs original; air charge behaviour UNVERIFIED; walk/run anim speeds fitted; cyclone has a safety cap; parallax factors guessed; no checkpoint/boss; music loops the whole track; HUD is a placeholder; 4:3 crop.

## Safety
Never write to the user's game folders. extracted/, converted*/, build/, docs/reference/ are gitignored. Never commit game data (`git ls-files` check before commits).
