# Decisions

**D-001 Strategy C (custom runtime) + reused extraction.**
Chowdren's source is not public and the game logic is native x86 code, so the runtime can't be reused (A),
and there's no event data to interpret (B). A focused C runtime driven by converted data is the only
practical route. Alternatives: hand-porting by decompiling the x86 (huge, legally risky). Revisit
B-style data recovery only for level layouts and animation tables.

**D-002 Logical 320×240, scaled ×2 to 640×480.** *(Superseded by D-012.)*
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

**D-007 Music: raw s16le stereo 22,050 Hz streamed from disc through `snd_stream`.** *(Superseded by D-013.)*
This avoids porting an Ogg decoder for now (~5 MB/min). Future: AICA ADPCM (4:1) or libtremor.

**D-008 Data on the CD filesystem (`/cd/data`), no romdisk.**
Music alone exceeds RAM, so everything is loaded per level from disc.

**D-009 Control mapping (port design, UNVERIFIED vs original defaults):**
A = jump, X = attack, B = special (Dragon Boost), Y = guard (reserved), Start = pause. D-pad or
analog stick to move.

**D-010 One data directory per level (`converted2/<lvl>/`).**
palettize.py builds the 4 PAL8 palettes per level, so each level dir carries its own palettes and Lilac bank (≈5 MB). Shared sounds/music stay at the data root; `levels.txt` lists built levels. Level change frees all textures (`r_reset` → `pvr_mem_reset`) and reloads.

**D-011 Tunables follow the original's traces, in `src/fp.h` (`PHYS_*`), each tagged RV/UNVERIFIED with the evidence.** Where traces contradict the knowledge base (e.g. Dragon Cyclone continues after releasing jump), the trace wins.

**D-012 View 426×240 letterboxed (supersedes D-002).**
The original's HUD is laid out for 426×240 (template-matched on docs/reference/hud_dv1.png): the clock sits at x 322, so a 320 crop would cut the timer. The view is 426×240 scaled 1.5× to 639×360, centred with 60 px bars (OFF_Y 60). 1.5× is not an integer, so pixels are slightly uneven; we accept that to keep the original layout. The old crop remains behind `FP_VIEW_320` for comparison.

**D-013 Music as AICA 4-bit ADPCM, stereo 32 kHz, hardware-decoded (supersedes D-007).**
Measured on the 67 tracks present (8,006 s): .adp total 256 MB (fits a CD with the game data); round-trip SNR 30.4 dB with an AICA-accurate decoder (`tools/audio/adpcm_check.py`; wav2adpcm's own `-f` decoder adds a fake high-pass and reports a misleading 8 dB). Streamed with `snd_stream_start_adpcm` (the AICA decodes, no SH4 cost), polled from a KOS thread off the main loop; switching to the same track does not restart it. Rejected: raw PCM (≈1 GB at 22 kHz stereo, too large); libtremor (Vorbis decode on the SH4 costs CPU every frame). SFX and voices are ADPCM WAV, mono 22,050 Hz, loaded with `snd_sfx_load`. Loops are whole-file because the original has no loop points.

**D-014 Front end built from the original frames' objects.**
`tools/import_ui.py` exports every object of the menu frames with all animations/directions. `frontend.c` drives them by class name (cursor positions, values, strings from the running original). Only options this build supports are shown: Screen Size (PC-only) is hidden, GALLERY does nothing yet, and Carol is shown but not selectable (only Lilac is implemented). QUIT returns to the Dreamcast menu.

**D-015 Developer level select reachable only by L+R within 2 s on the title screen.**
The user required this. `FP_DEBUG_SELECT` is kept as a no-op build flag for old scripts.

**D-016 Unknown original behaviour is labelled, not invented.**
Where the original could not be observed (results layout after a boss, card timings, pause cursor, transition stagger, invulnerability length), the port uses the closest evidence and marks it INFERRED or UNVERIFIED in code and in RUNTIME_MAP.

**D-017 Lilac's physics in doubles.**
The original keeps Clickteam alterables as doubles and floors B, G and N to 4 decimals every frame (event 1574). In float the rounding drifts (−2.9 floors to −2.9001), which cost exact frames. Lilac's position, speeds and angles are now doubles with double constants. The SH4 has hardware double precision and this is a handful of operations per frame. The renderer and the other objects stay float.

**D-018 Recover logic from the executable; never ship decompiled code.**
Behaviour is taken from the original's compiled events (named functions, readable with Ghidra and gdb watchpoints) whenever possible, and confirmed with traces. Decompiled or disassembled output stays in `build/re/` (gitignored); the port is written from the prose and formulas in RE_NOTES.md.

**D-019 Camera from MasterCamera_408 (replaces the reconstruction).**
The recovered dead-zone camera also decides which enemies are active (Turretus counts only within 200 px of the view), so it is a gameplay rule as well as presentation.
