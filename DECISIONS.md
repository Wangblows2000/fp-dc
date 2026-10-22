# Decisions

**D-001 Strategy C (custom runtime) + reused extraction.**
Chowdren's source is not public and the game logic is native x86 code, so the runtime can't be reused (A),
and there's no event data to interpret (B). A focused C runtime driven by converted data is the only
practical route. Alternatives: hand-porting by decompiling the x86 (huge, legally risky). Revisit
B-style data recovery only for level layouts and animation tables.

**D-002 Logical 320×240, scaled ×2 to 640×480.**
The original renders 424×240 (16:9). The DC is 4:3, so this crops ~52 px each side. An alternative is
letterboxed 424×240 → 640×362. We chose crop to keep the native 2× pixel scale. Revisit if horizontal
view matters for gameplay.

**D-003 PowerVR punch-through list, ARGB1555 non-twiddled textures, nearest filtering.**
1-bit alpha matches the pixel art; PT needs no sorting. Twiddling and VQ are future optimizations.
Atlas per sprite bank (Lilac 512×512, objects 256×256).

**D-004 Host build with a software reference renderer.**
The same game code compiles for Linux with `plat_host.c` + `render_soft.c`. It runs headless with
scripted input, dumps frames, and prints a per-frame state trace. This gives deterministic regression
tests without an emulator.

**D-005 Physics: Sonic-style ground speed with heightmap tiles.**
Constants (acc 0.125, top 6, grav 0.21875, jump 6.5, …) are **placeholders, UNVERIFIED**. The real
values are compiled into FP.exe. Planned: recover them from the `.rdata` doubles and compare against
recorded footage.

**D-006 Test level is original (hand-authored ASCII, `tools/make_test_level.py`).**
Real stage layouts aren't in Assets.dat (see RESEARCH_LOG). It uses original FP art.

**D-007 Music: raw s16le stereo 22,050 Hz streamed from disc through `snd_stream`.**
This avoids porting an Ogg decoder for now (~5 MB/min). Future: AICA ADPCM (4:1) or libtremor.

**D-008 Data on the CD filesystem (`/cd/data`), no romdisk.**
Music alone exceeds RAM, so everything is loaded per level from disc.

**D-009 Control mapping (port design, UNVERIFIED vs original defaults):**
A = jump, X = attack, B = special (Dragon Boost), Y = guard (reserved), Start = pause. D-pad or
analog stick to move.

**D-010 One data directory per level (`converted2/<lvl>/`).**
palettize.py builds the 4 PAL8 palettes per level, so each level dir carries its own palettes and Lilac bank (≈5 MB). Shared sounds/music stay at the data root; `levels.txt` lists built levels. Level change frees all textures (`r_reset` → `pvr_mem_reset`) and reloads.

**D-011 Tunables follow the original's traces, in `src/fp.h` (`PHYS_*`), each tagged RV/UNVERIFIED with the evidence.** Where traces contradict the knowledge base (e.g. Dragon Cyclone continues after releasing jump), the trace wins.
