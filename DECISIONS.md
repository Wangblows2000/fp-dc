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

**D-020 Front end follows the original's console layout; content limits are shown, not hidden.**
- **Console variant:** the original has a built-in console variant (global G[32] = 1). It hides QUIT, Screen size, Controls and Sensitivity. Those options make no sense on a Dreamcast pad, so the port uses this variant.
- **Characters and stages:** Lilac, Carol (since session 10, D-039) and Dragon Valley exist in the port.
  - Milla appears on character select but is locked.
  - The Time Attack list shows every stage, but only Dragon Valley can be started. Shang Mu (where the cursor starts) plays the refusal sound.
- **Port shortcuts:**
  - GALLERY plays the move sound only.
  - Adventure mode skips its opening cutscene and starts DV1.
  - Continuing a used file starts DV1 (no stage progress yet).
  - Pause QUIT from a file game returns to the hub (INFERRED).
- **Buttons:** menu buttons follow the original: jump/A confirms, attack/B goes back, Start confirms where the original accepts it.

**D-021 One small VMU save, written only on explicit actions.**
`FPDC.SAV` holds the options, 8 file slots and the Time Attack records in 515 bytes (6 blocks with its 3-frame icon). It is written when OKAY is chosen in options and when a new Time Attack record is set, never in the middle of gameplay. Writing to a VMU takes several frames on hardware, so a mid-gameplay write would stall the game.

