# Research Log

Ledger categories: **CONFIRMED** (verified on the files / reproducible test), **STRONGLY INFERRED**,
**UNVERIFIED**, **UNSUPPORTED** (tested, incompatible).

## 1. The supplied game build

| Finding | Status | Evidence |
|---|---|---|
| Install is the **GOG Windows** release (gameId 1207667013, buildId 51530401073578372) | CONFIRMED | `goggame-1207667013.info` |
| Version is **v1.21.5**, *not* the 2014 launch build (v1.0.x) | CONFIRMED | Image 91 in Assets.dat renders the string `v1.21.5` |
| `FP.exe`: PE32 i386 GUI, 26,705,920 bytes, sha256 `0b2b373c…7b88` | CONFIRMED | `file`, sha256sum |
| Runtime is **Chowdren** (Clickteam Fusion → C++ transpiler) on **SDL2** | CONFIRMED | Strings `Chowdren.exe`, `CHOWDREN_SDL_DEBUG`, `CHOWDREN_SHOW_DEBUGGER`, SDL2 assert/log strings; zero `PAMU`/`PAME` Fusion runtime headers |
| Fusion event logic is compiled to native x86 code (no event bytecode to decompile) | STRONGLY INFERRED | Chowdren architecture + absence of Fusion data blocks; Fusion group/function names present as plain strings (`Player01_Interruption_Control`, `Plaftorms_UpdatePosition`, `sound_jump`, …) |
| Windows exe has **no game symbols** (no `Frames::on_frame_N_init`, `create_*`), only MSVC std RTTI (30 `.?AV` entries) | CONFIRMED | `strings` / RTTI scan |
| `Assets.dat` md5 `4085c983fb918703ce459f17f69c474c` = fp-assets' "latest Windows" profile | CONFIRMED | md5sum; fp-assets source |
| Assets.dat layout: preload 34,324 B, then offset table; 17,162 images, 475 sounds, 0 fonts, 95 shader pairs, 18 text files, 5 type sizes; Chowdren LZ-style image compression | CONFIRMED | fp-assets extraction succeeded in 35 s |
| Image meta = 4×u16: (hotspot x, hotspot y, action-point x, action-point y), i.e. Fusion hotspot/action point | CONFIRMED | Lilac frames: hotspot at feet (e.g. 21,35 on 42×37); values signed (65488 = −48) |
| Sounds: Ogg Vorbis, mostly 44.1 kHz stereo, 4-byte meta (always `02 00 00 00` in samples seen) | CONFIRMED (sample of 61) | ffprobe |
| No sound or image **names** in Assets.dat; images are not grouped by animation (interleaved by original Fusion handle order) | CONFIRMED | Contact sheets `build/sheets/` |
| Music: 68 Ogg Vorbis files in `Data/BGM` (44.1 kHz stereo), no LOOPSTART tags (loop points live in code) | CONFIRMED (m_stage1 probed) | ffprobe |
| Voices: 1,373 Ogg files in `Data/voices` | CONFIRMED | directory listing |
| `files/file_1..17.txt`: INI-style object layouts (`[PROPERTIES] objects=N life=6`, `[OBJECT_n] id= x= y=`) | CONFIRMED | Probably Battlesphere / challenge room layouts — **purpose UNVERIFIED** |
| Main stage layouts (terrain pieces + object placements) are emitted as code in `Frames::on_frame_N_init` | STRONGLY INFERRED | freedom_editor's approach on the Linux build; absent from Assets.dat |
| Physics constants appear as `double` literals next to event-group names in `.rdata` (e.g. `?333333` patterns near `sound_jump`) | UNVERIFIED | Pattern seen at file offset ~23,771,000; not decoded |

## 2. Tool compatibility matrix

| Tool | Purpose | Input | Compatible with this build? | License | Decision |
|---|---|---|---|---|---|
| **fp-assets** (snickerbockers, local copy + github) | Extract/repack Assets.dat | Assets.dat (md5-keyed profiles) | **YES** – auto-identified, full extract (images→PNG, ogg, shaders, text) | Public domain | **USED** (pipeline stage 1) |
| **freedom_editor** (github snickerbockers/freedom_editor) | Dump frame/object placement via radare2 | 64-bit **Linux** `bin64/Chowdren` ELF with symbols | **NO** – relies on ELF symbols `Frames::on_frame_%d_init`, `create_*_N`; Windows PE is stripped | see repo LICENSE | **UNSUPPORTED on this install**; becomes usable if user supplies the GOG **Linux** build. Also needs Python 2.7 + r2pipe |
| **CTFAK** (github CTFAK/CTFAK) | Decrypt/dump Clickteam Fusion EXEs | Fusion runtime EXE with PAME/PAMU data | **NO** – no Fusion data blocks in a Chowdren-compiled exe | AGPLv3 | **REJECTED** (not built: C#/.NET + Windows deps, and nothing for it to read) |
| **Nebula** (CTFAK successor family) | Same, Fusion 2.5+ | Fusion EXE/APK/data | **NO** for same reason | — | **REJECTED** (not tested further; same input requirement) |
| **Chowdren** runtime source | Reuse runtime | — | Proprietary, not public | — | **Strategy A rejected** |
| **UndertaleModTool** | GameMaker data.win | GameMaker data files | **N/A** – no GameMaker artifact present | GPLv3 | **NOT APPLICABLE** |
| radare2 / Ghidra / Capstone | Targeted binary analysis | PE32 | Not yet needed | — | Optional, for physics-constant recovery |
| **KallistiOS** (git 5f834c1, 2026-10-20) | DC SDK | — | Building | BSD-style | **USED** |
| kos-chain stable: binutils 2.45.1, GCC 15.2.0, newlib 4.6.0.20260123 | SH-4 toolchain | — | Building from source | GPL (tools only) | **USED** |
| **mkdcdisc** v0.0.4 (gitlab simulant, 2b98b0d) | ELF+data → CDI/GDI | ELF | Built from source OK | see repo | **USED** |
| **Flycast** (github flyinghead, fa9d39a) | Emulator | CDI/GDI | Cloned, building | GPLv2 | **USED** for validation |

## 3. Asset identification (manual, by contact sheet)
- Lilac main block ≈ images 5510–5700 (interleaved with other objects).
- Dragon Valley look-alike art: rock fill 9/117, sky 3032, mountains 3034, forest 3036, log platform 2.
- Hornet enemy 12569–12614; explosion 360, 363–365; red crystal 47.
- **Animation frame order is UNVERIFIED** (chosen by eye); the true sequences are in compiled code.

## 4. Open questions
1. Recover animation tables + frame layouts: requires the Linux build (symbols) or targeted x86 analysis of `create_*` equivalents.
2. Recover physics constants (double literals in `.rdata`) and verify against footage.
3. Map sound indices to names (none in archive; maybe via code references to sound handles).
4. Identify the 2014 v1.0 build if the user owns it (fp-assets knows md5 `f14f24317d…`, zlib images, 16,893 images) – matters for "original release" fidelity.
