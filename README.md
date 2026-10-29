# A partial Freedom Planet Decompilation, targetting Dreamcast.

A native KallistiOS runtime and an asset pipeline that converts **your own** copy of Freedom Planet into
Dreamcast-ready data. No game data is stored in this repository.

## Requirements
- Linux, Python 3 + Pillow, ffmpeg
- KallistiOS + SH-4 toolchain (`$KOS_BASE/utils/kos-chain`, stable profile; set `makejobs=2`, a number)
- mkdcdisc (meson/ninja, libisofs) for disc images; Flycast for emulation

## Build
```sh
source /opt/toolchains/dc/kos/environ.sh
make data FP_GAME="/path/to/Freedom Planet"   # extract + convert (needs your install)
make                                          # fpdc.elf
make cdi                                      # build/fpdc.cdi
make test                                     # host build + gameplay regression tests
```

## Run the host build
```sh
./host/fpdc_host converted 900 tests/scripts/run_right.txt build/frames 30   # PPM frames + state trace
```

## Controls (Dreamcast pad)
D-pad/stick move · **A** jump (press again in the air: Dragon Cyclone) · **B** attack ·
hold/release **X** special / Dragon Boost (aim with the D-pad) · **Start** pause. The original's buttons A / B / C map to
the pad's A / B / X, and the game shows the original's "C" as X (D-110). Menus: A confirms, B goes back.

## Layout
`src/` engine (platform, renderer, resources, physics, characters, game) · `tools/` converters ·
`data/` original test level · `host/` Linux test build · `tests/` regression tests ·
`third_party/fp-assets` (public domain) · docs: `HANDOFF.md`, `PROJECT_STATUS.md`, `TODO.md`,
`RESEARCH_LOG.md`, `DECISIONS.md`, `ASSET_PIPELINE.md`, `BUILD_AND_TEST_LOG.md`

Fan project, not affiliated with GalaxyTrail. Freedom Planet assets belong to their owners.