**D-022 Loading: one pack per screen, pre-twiddled textures, resident Lilac.** Measured with loadprof before changing
anything. Packs remove hundreds of ISO9660 opens; textures are twiddled on the PC; Lilac's banks stay in VRAM across
compatible transitions (CRC-checked, dropped otherwise). No general cache: only the one pack in RAM, freed at loading_end.
**D-023 Mirrored sprites by UV flip.** Halves Lilac/hair VRAM (lilac2 1 MB → 512 KB, hair 512 KB → 256 KB) so DV levels fit.
**D-024 Header dependencies in the Makefiles** after the TA regression caused by stale objects.
**D-025 16:9 letterbox is an option, the logical view stays 426×240.** OPTIONS → SCREEN SIZE (a row the console layout
hides, shown by the port) switches between 16:9 letterbox (×1.5, opaque bars over anything drawn outside the view) and
4:3 fill (×1.5 across, ×2 down). Nothing in gameplay sees the difference. Default: letterbox. Saved in the save (v2).
**D-026 One enemy/boss/gimmick bank per level (`<lv>_ens`).** Sprites the level's events need are picked by
(class, animation, direction) in tools/import_frame.py ENS and loaded by name; palettes share flexible groups.
**D-027 Parallax as 4 bpp power-of-two chunks.** Each image key gets one 16-colour block (`palettize --pal4`), so the
DV backgrounds use ~0.9 MB less VRAM and match the original's tiling exactly (spec_parallax).
**D-028 Momentum (intentional change, brief):** the uppercut slides through its wind-up and keeps 90 % of a faster run speed;
the down-kick wind-up keeps its speed. `FP_ORIG_MOVES=1` restores the original for trace comparisons.
**D-029 Save v2.** Letterbox and per-act card bits are appended; v1 files still load. Writes happen only on explicit
actions or act boundaries: options OKAY, a new Time Attack record, cards at the act change (only when a new card was
taken), the results save at D = 150. Never mid-stage.
**D-030 Death restarts the act with one life less** (keystones and crystal counts restored to the act start); at 0 lives
the game returns to the menu (the original's game-over card is not ported).
**D-031 Every sound a ported event plays is loaded at boot** (61 original ids + 16 voices, ~1.1 MB of SPU RAM as 4-bit
ADPCM). Ids not in the list are silent, so new events must add theirs to `SND_IDS` and `convert_sfx.sh`.
**D-032 Verification demos can start a level directly** (`LEVEL <dir>` line in demo.txt, same path as host `FP_LEVEL`),
so host and emulator replays of a route script start from the same state.
**D-033 BGM files end at the ADPCM decoder's reset state.** The AICA decoder has no leak, so a whole-file loop carries
the end-of-file state (sample, step) into data encoded from (0, 127): wrong offset and step until the clamp wears it
off (m_invincibility: 2nd loop rms x2, ~19 k clipped samples, simulated). `tools/audio/adpcm_loop_seal.py` (run by
convert_bgm.sh) appends a few samples per channel that end both channels at exactly (0, 127) and pads to 32 bytes
(2-4 ms). Chosen over restarting the stream at the loop (an audible gap) and over trimming (cannot reach the state).
**D-034 Sensor queries scan candidate lists, not every slot.** Per group (solid / obstacle / no-edge / obstacle-all)
the slot indices whose type is in the group, in slot order, rebuilt when a slot is allocated or retyped. Every
original test still runs in the same order, so results are identical (cmp_all and host frames unchanged).
`make FP_PERF_BASELINE=1` builds the old full scan for A/B measurements.
**D-035 Piece atlases after the first are at most 1024x512** (tools/import_frame.py; up to MAX_ATLAS = 4). Saves
256-512 KB of VRAM per DV level; host renders of the four levels are pixel-identical.
**D-036 A resting pushable block presses the DV2 plate from up to 32 px above (port deviation, by request).** RE 4971
presses the plate when a resting q14 block overlaps it, but in DV2 the block cannot enter the plate's pit (original
sweep: it always rests on the ledges 28 px above), so in the original only the player can press it. The brief asked
for the block to activate it; the port counts a block covering the plate from up to 32 px above. Door motion is the
original's (RV plate3). Tests dv2_block_presses_plate, dv2_door_sinks_when_pressed.
**D-037 Sounds are loaded per character (supersedes D-031's "everything at boot").** With Carol's ~30 samples the
boot set reached 1812 KB and overflowed AICA RAM (the ARM stopped with CPUUndefinedException in Flycast). Boot loads
the shared set; `snd_group_load()` swaps in the playing character's group at level load (slots reused,
`snd_unload`). Largest free sound RAM block: 389 KB with Lilac, 351 KB with Carol. New character sounds go into
`SND_LILAC` / `SND_CAROL` in game.c and into convert_sfx.sh.
**D-038 Banks split into two atlases when that saves VRAM.** tools/import_frame.py packs a half-height first atlas and
a small second one (`<name>_2.tex`, frames v | 0x8000, drawn through bank_draw / bank_frame_tex) when the content
height is just over a power of two: carol2 1 MB → 576 KB, carolbike 512 → 384 KB, and the DV1-DV3 object banks
1 MB → 576 KB (Carol's fuel can and pads had pushed them from under 512 rows to 552). Applies to Carol's banks and the
level `_obj`/`_ens` banks; Lilac's skin and hair banks need no split. Host renders of Lilac and Carol in DV1-DV4 are
pixel-identical before and after.
**D-039 Carol is a full character, matched to original traces.** Her events run in the original's in-frame order
(carol.inc, carol_bike.inc) and keep the original's counters (OTHERS.Y as cY, PSC.L shared by the kick and the pads),
because trace matching needs the same intermediate values, not only the same moves. Where the static RE disagreed
with a trace, the trace wins and the correction is listed in RE_NOTES "Session 10". Random choices (voices, claw pitch)
use the port's own random numbers. Carol's sounds, VMU life icon, Time Attack records and file slots use the
existing per-character fields; a file saved on the bike resumes on foot.
**D-040 Milla is unlocked and starts at Dragon Valley.** The original locks her until records.dat (2, 42) >= 2 and sends
her Classic / Adventure game to Aqua Tunnel 1 (frame 16, RE 799), which is not ported. The port shows her on character
select (voice 186) and starts her at DV1 like the others; Time Attack works as for Lilac. Revisit when Aqua Tunnel is
ported.
**D-041 Overlays with <= 15 colours are 4 bpp.** Lilac's hair, Carol's tail, Milla's tail and her cube / shield bank use
16-entry blocks in the character's palette group (palettize --pal4 per group). Host renders of Lilac, Carol and the bike
in DV1-DV4 are pixel-identical before and after; Flycast VRAM free +300 KB with Lilac.
**D-042 Milla's cube is not an object in G.o.** PlayerReflect_463 is a single reused instance driven only by Milla's
events; the port keeps it in milla.inc (static state, ticked and collided at the frame start like update_objects /
test_collisions_20) and exposes its hitbox to combat.c / objects.c. Rotated hitboxes (beam, shield) are tested per
pixel only inside the overlap rectangle (performance: a full scan cost 300 ms in one Flycast frame).
**D-043 Dig zones are level data of stages not ported.** The dig state M27 is implemented and verified against the
Aqua Tunnel trace timing; in DV the original never sets MILLA.D, so crouch + attack does nothing (also in the port).
Host tests enter a zone with `<f> DIGZONE 1`.
**D-044 Milla's monitor lifting is not ported.** Attack while standing on a monitor (2246) lifts it in the original; the
port charges a cube instead. Listed in HANDOFF.


## Session 12 (2026-10-24): Aqua Tunnel
**D-045 Aqua Tunnel's play layer is mapped onto the port's layer 2.** AT plays on Chowdren layer 3 (DV: 2), so
tools/import_frame.py maps AT layers 2→1, 3→2, 4→3 (the main layer is the one holding the in-level
playerstartingpoint); the original index stays in `layer0`, and an AT layer-1 piece keeps its 0.8 scroll as piece pad
bit 2. Objects that come in per-layer copies (LevelCrusher1_808, SlimeFloor_857, water jets, waterfalls) carry the
mapped layer in the lvobj anim field: the copy below the play layer is drawn, the play-layer copy collides. DV rebuilds
are byte-identical.
**D-046 Only Milla plays Aqua Tunnel; it reverses D-040.** As in the original (RE 799 / 623-625), Milla's Classic,
Adventure and Continue start at Aqua Tunnel 1, and her Time Attack shows the Aqua Tunnel icon in Dragon Valley's slot;
Lilac and Carol keep Dragon Valley. Milla's dig spots and rewards (spec_milla_moves §6) are live in AT1. Lilac and Carol
are checked through all four acts by host tests only (tests/test_aqua.py).
**D-047 Stage sound groups.** Aqua Tunnel's 14 samples (pipes 62, bombs 172/173, jets 240, crate 278, dig 306, shields
308/316, Iron Worm 242/55/69, Chomper / snake 99/251/252) load with an AT act and are freed when another stage loads,
like the character groups of D-037 (Flycast: 324 KB sound RAM largest free block with Milla in AT1).
**D-048 The AT gimmick block runs after Lilac's movement.** The RE call order puts the AT platform/gimmick events
before the movement call, but against the original's traces (a4_raft, a1_plates, a3_crusher, a2_beam, a2_crusher) the
port only matches with the block in aqua_update(), after player_update() and the object pass: the raft starts and the
door moves on the frame the sensors first reach them. DV's platform block stays before the movement (its traces match).
**D-049 Traces win over the static specs, as in D-039.** Corrections found this session (RE_NOTES "Session 12"): the
Iron Worm head is link 7 of its chain (x 6E, y 7E, F0 = -140) and its chain has run 62 frames when play starts; the
slime's animation counter starts at 50; a finishing animation update leaves the counter unchanged; the firefly patrol
still runs on its defeat frame; snake acid only splashes on the play layer.
**D-050 Monitor-icon break sounds by D.** Monitors break with sound 278 (crate), 12 (rings, keystone, bomb), 308 + 316
(invincibility, shields) or 307 + 12 (others), read from 2623-2626 (RE); test_gameplay's invincibility expectation was
changed to 308 + 316.
**D-051 Milla lifts, carries and throws monitors (supersedes D-044).** spec_milla_monitor_carry.md: attack while the
bottom-big sensor is on a monitor (2246) or a falling monitor on her head (2202) lifts it (M30, F = 1, skin directions
31/15, no sound); 3132 carries it above her action point; attack throws it (3134: X = cos(O)(4Q + B),
Y = -sin(O)(4Q + B)); a hurt / roll drops it (3133). Loose monitors (src/game/monitors.c): the thrown / dropped flight
breaks on background, solids, obstacles or enemies (2751: -15) and leaves EffectsExplosion01; the A = 1 hop lands
with the unstuck loops. The head bump that starts a hop (2605, every character) is enabled in Aqua Tunnel only, so
Dragon Valley's monitors keep their traced session-8 behaviour. RV a4_snake / a4_snake_bomb (Milla catches the falling
bomb at frame 1048) now match.

