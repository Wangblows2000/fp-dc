# A Freedom Planet Decompilation targetting Dreamcast.
Fan project, not affiliated with GalaxyTrail. Freedom Planet assets belong to their owners.


Through a mixture of decompilation and guesswork - followed by compression and some other optimization bulldada - I present what is effectively a FOSS approximation of Freedom Planet. This project was entirely built with Claude's Opus 5.5, with myself merely play-testing and prompting the majority. Huge credits to the https://github.com/snickerbockers/fp-assets tools for making this possible, as well as making it known to myself that the debug symbols for this game were actually out there.

## Compression, optimization?
Graphics were shrunk by converting every texture to 8-bit (and small overlays to 4-bit) indexed colour, sharing palettes across each level, packing sprites into compact texture sheets, and storing them on disc pre-formatted for the graphics chip and LZ4-compressed. Audio was compressed to 4-bit ADPCM with streamed music, and sound effects were split into shared, per-character and per-act sets, the act sets at half rate with silent tails trimmed, so only what the current act needs sits in the 2 MB of sound memory. Anything rarely used, like shield sprites, gallery artwork and results voices, is loaded only when it’s actually needed. Unfortunately, this is all baked into the codebase, and thus not removable in PC builds. In practice, this means that it runs at ~42MB of RAM on PC (up to 6x lower than retail) with little visual difference. It also means that it runs at a solid 60fps in all stages on a real-hardware Dreamcast, excluding screens wherein multiple large interactive moving sprites are (notably the Dragon Valley final boss and the chase cutscene in Relic Maze).

## How much is decompiled?
Roughly 90% decompiled, 10% inferred. The repo has about 45.7k lines of C. About 42k of those are gameplay (game/, characters/, physics/), and in that code about 7,000 comment lines cite the original’s events or collision handlers. Those are recovered from the decompile (RE) or checked against recordings of the original game (RV). Only about 550 lines are marked INFERRED, and about half of those still cite an original event: they fill a gap inside logic that was otherwise recovered. Another 18 are marked UNKNOWN. This is a rough heuristic based on how the code is labelled, not a line-by-line audit.

## Current issues?
To keep it brief, many adventure-mode cutscenes are extremely rough visually, and some late-stage gimmicks are either missing or visually-lacking.

Status as of 29 Oct 2026: every Adventure-mode story scene the original has is now in (Lilac, Carol and Milla versions,
voiced, skippable from the pause menu, Adventure only). Their visuals are still being polished, and two short pieces are
missing (Brevon's starfield opening before the Zao ambush scenes, the ride before Battle Glacier). An audit of the late
stages fixed the gimmicks it found missing or invisible (Final Dreadnought vanishing blocks, Carol's jump pads, the vacuum
in Final Dreadnought 2b, Jade Creek's rain, springs, pillars, jumbotrons and more). What is still open is listed at the
top of [TODO.md](TODO.md).

## Building for Dreamcast

> **Easy builder:** no tools to install. Drag your Freedom Planet **1.21.5** `Assets.dat` (the GOG Windows one or the
> Linux one) onto **`build_dreamcast.bat`** (Windows) or run **`./build_dreamcast.sh /path/to/Assets.dat`** (Linux).
> It builds everything by itself and writes the disc image **`fpdc.cdi`** next to the script. No compiler is needed: the
> program itself is ready-made in `prebuilt/`. (Unzipped inside your Freedom Planet folder, a double-click is enough: it
> finds the `Assets.dat` by itself.)

1. Download this repository (*Code* > *Download ZIP*) and unzip it.
2. Drag `Assets.dat` from your Freedom Planet folder onto `build_dreamcast.bat` (Windows), or run
   `./build_dreamcast.sh "/path/to/Freedom Planet/Assets.dat"` (Linux).
3. The first run downloads Python and ffmpeg (about 110 MB on Windows, 55 MB on Linux, checksum-verified, kept in
   `build/cache`) and takes 20-60 minutes; later runs only redo what changed. About 3 GB of free disk space is needed.
4. Burn `fpdc.cdi` (in the same folder as the script) to a CD-R (DiscJuggler, or ImgBurn after converting) for a Dreamcast that boots CD-Rs, or open
   it in an emulator such as Flycast.

Your game folder is only read. No game data is stored in this repository: the builder takes the art, sound and music
from your copy. The only game-derived files here are the level layout / animation tables and the story tables (scene list,
dialogue text) in `tables/` (see its README).
Details, troubleshooting and the developer build (KallistiOS toolchain, `make && make cdi`) are in **[BUILDING.md](BUILDING.md)**.

## PC port (Linux and Windows)

A PC version of the same code (SDL2; Linux x86_64 and 32-bit Windows) lives on the
**[`pc-port` branch](https://github.com/Wangblows2000/fp-dc/tree/pc-port)**. It has the same drag-and-drop builders:
drop `Assets.dat` onto `build_win32.bat` to get a working `fpdc.exe`, or onto `build_linux.sh` for a Linux executable.
