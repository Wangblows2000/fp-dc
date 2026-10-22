# HANDOFF

## Project objective
Native Sega Dreamcast (KallistiOS) port of Freedom Planet. The original runtime (Chowdren, compiled
C++) can't be reused, so a custom C engine plays data converted from the user's own install. Current
scope: a playable vertical slice → real level data → full game.

## Verified environment (cloud VM, ephemeral; rebuild if missing)
- Ubuntu x86_64, 2 cores, gcc, python3 + Pillow 12.3, ffmpeg, Xvfb, meson/ninja.
- KallistiOS git `5f834c1` at `/opt/toolchains/dc/kos`. Toolchain from kos-chain stable (binutils
  2.45.1, GCC 15.2.0, newlib 4.6.0.20260123) at `/opt/toolchains/dc/sh-elf`. Config: `makejobs=2`,
  objc off.
- mkdcdisc v0.0.4 built at `third_party/mkdcdisc/builddir/mkdcdisc`.
- Flycast `fa9d39a` source at `third_party/flycast`, build dir `build/` (Ninja).
- The user's PC (Windows) is reachable via the device bridge. Game at
  `C:\Users\wobbles\Desktop\Freedom Planet`, staged read-only to `/mnt/user-data/uploads/Freedom Planet/`
  (Assets.dat, FP.exe, Data/BGM/m_stage1.ogg, font.bmp, fp-assets).

## Current implementation (repo `/home/claude/fpdc`, git)
- `tools/convert.py` + `tools/slice_manifest.json`: extracted assets → .tex/.spr/.lvl/.wav/.pcm (deterministic, validated).
- `tools/make_test_level.py` → `data/test1.txt` (original layout).
- `src/`: engine. The platform split (`plat_dc.c` / `plat_host.c`) and renderer split (`render_pvr.c` /
  `render_soft.c`) share `resources/assets.c`, `physics/level.c`, `characters/player.c`,
  `game/objects.c`, `game/game.c`.
- `host/` Linux build, `tests/test_gameplay.py`: 17 regression tests over the frame trace.

## Latest verified build
- Host: `make -C host && python3 tests/test_gameplay.py converted` → 17/17 PASS.
- Dreamcast: see PROJECT_STATUS.md for the latest result.

## Current milestone
M1 toolchain proof + M6 bootable slice. Remaining: KOS lib build, `make`, `make cdi`, Flycast boot and screenshot.

## Known failures / uncertainties
- Animation frame orders, SFX ids and physics constants are UNVERIFIED guesses.
- freedom_editor can't work on the Windows exe (no symbols). Needs the GOG **Linux** build.
- The install is v1.21.5, not the 2014 v1.0.

## Next actions
1. Get the DC build compiling, CDI built and booted in Flycast (screenshot + input test).
2. Ask the user for the GOG Linux build → port `dump_frames_linux_64.py` to Python 3 (radare2 or
   pyelftools + capstone) → real level/animation data.
3. Physics constant recovery + footage comparison.

## Recovery
```sh
cd /home/claude/fpdc && git status && git log --oneline | head
# toolchain missing? cd $KOS/utils/kos-chain && cp Makefile.dreamcast.cfg Makefile.cfg (makejobs=2) && make
source /opt/toolchains/dc/kos/environ.sh && make -C /opt/toolchains/dc/kos
# data missing? stage the game via the device bridge, then:
make data FP_GAME="/mnt/user-data/uploads/Freedom Planet"
make && make cdi && make test
```

## Safety
- Never write into the user's game folder. `extracted/` and `converted/` are regenerable and gitignored.
- Never commit game-derived files (see .gitignore).
