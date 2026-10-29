# A Freedom Planet Decompilation, targetting Dreamcast.
Fan project, not affiliated with GalaxyTrail. Freedom Planet assets belong to their owners.


Through a mixture of decompilation and guesswork - followed by compression and some other optimization bulldada - I present what is effectively a FOSS approximation of Freedom Planet. This project was entirely built with Claude's Opus 5.5, with myself merely play-testing and prompting the majority. Huge credits to the https://github.com/snickerbockers/fp-assets tools for making this possible, as well as making it known to myself that the debug symbols for this game were actually out there.

## "Compression, optimization?"
Graphics were shrunk by converting every texture to 8-bit (and small overlays to 4-bit) indexed colour, sharing palettes across each level, packing sprites into compact texture sheets, and storing them on disc pre-formatted for the graphics chip and LZ4-compressed. Audio was compressed to 4-bit ADPCM with streamed music, and sound effects were split into shared, per-character and per-act sets, the act sets at half rate with silent tails trimmed, so only what the current act needs sits in the 2 MB of sound memory. Anything rarely used, like shield sprites, gallery artwork and results voices, is loaded only when it’s actually needed.

## How much is decompiled?
Roughly 90% decompiled, 10% inferred. The repo has about 45.7k lines of C. About 42k of those are gameplay (game/, characters/, physics/), and in that code about 7,000 comment lines cite the original’s events or collision handlers. Those are recovered from the decompile (RE) or checked against recordings of the original game (RV). Only about 550 lines are marked INFERRED, and about half of those still cite an original event: they fill a gap inside logic that was otherwise recovered. Another 18 are marked UNKNOWN. This is a rough heuristic based on how the code is labelled, not a line-by-line audit.

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

## Layout
`src/` engine (platform, renderer, resources, physics, characters, game) · `tools/` converters ·
`data/` original test level · `host/` Linux test build · `tests/` regression tests ·
`third_party/fp-assets` (public domain) · docs: `HANDOFF.md`, `PROJECT_STATUS.md`, `TODO.md`,
`RESEARCH_LOG.md`, `DECISIONS.md`, `ASSET_PIPELINE.md`, `BUILD_AND_TEST_LOG.md`