## Session 13 (2026-10-25): bug fixes, reusable objects and levels, save v3
**D-052 Levels are data (src/game/levels.c).** Shared code reads one row per act (frame, stage index, act count,
music, water, walk-in, exit rule, edge results, start rule, checkpoints, boss camera, item sets, camera bounds, card
ids, pads, dig zones, switch rule, sound group, ...) instead of testing "dv" / "at" names. A new act of an already
ported mechanic is one row; only genuinely new behaviour needs code. Stage-specific scripting (AT2's intro current,
the bosses) stays in its stage file and is selected by the stage index.

**D-053 One engine type per Fusion object (class + handle).** Where two handles share a class name (SwitchDoor 770 /
997, SwitchWallPlate 831 / 1029, AquaBrickPlatform 731 / 803, LevelCrusher1 808 / 847, LevelAxelPlatform 680 / 845 and
737 / 846, GimmickSwingSpawner1 824 / 848) the importer assigns separate types (ENGINE_TYPES_ID), so no runtime code
tests the act to tell them apart. Data format unchanged (lv2 type numbers of the AT levels only).

**D-054 Shared switches with a per-level plate rule.** PressurePlate_769 is the same object in DV2, DV3 and AT1 but
pressed by different events (RE 4971-4973 vs 4093-4096); the rule and the pass (before / after the movement) are level
data, the plate, the Milla block and the doors live in src/game/switches.c. Every door type answers the level's
switches. Each level's original behaviour is kept (cmp_all, cmp_at, test_gameplay, test_aqua identical).

