# fpdc: Freedom Planet → Sega Dreamcast (native port, work in progress)

A native KallistiOS runtime and an asset pipeline that converts **your own** copy of Freedom Planet into
Dreamcast-ready data. No game data is stored in this repository.

**Status:** a playable vertical slice (Lilac, one original test level built from real game art, a hornet
enemy, crystals, spikes, checkpoint, goal, SFX and stage music). It runs and is tested on the host
(Linux reference build). The Dreamcast build and the emulator boot are being validated; see
`PROJECT_STATUS.md`. This is **not** a full port.

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
D-pad/stick move · **A** jump (press again in the air: Dragon Cyclone) · **X** attack ·
hold/release **B** Dragon Boost (aim with the D-pad) · **Start** pause

## Layout
`src/` engine (platform, renderer, resources, physics, characters, game) · `tools/` converters ·
`data/` original test level · `host/` Linux test build · `tests/` regression tests ·
`third_party/fp-assets` (public domain) · docs: `HANDOFF.md`, `PROJECT_STATUS.md`, `TODO.md`,
`RESEARCH_LOG.md`, `DECISIONS.md`, `ASSET_PIPELINE.md`, `BUILD_AND_TEST_LOG.md`

Stages: Dragon Valley 1-4 (Lilac, Carol, Milla) and Aqua Tunnel 1-4 (Milla, session 12).

Fan project, not affiliated with GalaxyTrail. Freedom Planet assets belong to their owners.