**D-055 Object registry (src/game/objdefs.c).** Qualifier-group membership, image-box collision and the spawn sprite
are per-type data shared by every level (Fusion qualifiers are class properties). Verified identical to the previous
hand-written lists for all 256 types before the switch.

**D-056 Subsystems follow object presence.** The AT passes and the DV platform passes run in every level and act on
their types; a level without any object of a family skips its passes (objects_have / objects_have_from), so Dragon
Valley pays nothing for Aqua Tunnel code and the reverse.

**D-057 Save v3, stage-indexed (supersedes the v1/v2 layout).** Mirrors records.dat + file<n>.sav (spec_records_saves):
records rec[15][4] in ms, per-file blocks per stage (totals are sums, RE 954), progress per character, 100 card ids.
The port has no routing frame 8, so its writes happen once at the results' D = 150 (records 907 / 908 in any mode,
file / progress 954 / 955 in story only). A new game replaces its slot only at its first stage end (the original
writes the file at frame 8 too). v1 / v2 saves convert on load; 1292 bytes, 6 VMU blocks with the 3-frame icon.

**D-058 Menu transparency and solid masks in the UI importer.** Solid quick backdrops are imported as rects (kind 2);
images with one semi-transparent alpha become an opaque part plus a translucent layer drawn with vertex alpha (kind 3),
since the textures keep 1 bit of alpha. The checker layer of the menus wraps vertically (RV screenshots).

**D-059 No boss hit tally on the HUD (supersedes the session-10 "hit tally bottom right").** The original shows no
boss counter (GUIBoss is never visible, U-G1); the port's tally is removed at its source (serp4_hud). Boss logic,
damage, defeat and results are unchanged.

**D-060 The PVR translucent list uses MODULATEALPHA.** With MODULATE the vertex alpha was ignored on the DC, so the
port's vertex-alpha translucency (menus, results panels, cards, whisps, shield particles) only existed on the host
renderer. Alpha 255 draws are identical; Flycast now matches the host and the original's screenshots.

## Session 14 (Relic Maze)

**D-061 Stage flow is level data.** Exits (edge + Y band → target act, with the walk-off speed), entries chosen by the
source act, death / restart targets (RM2 → RM1 with g4 kept), stage flags (ML 0x1 / 0x2 / 0x4), flag-cleared gates,
checkpoints, room camera bounds and room warps (RM5 labyrinth 6731-6750) are fields of the level rows in
src/game/levels.c. game.c reads them; no shared code tests a level name (RE_NOTES "Reusable objects and levels").

**D-062 One engine type range per module; families share a type.** relic.c 232-245 (enemies), relic_gima.c 246-292
(acts 1-2 gimmicks), relic_gimb.c 300-336 (acts 3-5), relic_boss.c 350-371 (golems), rm_claw.c 400-449, rm_mantalith.c
450-499; MAX_OT = 512. Fusion objects with identical rules (e.g. GimmickMovingPillar1/2/3) share one type and carry
their member index in the lvobj anim field (importer RMB_SUB). Each module owns its registry block in objdefs.c.

**D-063 Relic Maze stage sounds at 11025 Hz.** With Milla's sound group and the 64 KB music stream buffer the RM set
at 22 kHz exhausted AICA RAM in Flycast ("no chunks big enough", then a crash); KOS also limits an sfx to 65534
samples. The stage set (SFX_RM, only ids outside the global set, D-047) is converted at 11025 Hz (convert_sfx.sh LOW
list). Mantalith sounds 147, 214 and 215 are left out (their calls are silent). Global sounds are unchanged.

**D-064 Classic routing only.** DV4 / AT4 results continue to RM1 and RM5 ends at the front end (Fortune Night is not
ported). The RM5 Adventure dialog cutscene (group 0x22517) is not played; the Classic path (6865) always runs.

**D-065 Unlock records are not written.** The RM5 card-unlock record (7119), achievement records (6900, 7116 / 7117)
and the gallery are not ported; cards, records, files and progress use the shared v3 save (D-057). The Time Attack
entry for Relic Maze follows the original's progress lock (RE 641-643) and becomes selectable once the stage is cleared.

**D-066 Camera after the Eye Golem (INFERRED).** At the Eye Golem's defeat the boss lock is released to the act's
bounds (5982 intent); the original may keep it (trace T3 of spec_rm_golems).

**D-067 Sensor and pass prefilters (performance, results unchanged).** Player-phase sensor queries reject far objects
from a packed x / y / reach copy (objects_sense_snapshot; superset with 64 px slack, rebuilt per phase and on slot
changes); enemy_type is a per-type table; the Aqua Tunnel passes run only when one of their types is alive
(objects_alive_type). Relic Maze places dig spots, so the AT pass block used to scan every slot ten times a frame.

## Session 15

**D-068 Options are loaded from the save once per boot.** Global 12 (difficulty) is changed by the file screen and the
options; re-reading it on every front-end load reverted a Casual game to the saved Normal after a stage, which looked
like damage turning on and off. Casual keeps the original's no-damage rule (RE 2414).

**D-069 Registry flag OD_HIDDEN for classes whose constructor calls set_visible(0).** Replaces the hard-coded list in
objects.c; applied to the ported collision-only / event-shown classes (RM1 switch column front, minibossbarrier, spiral
front, spinning-log mask). The other ported set_visible(0) classes are not audited yet (TODO).

**D-070 RM2 entrance gates wait for the player to pass (INFERRED).** RE creates them "behind the player" at fade
A ≥ 32; the walk-in speed is UNKNOWN and the port's walk-in stops short, so the gate is created once the player is
28 px past its x (once per visit). Replace with the traced walk-in when an original trace of the RM1 → RM2 entry exists.

**D-071 Gallery and save v4.** The gallery follows spec_gallery (cards = the sound test and art, achievements, data);
save v4 adds the opened state of the 100 cards and 56 achievement states (1361 bytes, still 6 VMU blocks). v1-v3 saves
convert (taken cards face down). Artworks and sound cards load on demand; the achievement pop-up is an overlay instead
of frame 9. Only Card Collector and the cheat's 1-3 are set by the port.

**D-072 Unlock cheat in final builds.** Left x3 then Right x3 on the title screen or title menu (1 s per step, other
input resets). It writes only through the save's own fields (progress, cards, achievements 1-3) and save_store, so a
save made with it is an ordinary v4 save.

## Session 16

**D-073 Quarter-pixel screen offsets on the DC.** Quad edges no longer coincide with sample points for either PVR
sampling convention; tiling stays watertight because shared edges stay identical. Hardware verification pending.

**D-074 Backdrop pieces are placed at the instance position (RE Backdrop::draw).** One importer rule for every level;
it corrects the RM3 caravan (and its roof collision, which the RM3 wreck puzzle needs) and shifts the few other
backdrops whose images carry a hotspot.

**D-075 Basement darkness is a black edge vignette (user request) instead of the original's light image** (shader 20
UNKNOWN). Alpha still follows the original's depth rule, so its scope and clean-up are the original's.

**D-076 VMU LCD shows rotating file-select portraits without lives** (user request). Uses the logic tick; the save file
and its icon are unchanged.

**D-077 Achievements follow spec_achievements; the pop-up waits for a confirm as the original's frame 9 (RV).**
Achievements needing unported stages are listed as out of scope; 18 Switch Skipper is an INFERRED reading.

**D-078 Earth-shield magnet left as is.** The reported persisting pull could not be reproduced; no change without a
reproduction.

## Session 17 (Fortune Night flow)

**D-079 Fortune Night flow is level data plus one stage script.** New generic row fields (start_card, script_exit,
walkin_minx / walkin_free, death_g4, cam_box, ckills / cmarks, mus4/6/7, res_voice / res_wipe); FN-only events (FN3's
Serpentine pass 7710-7725, FN5 8371) live in src/game/fn_flow.c, attached by stage index 3. Classic RM5 -> FN1; FN6's results
end at the front end (Sky Battalion not ported). FN6 deaths go to FN5 with g4 2; without an FN5 checkpoint (debug starts)
FN5 starts fresh (INFERRED; the original's case is UNKNOWN).

**D-080 FN parallax in the character palette group's free top (tools/par_top.py).** FN's backdrops cannot be 4 bpp and
the other palette groups are full; group 2 has 117 free entries in every character palette. Reduced by median cut when
needed (visible only as merged near shades).

**D-081 The stage timer can stop (G.timer_stop, game_timer()).** FN3 7710 switches the timer group off for the exit pass;
the stopped value carries into FN4.
