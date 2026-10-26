# Runtime Map: original Freedom Planet (Linux build, v1.21.5)

Build: `bin64/Chowdren` sha256 `ff514984…` (not stripped); title screen shows **v1.21.5** (same as the Windows install).
Tags: **RV** = runtime-verified (traced in the running game) · **SD** = static-derived (disassembly) · **INF** = inferred.

## Launch and navigation
| Item | Value | Tag |
|---|---|---|
| Headless launch | `tools/orig/start_orig.sh`: Xvfb `:93` 852×480, llvmpipe, dummy audio, writable copy `/home/claude/fprun` | RV |
| Display fix | The window is created 426×240 but the game renders 2× (852×480), so only the bottom-left quarter showed. Resize the window **once**, ~5 s after startup, to 852×480 at (0,0). Resizing earlier leaves a stale viewport (offset or 1× image). | RV |
| Audio files | A missing `Data/BGM`/`voices` ogg crashes the stream thread; silent placeholders with the same names work | RV |
| Menu route to DV1 | `tools/orig/route_dv1.sh`: frames 0 → 82 → 2 → 3 → 81 → 75 → 20. Keys: Return ×2 (skip intro), Return ×3 (Start Game, New File 1 Normal, Adventure), then `z` ×~70 to advance dialogue | RV |
| Frame indices | `Setting frame: N` = `Frames::on_frame_(N+1)_init`. **DV1 = 20** (`on_frame_21`), the stage entered after the intro scene (75) | RV |
| Restart stage | `GameManager::set_frame(manager=0x285ec60, 20)` @0x9fc8c0, called from gdb **outside** breakpoint handlers (calling inferior functions inside `stop()` hangs gdb) | RV |
| Save files | `file1..8.sav` absent → "Could not load data.array" (normal for a fresh install) | RV |

## Main loop and timing
| Item | Value | Tag |
|---|---|---|
| Loop | `GameManager::update()` → `platform_poll_events()` → … → one `Frame::update()` → draw. One logic step per call. Wall-clock time only feeds `FPSLimiter` and timers. | SD + RV (traces advance one logic frame per breakpoint) |
| Per-frame hook | `0x9fcb7a` (just after `platform_poll_events`) | RV |
| Input lag | A key set at frame f first changes state read at the start of frame f+2 | RV |

## Input
| Item | Value | Tag |
|---|---|---|
| Key list | `manager+0x88` count; entries from `manager+0x8c` (0x285ecec): {int SDL keycode, u8 state}. state 0 = pressed this frame, 1 = held, 2 = up | SD + RV |
| Accessors | `is_key_pressed` (state≠2, 15 call sites), `is_key_pressed_once` (state==0, 42 sites), joystick functions (30). `is_player_pressed` / bitmask at manager+0x1b4/0x1b8 are effectively unused | SD |
| Keycodes seen | Right 0x4000004F, Left 0x40000050, Down 0x40000051, Up 0x40000052, z 0x7A (jump, RV), x 0x78, c 0x63 (attack/special mapping: see traces) | RV/INF |
| Injection | `tools/orig/trace_orig.py` rewrites the key list at 0x9fcb7a each frame | RV |

## Object and memory layout
| Item | Value | Tag |
|---|---|---|
| FrameObject | x int @+0x8, y int @+0xc (layer-relative; layers all at 0 in DV1), alterables* @+0x20 | SD + RV |
| Alterables block (pool, 0x128 B) | 10 string slots, then **26 doubles (A–Z) @+0x50** | SD + RV |
| Active animation | s8 @+0xb0 (`Active::set_animation`) | SD |
| Global values | `manager+0x8` → doubles (only #3 changes: frame/time counter) | RV |
| Lilac objects | `Player_326` (control, vtable 0x20b6f20), `PlayerSkinSash_341` (sprite), `PlayerCharacterSensor_375`, **`PlayerMovementValues_253`** (vtable 0x20b2900) = movement state | RV |

## PlayerMovementValues_253 alterables (Lilac, DV1)
| Alt | Meaning | Value / behaviour | Tag |
|---|---|---|---|
| A | ? | 0 | — |
| B | **ground speed** on ground; **air x speed** in the air | — | RV |
| C | **top run speed** | 5 | RV (speed settles at 5.0128 on flat) |
| D | ? (max speed?) | 15 | INF |
| E | **acceleration** | 0.07375 px/f² | RV |
| F | **slope factor** | 0.203125: dB = −F·sin(O) when \|O\| > ~30° | RV |
| G | **air y speed** | — | RV |
| H | ? | 15 | — |
| I | **gravity** | 0.25 px/f² (G += I each air frame) | RV |
| J | ? | 0.84 | — |
| K | ? | 0 in run/curve traces | — |
| L | **on-ground flag** | 1 ground, 0 air | RV |
| M | state/action id? | 11 while pushing into a wall | INF |
| N | **raw surface angle** (degrees, CCW, 0/360 flat; 315 = 45° downhill to the right) | — | RV |
| O | **smoothed angle** used for physics (eases toward N) | — | RV |
| Q | facing | 1 right / −1 left | RV |
| W | ? | 0.75 normally, 5.5288 after hitting the wall | — |
| X, Y | **precise position** (sub-pixel); reference point ≈14 px above the feet | — | RV |

## Ground movement (from `run_right` trace)
- From rest, holding a direction: **+9·E (0.66375) per frame while |B| < 2**, then **+E (0.07375)** up to **C = 5**.
- At or above C while holding the direction: speed is kept (no friction). Downhill can push it above C (7.94 seen).
- Slope force only when the smoothed angle O is steeper than ~30°: **B −= F·sin(O)**, both downhill (speeds up) and uphill (slows).
- Movement per frame ≈ B along the surface (x advances ≈ B·cos(O)).
- Quarter-pipe: rides up to ~82° then leaves the lip. Launch values observed: B (air x) = 2.0, G = −5.97.
- In the air holding a direction: B += 2·E (0.1475)/frame; G += I (0.25)/frame.
- Landing: the original runs the ground-speed update on the landing frame itself (down_kick: lands at B 5 → 4.631 the same frame).
- Crouch stop: holding Down (no direction) on the ground, speed is zeroed on the frame it would drop under 2 (down_kick f119, M3).

## Jump (RV, `jump_hold`, `jump_tap`, `run_jump`)
- Jump frame: Lilac first moves along the ground with her current B, then velocity = B along the surface + 7 perpendicular to the **raw** angle N. No gravity that frame. (run_jump: N 355.914, B 5.013 → vx 5.499, vy −6.625. The smoothed O would give 5.396 / −6.71.)
- Each air frame: move, then air accel (2E), gravity (I = 0.25), then drag; releasing jump clamps rising speed to −3 after gravity.
- Air drag: **vx ×= 0.96875 while −4 < vy < 0**, tested after gravity (attack_up: vx 2 → 1.9375 on the frame vy goes −4 → −3.75).
- Apex from flat ground 101.5 px; airtime 57 frames; tap jump 46 px / 35 frames.

## Moves (RV unless noted). Keys: z = jump, x = attack, c = special
| Move | Input | State M / anim | Behaviour |
|---|---|---|---|
| Look up | Up, stopped | M2 / 11 | no movement |
| Crouch | Down, stopped (or landing slow with Down held) | M3 / 10 | no movement |
| Ground attack | x on ground | M20 / 26 | 22 frames; physics unchanged (no input) |
| Air attack | x in air | M20 / 25 | 22 frames; physics unchanged |
| Uppercut | Up + x on ground | M21 / 28 | stands 10 frames, then vx = 2·facing, vy = −7; state ends ~39 frames after start; jump cut does not apply |
| High uppercut | Up + x + z together | M21 / 28 | jumps during the 10-frame windup, then vy is reset to −7 (rises ~160 px total) |
| Crouch kick | Down + x on ground, stopped | M? / 30 | 18 frames |
| Down kick | Down + x in air | M19 / 7, then M0 / 19 | windup 15 frames at gravity 0.125 (jump ignored), then vy += 5, vx = 5·facing, normal gravity until landing. From the kick phase a Dragon Cyclone is allowed if not yet used this airtime |
| Dragon Cyclone (air) | z again in the air | M26 / 18 | vy = −3 (0 if Down held), gravity 0.1 (0.25 with Down); lasts **79 frames**, landing does not end it; once per airtime; can be cancelled into a down kick |
| Dragon Cyclone (ground) | Down + x while moving (gsp ≠ 0) | M26 / 18 | 79 frames with normal ground physics and steering; jump during it → vy −4.9 |
| Action entry | — | — | on the frame an action starts, the old state's physics run first, then the state switches ("move-then-switch") |
| Dragon Boost | hold c with a FULL meter | M5 / 7, then M5 / 13 | a tap does nothing; needs energy = 100. Holding: 30-frame charge (speed frozen; in the air one move frame, then hover), then auto-fires for 33 frames: straight 12, diagonals (±8, ±8); Up/Down alone give the facing-side diagonal; a down-diagonal boost that lands continues on the ground at 10. Slope force still applies. Sprite angle = direction of travel; left-facing boosts use the dir-16 frames rotated 180° |

## Animation selection (playerskinsash_341, RV indices)
Active animation id (byte at +0xb1; the old image-table labels said 17 for skid, 22 for fastrun and 16 for hurt, which were wrong): idle 0 · walk 1 (0 < |B| < 5) · run 2 (5 ≤ |B| < 9.5) · fastrun 23 (≥ 9.5) · jump 24 · skid 16 · lookup 11 · crouch 10 · attack 26 · airatk 25 · uppercut 28 · charge / down-kick windup 7 · down kick 19 · cyclone 18 · boost 13 · hurt 8 · defeat 38 · crouch kick 30 · spring 14 · hang 15. RE: skin animation by OTHERS.Y, events 2426-2470. The hair overlay (PlayerSkinSashHair_424) always has the skin's animation id, with its own frames and speeds, drawn behind at the same position and angle. Walk/run playback speeds are fitted (≈30 and ≈35+|B|), not read from code.

## Move overlay effects (RV `build/re/fx` probes: TRACE_ADDOBJ / sparkle positions; RE events 2043, 2834, 2844, 2845)
- Cyclone (M26): RingSparkle_142 every 6 frames from the first M26 frame at (int X, int Y) + rand 0..40 − 20 per axis; an invisible ShieldInstaShield_407 is created with it. Energy −0.25 per frame (state at frame start).
- Charge and boost (M5): RingSparkle every other frame at ±8. PlayerOthers D = 1, E = 29 during M5 (and for 29 frames after).
- Invincibility (Others D = 1; boost or monitor): Sparkle1_316…Sparkle4_400 orbit an anchor (int X, int Y − 4): 1/2 at radius 20, D +7°/frame, 180° apart; 3/4 at radius 10, +10°/frame. Position = anchor + r·(sin D, −cos D). Anchors follow Lilac every 3 frames (1/2) and 6 frames (3/4). Every 6 frames a Sparkle5_284 (anim 4, plays once) is left at each orbiting sparkle.

## Comparison harness
`tools/orig/run_all.sh` traces every `tests/orig/*.txt` in the original; `tools/orig/cmp_all.sh` runs the same inputs in the fpdc host build and checks key outcomes (apex, top speed, time to top, airtime, distance, stop timing). The **16 baseline sequences pass** (host-test vs original-trace); 46 of 71 sequences pass overall (see BUILD_AND_TEST_LOG for the failing ones and their first failing frames).

## Frame index table (RV: title card after `set_frame(N)` in the running original, `tools/orig/goto_frame.sh`)
`N` = "Setting frame: N" = `Frames::on_frame_(N+1)_init` = `build/frames/frame_(N+1).json`. Stage numbers on the cards depend on the character route, so only names are recorded. Screenshots: `build/orig/sel_N.png`.
| N | Content | N | Content |
|---|---|---|---|
| 13 | bonus room (dice) | 44–49 | Thermal Base (6 frames) |
| 14, 15 | Lilac's treehouse (hub, HUD) | 50–55 | Pangu Lagoon (6) |
| 16–19 | Aqua Tunnel (4) | 56–58 | Trap Hideout (3) |
| **20–22** | **Dragon Valley acts (card "Dragon Valley")** | 59–64 | Battle Glacier (6; 64 = boss arena) |
| 23 | Dragon Valley, no card (continuation/boss section, tree start) | 65–72 | Final Dreadnought rounds 1–4 (2 frames each) |
| 24–28 | Relic Maze (5) | 73 | "Hit the targets" mini-game |
| 29–34 | Fortune Night (6; 34 = boss) | 74 | Continue? screen |
| 35–38 | Sky Battalion (4) | 75–82 | story/scene rooms (dialogue) |
| 39–43 | Jade Creek (5) | 0–12, 83–86 | intro, menus, file select, map, credits (see `build/frame_summary.txt`) |
Note: after a few jumps between frames the original swapped the playable character (Milla/Carol appear on later cards): the character is a global, not part of the frame.

## Energy meter, petals, lives, crystals (RV; GUIEnergy_127 alt A, counters at +0xa0 / max +0xac)
- Meter 0..100. **Starts empty at every stage/act start** and refills **0.4/frame** (250 frames). Boost needs 100; drains **3/frame** while boosting (paused during the 4-frame enemy hitstop).
- Crystal +5 energy (enemy-drop crystal +10; the meter can briefly read 110). Petals heal one half-petal, add no energy, do nothing at full health.
- **No 8-second boost cooldown exists, and petals do not shorten one.** The boost is gated only by the meter (prompt §2C discrepancy).
- Petal counter max 14 half-petals (7 petals). Crystal counter counts DOWN from 200 (200 = a life). Lives start at 5.

## Damage and invulnerability (RV)
- Hurt (RE, event 2414): **B = 0.75·B − 2.2·Q, G = 0.75·G − 4.2** (Q = facing, so the knock-back is away from the facing), gravity 0.5, state M8 for 22 frames (skin alt O counts 1..22). Damage = mult × source / 5 half-petals (Turretus pellet 10 → 2 on Normal, 4 on Hard; Casual none). Sound 25 (+17 logged, + voice).
- Post-hit invulnerability (RE + RV): **60 frames**, PlayerOthers_126.E set to 60 and counted down 1 per frame; collision handlers ignore Lilac while E > 0.

## Springs (RV, DV1 = on_frame_21; traces `tests/orig/spring_*.txt`)
| Spring | Effect |
|---|---|
| yellowspringup (32×16) | fires on the frame the feet reach its top edge; feet put at top + 0.25; vy = −10.75 (vx kept); state M40, skin anim 14 while rising, then the fall anim; normal gravity; lands back on it → same bounce |
| yellowspringupleft/right (32×32) | same top-edge rule (feet exactly on the top); (∓9 / ±9, −8.75); normal air drag below |vy| 4 |
| yellowspringright/left (16×32, on walls) | ground speed ±10, and that frame's friction already applied (9.63125); **input ignored for 4 more frames**; player half-width against it 16.8–19.4 (not the ±7 body box) |
Spring sound id 5 (played on contact).

## Skid / turning rule (RV, `skid`, `spring_yellow_right`)
Input against the motion: if it turns Lilac round, that frame is friction (5E / 9E) + acceleration toward the input, and the skid state starts next frame (−0.5/frame, anim 16, until stopped). If she already faces the input (e.g. thrown backwards by a spring), there is no skid, just friction + acceleration every frame (6E above speed 2, ×9 rules below).

## Landing (RE/RV)
Ground is detected by PlayerSensorBottomBig_334 (11×11 at floor Yref + 13): surface rows up to **floor(feet) + 4** count, then the slope loops move Lilac onto the surface. Landing speed (event 1591): **B = B − G·0.84·sin(N)** for 22° < N < 338°, with N from the landing-angle routine (not ported yet; ours measures the angle differently). A boost that lands keeps its speed. Falling speed tops out at **15.25** (event 1577 clamps G to 15 before gravity).

## Ceiling (RV, `spring_yellow_upleft` f52)
On the frame the head reaches a ceiling, vy becomes 0 at once (no gravity that frame; air drag still applies).

## Sound IDs (Media::play_id 0xa4e650 / play(string) 0xa4e2c0, logged by the tracer to `<trace>.snd`)
8 jump · 9 boost charge · 53 boost fire · 21 attack swing (7 frames in) · 288 + 174 cyclone · 405 kick windup · 176 + 396 kick launch · 25 + one of 80/81/82 Lilac hurt (17 also on projectile hits) · 307 + 12 monitor break · 13 rock break · 28 Turretus shot · 5 spring · 27 crystal pickup · 4 pickup (ambiguous) · 100 Press Start · 64 menu move · 63 confirm · 56 begin.

## Music (RV: `loop_play_music_<n>_<frame>` / `on_frame_N` handlers)
m_title (title frames) · m_menu2 (hub: title menu, files, options; RV) · m_menu1 (Time Attack character select + stage select) · m_results (Classic/Adventure character select; RV) · m_stage1 (DV frame index 20) · m_stage1b (21–22) · m_boss (23 = DV4; RE: `Frames::loop_play_music_4_23` string) · m_stage2 (24, Relic Maze 1) · m_invincibility (invincibility monitor) · m_1up (1-up monitor) · m_clear → m_results (results, Time Attack trace: m_clear at act end, m_results 150 frames later). BGM loops whole-file: ChowdrenAudio has no loop points (SD). `m_stage2a.ogg` is referenced but absent in this build.

## Front end (RV positions in the original frames; 426×240 view)
- Frame indices (RE, member maps): 2 Press Start (camera 107,0) · 3 hub 1440×480: title menu camera (43,0), file select (43,240), options (523,0) · 5 Time Attack stage select · 6 character select · 7 dispatcher · 8 stage card · 10 gallery · 11 finished-file stage select · 12 mahjong. (Session 5 listed the stage select as 11: corrected, 11 is the finished-file variant.)
- Title cursor x 138, rows y 131/151/171/191/211 (START GAME, TIME ATTACK, GALLERY, OPTIONS, QUIT).
- File select: cursor x 170 at y 160/184/216; slots centred at 213, 120 px pitch, wrapping; hint box fileback frame 0 at (93,16), text at (101,24)/(101,44), wrapped at 26 chars. Strings: "Press START to begin a new game.", "Press LEFT or RIGHT to adjust the DIFFICULTY.", "Select RETURN to pick another file slot.".
- Options: cursor x 90 at y 72/128/160/184, OKAY x 170 y 216; values right-aligned (last digit x 337, y 120 music / 176 sensitivity, 14 px pitch). Difficulty dirs 0 HARD, 1 NORMAL, 2 EASY, 3 CASUAL. Defaults Normal, music 100 %, sensitivity 70 %.
- Footer " A: Select ... B: Return". Font Data/font.bmp, 8×20 cells. Transition: 32 px tiles shrinking over 6 frames (stagger INFERRED).
- Stage select: stagename_138 / stageicon1_139 frames 0 Aqua Tunnel, 1 Dragon Valley, 2 Relic Maze, 3 Fortune Night.
- Developer level select (port-only): **only** L+R within 2 s on the title screen (Press Start or title menu).

## HUD (RV template match on docs/reference/hud_dv1.png, 426×240)
Meter frame 3784 at (8,8); fill 7439 at (16,10) scaled by energy/100 and drawn on top; red end cap 12760 at low health; shine loop (guienergy anim 1) when full. Petals guirings frame = hp at (8,19). Lives icon 311 at (128,8), digits x 144/154. Crystal digits x 176/186/196. Clock 4564 at (322,0); timer digits 343/353, 369/379, 395/405 at y 8 (mm:ss:cc).

## Stage card, pause, results
- Card: STAGE text final y 159, name y 185, centred x 213 (10 fps capture; rise/fade timings approximate).
- Pause (RE, events 4878-4881/4296-4305): GUIPause at (214,101), art frame 3 = Resume/Restart/Quit, 4 = Restart greyed (lives ≤ 1); Crystal cursor at (166, 141 + 24·row); 50-frame wipe for Restart/Quit. Sounds 63 open, 64 move, 56 confirm.
- Act transition DV1 → DV2: fade + card, no results, timer continues.
- Results assets: 8005 "Stage Complete!" (320×48; build-in from levelcardactclear1 anim 7), 955 "Results" banner, 10860 panel (180×48), 10837 rows (148×40: crystal icon, TIME, colons), totalbonus digits 2794…, small digits 1922…. **Layout INFERRED**: the original's post-boss results were not observed (killing the Serpentine via memory failed, time-boxed).

## Recovered from the executable (session 4; details and event numbers in RE_NOTES.md)
- **Camera:** MasterCamera_408: ±16 px horizontal dead zone (the view snaps after a teleport), grounded landing ease 4 px/frame, air dead zone −48…+16, look-ahead up to 64 px at speed ≥ 5 (2.5/frame while boosting), look up/down 96 px after 120 frames, display clamp to the frame. Replaces the reconstructed 0.15 easing.
- **Turretus:** idle, aims in direction 15/1 (11.25° up), timer E counts while within 200 px of the view, shoots at E > 60 then E = −40; pellet released at shoot-animation frame 7 from the action point (±18, −14). **EnemyPellet:** PinballMovement 5 px/frame, gravity 0.25 px/frame², floor stepping (exact).
- **Animation timing:** a frame advances when the counter goes above 100 (not at 100); changing animation keeps the counter.
- **Angle smoothing:** k = max(0.175, |B|/15) within 45°, snap beyond. **Slope force:** N in 35…325°, ×0.38 below speed 5, none when stopped.
- **Precision:** the original floors B, G, N to 4 decimals every frame in doubles; the port now uses doubles for Lilac's physics.
- **Handlebar:** grabbed by the top-big sensor while falling; Lilac hangs at (sensor x, sensor y + 32).
- **Results screen:** located (DV4 events 4560–4589, `loop_tally_19`), not ported; the port's layout is still INFERRED.

## Monitors and items (RE events 2543, 1288-1291, 2527-2531, 2632-2670)
- Crystal: reach X ±19, Y −39..+19 of (int X, int Y); not while hurt (M8/M9); energy +5 (max 100); crystal counter −1; sound 27; RingSparkle.
- Petal: reach Y −31..+19; sound 37; flies to the HUD counter; +1 half-petal (+2 on Casual), sound 4, two hearts. No pickup at full health (RV).
- MonitorCrystals throws 7 crystals (loop_spawn_crystals 1323), MonitorRings throws petals, Monitor1up gives a life, MonitorInvincibility sets Others D = 1, E = 1800 and plays m_invincibility. Throw arcs and the 1-up jingle length are INFERRED.

## Loading screen and audio (port design, host-test + emulator)
Every level, front-end and boot load runs behind a black loading screen with ObjectCrystal_317's spin bottom-right. The music stream and every SFX channel are stopped for the whole load, and the next track starts after it. Root cause of the doubled or stuttering music on the Dreamcast: the AICA loops the stream ring buffer in hardware (`chan->loop = 1`), and blocking GD-ROM loads starve the refill thread, so stale halves replay. Also fixed: the stream volume was set before `snd_stream_start`, which forces volume 255.

## Front-end screens in the port (session 6)
| screen | original frame | port | sounds | from the original | INFERRED / not ported |
|---|---|---|---|---|---|
| Press Start | 2 | frontend.c FE_PRESS | 100 | events 307-330, 1-frame input latency | – |
| Title menu | 3 (cam 43,0) | fe_hub.c title_events | 63/64 | 380-392, console layout | QUIT hidden (console) |
| Gallery | 10 | fe_gallery.inc FE_GAL | 64/63/32/40/443/56/100 | spec_gallery (992-1139), RV t_gal1-6 | see "Session 15: Gallery" |
| File / mode select | 3 (cam 43,240) | file_events, delete_events | 63/64/56 | 393-437 | Adventure skips the cutscene; continue starts DV1 |
| Options | 3 (cam 523,0) | options_events | 63/64 | 464-511 | Controls / screen size / sensitivity hidden (console); SFX volume display-only on host |
| Character select | 6 | frontend.c FE_CHSEL | 63, 220/221/186, 56 | spec_charselect | Lilac, Carol (session 10) and Milla (session 11) playable |
| TA stage select | 5 | fe_stsel.inc | 63/64/56 | spec_stageselect | only Dragon Valley startable; BG parallax drift not ported; confirm 3 frames late |
| Pause | 20 (DV1) | game.c pause_* | 63/64/56 | 4878-4881, 4296-4305, 3551 | QUIT from Classic → hub |

## Session 7: ladders, platforms, Ganseki, push (port: src/characters/player.c, src/game/objects.c)
- States: ST_PUSH 12, ST_LADDER 13 (M46), ST_LADDERIN 14 (M25). Anims: push, climb, climbdown, climbover, catch.
- Object types: OT_LADDERTOP 30, OT_GANSEKI 31, OT_PLAT_VALLEY 32, OT_PLAT_MOVE2 33, OT_PLAT_MOVE3 34, OT_BRIDGE 35, OT_LOG 36.
- The camera follows Lilac's Y on the ladder; cam.M4 holds during the enter-from-top.
- Hit-stop: simplified (whole player update skipped while it counts down).
- Mirrored dir-16 animations are no longer stored; `set_anim` draws the right-facing anim flipped (draw_flip / hair_flip).
- Loading: packs per screen in RAM, released at loading_end; loose files only loading.* and vmu_*.

## Session 8: bosses, chase, results, parallax, keystones, roll/tunnels (port: src/game/bosses.c, serpentine.c, chase.c, results.c, parallax.c, gimmicks.c)
- States: ST_CORK 15 (M45), ST_ROLL 16 (M6). Roll animation = skin anim 7 (2438), speed int(|B|·5 + 25).
- Object types added: OT_CHUNK 138, OT_WF_* 139-143, OT_BBOULDER 144, OT_KEYSTONE 146, OT_SPIRAL_FRONT 147,
  OT_FRONTROCK/FRONTTREE/TREEFRONT 148-150; spinning logs (115/116) and PushableBlock1 (98) now active.
- Original traces recorded this session (scripts in tests/orig_extra_*.txt, CSVs in build/orig_traces/extra_*.csv):
  tredd 1-3, snake_t1, dv4_chase, dv4_results, dv4_boss, spiral_dv1 (top ride), spiralb_dv1 (bottom ride + reversal),
  balltunnel_dv3, pblock_dv2. Host TP lands 2-4 frames before the original's; tests align by that offset.
- Matches: Tredd 1100/1100 frames; DV2 snake 271/271; results timings 72/292/441; spiral bottom ride, DV3 ball tunnel and
  the pushable block exact (X, B per frame); spiral top ride exact up to input alignment.
- Camera bounds: boss mode (U = 1, 2862-2868 ease 2 px/frame), chase mode 3 (minimum C), cinematic (5590).
- Object layer drawing: objects_draw(pass) 0 = layers ≤ 1, 1 = layer 2, 2 = layers ≥ 3; order pieces 0-1, objects 0,
  pieces 2, objects 1, Lilac, spiral front, pieces 3, objects 2, pieces 4-7.
- Teleport caveat (tracer): sensors are clamped to the camera bounds, so a TP far from the start region can push Lilac out
  of the "ground" every frame (DV2 logs at x 10240 could not be reached that way).
- Emulator (Flycast, developer select, 16:9): DV1-DV4 load and draw; pvr free after load 2046 / 1211 / 1326 / 1144 KB.

## Session 9: DC perf log and frame budget (port: src/platform/plat_dc.c, fp.h PROF)
Every 600 frames the DC build prints `perf:` and `prof(avg/max us):` with per-section average/maximum microseconds
(par, plat = objects_platforms, player, layers, obj = objects_update, gimmick = shields/water/pblocks/spinlogs/tunnel/
cork, cam, res, draw = game_draw incl. wait, submit = list submission, wait = pvr_wait_ready, mus = music refills on
the stream thread), `frame_avg` and `vblanks N per 600 frames` (pvr_get_vbl_count: 600 = no frame missed), then up to 8
missed-vblank frames with their own update/draw split. Times are timer_us_gettime64 (emulated time in Flycast).
Demo discs print `demo f` every 600 frames (console output is slow; the report frame is excluded from the pacing).

## Session 10: Carol (port: src/characters/carol.inc, carol_bike.inc, player.c; src/game/combat.c, game.c)
- Characters: CHR_LILAC 0, CHR_CAROL 2, CHR_CAROL_BIKE 3 (= the original's MV.A). Host `FP_CHAR=<id>`, demo discs
  `CHAR <id>`; character select sets G.character; a file slot saved on the bike starts on foot (hub).
- States: ST_CLAW 17 (M20), ST_WILDKICK 18 (M44), ST_WALL 19 (M47), ST_PAD 20 (M48), ST_BIKEAIR 21 (M32); the pounce
  is ST_NORMAL with cY = 24, the roll is ST_ROLL (M6). `player_orig_m()` maps back to the original M for traces.
- Player fields: cY (OTHERS.Y), cL/cA (PSC.L / PSC.A: kick counter, pad count, combo index), bikeB (BSK.B), bikeC
  (bike hit points 3), c_pavO (PAV_B.O), pad* (pad ride), item_ovr (pre-move pick-up position).
- Objects: OT_FUEL 158 (jerry can), OT_PBIKE 159 (parked bike); pads are created by pads_spawn() from per-level
  tables (PADS_DV2/DV3/DV4) in objects_spawn.
- Banks: carol2 (skin, palette pal2c.pal replaces bank 2), caroltail (overlay), carolbike (BSK + pbike + effects);
  carol2 and carolbike have second atlases (`_2.tex`).
- Event order per frame (Carol): energy → movement (bike_phys after water) → state switch (claw/kick/wall/bikeair/pad)
  → fuel_events → pads_events → Carol group (timers, bike_events, 2432, roll ball, Y9/Y13) → animation (cY → anim,
  tail, PSC) → engine update.
- Sounds: loaded per character at level load (`snd_group_load`); bike engine through `snd_loop_start/set/end`
  (DC: snd_sfx_play_ex looping + AICA_CH_CMD_UPDATE for volume/frequency).
- Traces: tests/orig_carol/*.txt (c_ on foot DV1, b_ on the bike DV1, c_jumppad_dv2 DV2), CSVs build/orig_traces/carol,
  compared by tools/orig/cmp_carol.py (V=1 prints the first mismatching frame per field). Host CSV columns added:
  M, cY, cA, cL, cS, aname, inv, hafr, sacc, hacc, rev; host f-line adds chr, bike, bc, oy.

## Session 11: Milla (port: src/characters/milla.inc, player.c; combat.c 2742-2745; objects.c rocks / monitors)
- Character id CHR_MILLA 4 (host `FP_CHAR=4`, demo `CHAR 4`); select cursor 3, voice 186. Classic / Adventure start at
  DV1 in the port (the original sends Milla to Aqua Tunnel 1, frame 16, which is not ported: D-040).
- States: ST_MATK 22 (M20), ST_FLOAT 23 (M22), ST_DIG 24 (M27), ST_MCARRY 25 (M30), ST_MSHIELD 26 (M31);
  `player_orig_m()` maps them back. OTHERS.Y is `cY` (shared with Carol).
- Player fields: mA/mB/mC/mD (dig power, progress, reward roll, zone), mE (float meter), mF (holding: 2 cube), mG (cube
  charge), mH (attack cool-down), mN (float counter), mfl1/mfl10, nol (MV.V = 0), mC4 (MV.C = 4), skinI.
- The cube: a static `mcube_t` in milla.inc (not in G.o): drawn after the skin, ticked and collided at the frame start
  (update_objects + test_collisions_20), its group 0x21a67 after the state groups. Hooks: `player_cube_hits /
  _breaks_rock / _opens_monitor / _damage` for combat.c and objects.c.
- Frame order: cube tick + collisions -> movement (milla_normal / milla_move) -> 1665 -> clamp -> 1723-1725 (grounded
  M0, after the clamp) -> 0x21a67 -> M20 / M22 / M27 / M30 / M31 groups (each only while M is still its state) -> 2414
  -> animation (index from OTHERS.Y, direction 0/16 - F, nearest existing direction, tail with its own counter).
- Banks: milla2 (skin + dig mound + HUD panel `hudm`, ACT1 action points, palette pal2m.pal), millatail (4 bpp),
  millafx (PlayerReflect_463, 4 bpp) in G.lilac / G.hair / G.bike.
- Host: f-line adds rA rx ry me mh mm; CSV adds mE, mF, mG, rA, rx, ry, rC, rF, ranim, mdir. Script commands
  `<f> DIGZONE v` (MILLA.D) and `<f> SETV others E v` (invulnerability, same line as the tracer's).
- Traces: tests/orig_milla/*.txt (DV1; m_dig_at1 Aqua Tunnel, FRAME=16), CSVs build/orig_traces/milla, compared by
  tools/orig/cmp_milla.py (FR=295 for the 300-frame traces). Teleports far from the start can fail in the original
  (camera bounds): use spots near the DV1 start or at x 5430 / 6740.

## Session 12: Aqua Tunnel (port: src/game/aqua.c, aqua_gim.c, aqua_boss.c; tools/import_frame.py; tests/test_aqua.py)
- Levels at1..at4 = original frame index 16..19 (`tools/build_level.sh 17 at1 converted2` … `20 at4`). AT layers map
  2→1, 3→2, 4→3 (D-045). Per-level banks at<n>_ens hold every AT class (AT_ENS in import_frame.py; at1/at4 split into
  two atlases).
- Routes: Milla's Classic / Continue load at1; Time Attack icon 6 (AT) replaces DV for her (fe_stsel.inc
  `stage_playable`, RE 623-625); records per stage index (`stage_rec_index`: AT 0, DV 1).
- Object types: 160-182 (dig spot, enemies, jets, jelly, springs, bomb / crate), 183-213 (gimmicks B: ABPLAT, COLLAPSE,
  SDOOR / SDOOR2 / APLATE / WPLATE_AT (AT retypes of SwitchDoor / PressurePlate / WallPlate), MBLOCK, CRUSH_V /
  CRUSH_VIS / CRUSH_H / CRUSH_H2, PIPE / PIPEHUB / PIPECOG, BEAM, AXEL1/2, AXMAKER1/2, SWSPAWN1/2 → SWBASE / SWCHAIN /
  SWPLAT, SCROLLBRICK, SLIMEFLOOR / SLIMEF_VIS, RAFT, TREEOVL, FISHSTATUE), 214-227 (bosses: IW_HEAD → IW_HEADF at
  emergence (enemy), IW_SEG, IW_BASE, CH_HEAD / TAIL / BALL (enemy) / MOUTH, SN_HEAD / SEG / MOUTH, ACID, ROCK_AT,
  BUBBLE).
- Frame order in an AT act: objects_platforms (no-op in AT) → aqua_pre (dig zones) → player_update → objects_update
  (combat, shared enemy events) → aqua_update: bursts, bombs, jelly, bricks, **aqua_gim_post** (path 3283, carry 4061,
  swings, collapse, plates / doors, Milla block, wall plates + pipes, beam, crushers, axels, slime / rafts, dip),
  **aqua_boss_update** (worm / Chomper / snake, rocks, acid), fireflies, slimes, burst timers, whisps, jets,
  checkpoints, boss camera, jet maker, dig.
- Rotated object masks (o->ang): obj_px / rows_hit_rot in objects.c serve the sensors, obj_rect_hit and obj_obj_hit.
- Player: ST_SHAKE (M12), ST_HELD (M43, the Chomper's mouth), `tint` (OTHERS.R, green 0x80ff80 while > 0).
- Results: `results_begin(2)` = AT4 (timer frozen at the kill, Milla stands, raft exit 4588-4592, wipe at x 7024).
- Sounds: stage group SND_AT in game.c (D-047).
- Host: `FP_TRACK=type:spawnx` (spawnx -1 = any; 214 also matches 227), `FP_AT_GT`, `FP_AT_PRERUN`; script commands
  `<f> BOSSHP v` (AT boss HP). cmp_at.py writes the raw host output to build/fpdc_traces/at_<name>.out; `XTRACK=` adds
  port objects to it.

## Session 13: level table, reusable objects, switches, registry, save v3 (port: src/game/levels.c, switches.c, objdefs.c, save.c)
- **src/game/levels.c** — `level_def_t` per act (fp.h): dir, original frame index, stage index s (0 AT, 1 DV, ...; the
  save / Time Attack index), act / nacts, music (+ after checkpoint 10), water line, walk-in, exit rule (exit_max_y),
  edge results, AT axel / pipe values, pads, dig zones, plate rule / switch pass, monitor hop, stage sound group,
  checkpoints, boss camera lock, act-start item sets, camera bounds, start rule (checkpoints), start fall, card ids.
  API: `level_def(dir)`, `level_cur()`, `level_next(dir)` (NULL after the last act), `level_first_of_stage(s)`,
  `level_route_start(character)`; stage flow shared by every level: `level_flow_spawn()` (item sets),
  `level_flow_frame(p)` (checkpoints, boss camera), `level_camera_bounds()`, `level_release_camera(keep)`,
  `level_cam_locked()`. Readers: game.c (music, act end / next act, walk-in, start rule, stage sounds), water.c,
  results.c (edge results), gimmicks.c (spinning-log line), carol_bike.inc (pads), aqua.c (dig zones, AT stage
  script = stage 0), aqua_gim.c (axels, pipes, swings' water line), monitors.c, switches.c, frontend / fe_stsel (routes).
- **src/game/objdefs.c** — `objdef(type)`: registry flags OD_SOLID / OD_OBSTACLE / OD_NOEDGE (qualifier groups,
  obj_in_group) and OD_BOXCOL (image-box collision: sensors in objects.c, aqua_gim is_box), and the spawn sprite (the
  old EA table). One row per engine type, ranges as base + k. OBJECTS.md is generated from it.
- **src/game/switches.c** — pressure plates (OT_PPLATE, rule from level data), the Milla block, SwitchDoor 997/1066
  (OT_DOOR), 770 (OT_SDOOR), the AT1 gate (OT_SDOOR2); `switches_update(p, late)` runs in the platform block (DV,
  late 0) or in aqua_gim_post (AT, late 1).
- **Presence:** `objects_have(type)` / `objects_have_from(type)` (objects.c): on-map placed types of the level, set at
  spawn. The Aqua Tunnel passes run in any level with objects from OT_DIGSPOT up; tunnels (q27) and spinning logs follow
  their objects.
- **Engine types added:** 228 OT_ABPLAT2 (AquaBrickPlatform_803), 229 / 230 OT_AXEL1P / OT_AXEL2P (845 / 846), 231
  OT_SWSPAWN1B (848); importer types 210 (SwitchDoor_770), 212 (SwitchWallPlate_831) and 188 (LevelCrusher1_847)
  directly. 211 (OT_APLATE) is unused.
- **Frame order in an act (every level):** objects_platforms (DV platform passes, switches with switch_late 0) →
  aqua_pre (AT2 intro, dig zones) → player_update → objects_update → aqua_update (skipped without AT-family objects,
  then only level_flow_frame): bursts, bombs, jelly, bricks, aqua_gim_post (path, carry, swings, collapse, switches
  late 1, pipes, beam, crushers, axels, rafts, dip), the AT bosses (stage 0 only), fireflies, slimes, timers, whisps,
  jets, level_flow_frame, jet maker, dig.
- **Save v3** (save.c, fp.h): options; file[8] {used, character, mode, last_stage, lives, shield, counter, stage[15]
  {time ms, crystals, deaths}}; rec[15][4] {time ms, crystals}; progress[4] (s + 1); cards (ids 1..100). Totals =
  sums. Writes: `game_save_stage_end(time_ms)` at the results' D = 150 (and a last act without results) →
  `save_stage_end()`; cards at each act change (`cards_commit`). New game: `G.file_new` replaces the slot at the first
  stage end. Deaths per stage: `G.stage_deaths` (+1 per death restart).
- **UI (ui.c / ui.h):** kind 2 = solid quick backdrop (w, h, colours); kind 3 = translucent layer of the object before
  it (alpha in the header pad byte; it copies the owner's position, frame, visibility, tiling); `tilew` (preview strips
  repeat across the window), `tileh` (the checker layer wraps vertically).
- **Renderer (render_pvr.c):** translucent-list headers use PVR_TXRENV_MODULATEALPHA (D-060). objects.c `obj_obj_hit`
  computes each rotated object's cos / sin once per test (the DV4 boulder vs the tunnel walls).
- **Host:** `<f> LOAD <dir>` (a fresh start of another level in the same run), `FP_FRAME_US=n` (frames whose update exceeds n us), `FP_CARD_DBG` (prints the card images of
  each load), `FP_DUMP_OBJDEFS` (registry per type), `FP_TA` / `FP_FILE` / `FP_FILE_NEW` (save tests).
- **Tests:** tests/test_levels.py (cards, load orders, restart, hang animations, results panels, registry vs importer,
  next acts, checkpoint), tests/test_saves.py (records, files, progress, cards, conversion, CRC).

## Session 14: Relic Maze (port: src/game/relic.c, relic_gima.c, relic_gimb.c, relic_boss.c, rm_claw.c, rm_mantalith.c; levels.c, game.c)
- **Level rows (levels.c):** rm1-rm5 = frames 24-28, stage 2, cards 11-20, music m_stage2 / m_stage2b. Row fields added
  this session: `exits` (exit_t: edge, Y band, target, walk-off B), `entries` (entry_t by source act), `death_to`,
  `flag1_y`, `crystals_once`, `wplate_cyclone`, `gates` / `flagclears`, `breakout` / `breakout_flag`, `rooms`
  (camroom_t: camera bounds per room), `warps` (warp_t), `walkin_purge`, `rmg` (pillar zones, pads base, door return,
  basement x), `pads`, `nobike`, `plate_rule` 2 (radio). SFX_RM = the RM stage sound group (D-047 / D-063).
- **G fields:** `stage_flags` (ML 0x1 / 0x2 / 0x4), `exit_to`, `exit_b`, `walk_b`, `keep_g4`, `stage_carry`,
  `prev_dir` (the act just left, chooses the entry), `input_lock`, `warp`, `cam_nF`.
- **Frame order (game.c):** objects_platforms → aqua_pre → player_update (inside objects_sense_snapshot(1/0)) →
  layers → walk-in / exit walk → objects_update (relic_gima pre / post, golems, rocks, iris purge via relic_frame) →
  aqua_update → relic_frame → relic_gimb_frame → rm_claw_frame; Mantalith via rm_mantalith_frame / _draw hooks. Exits
  and warps are checked after the update; at clear_t == 255 a warp or next_act runs.
- **Type ranges:** 232-245 enemies, 246-292 acts 1-2, 300-336 acts 3-5 (member index in lvobj anim), 350-371 golems
  and markers, 400-449 Claw, 450-499 Mantalith; MAX_OT 512 (presence bits, registry, group tables).
- **Results:** results_begin_boss (path 3, any boss: stage time stopped at the kill), results_tally_done /
  results_frames / results_script_input / results_request_end; voice_play_once (two slots, freed at the stage end).
- **Front end:** Continue starts the stage after the file's last completed stage with its lives / shield / crystal
  counter (frame 8 routing); the TA list gets Relic Maze from the level table (stage_playable) with the progress lock.
- **objects.c helpers (D-067):** objects_sense_snapshot(on) (packed sensor prefilter), objects_alive_type(t)
  (superset of alive types, rebuilt on slot changes and each objects_update).
- **Host:** `FP_TRACK=type:spawnx` (now tm / vx / vy), BOSSHP for golems, Claw hitbox and Mantalith head / arms,
  `FP_DBG_OBJS`, `FP_TRACE_SND`, `FP_DBG_BG`. Tests: tests/test_rm_gimmicks_b.py, test_rm_claw.py, test_rm_mantalith.py,
  RM cases in test_gameplay / test_aqua.

## Session 15: Gallery (port: src/game/fe_gallery.inc, save.c v4)
- **Route:** title menu GALLERY (hub wipe, sound 56) -> frame index 10 (m_results, tile fade-in 14x8 from (96,0)) -> RETURN
  (sound 56, wipe, F > 50) -> title menu (m_menu2). View fixed at camera left 107; the scroll group eases 1/8 per frame to
  -(D + 1) x 30 (cards) / x 25 (achievements); Return_110 follows the header at y - 24 (RV y 52).
- **Pages (header Left / Right, wrap):** CARDS (10x10, rows DV RM AT FN SB JC TB TH PL BG = ids 10r+1..10r+10, row icon frames
  1 2 0 3 4 5 6 8 7 9 or the padlock 15), ACHIEVEMENTS (8x7, frame k when announced, else "?"), DATA (four delete rows, YES/NO
  panel starting on NO, applied when leaving).
- **Cards:** dir 0 grey star (not taken), 1 face down (shimmer frame (t/3 + id/2) mod 40, held after 10), 2 padlock (type 3
  without achievement 1-3), 3 note / 4 speaker / 5 cookie (opened). A on face down: sound 32, flip frame B mod 11, sparkle
  spiral, at B = 50 opened (save 2), sound 40, white burst, selector back at B > 100. A on an opened card: music
  (`bgm/<file>.adp`, type 1 once, 0 / 3 loop) or a clip (`gal/<file>.wav`, its WAV length = busy) on "channel 31" (m_results
  does not come back), again = stop; art = `gal/art_NN` at (322,0) on top, A closes (443). C on an opened card: Close
  Card (443, face down, save 1). Card 68 shows the bird guard while it plays.
- **Text box (120,8):** 50 columns, 20 px lines at x 13 / y 8: label + hint, a hint alone on line 1 (RV), name +
  description, or the DATA warning. Footer "UP/DOWN: Scroll" (13,216) / "B: Return" (341,216).
- **Entry Card Collector (RE 992):** rows with all 10 cards and achievement 23+r == 0 -> 1, save; the "UNLOCKED" overlay
  (3 per page, sound 100, 150 frames or A after 30; INFERRED layout, the original's frame 9 is not ported) sets them to 2 and
  saves again.
- **Leaving:** deletes (times / crystals of every stage and character, cards + opened, achievements except 42) then one save
  write, only when something changed in the gallery (INFERRED: the original saves unconditionally).
- **Save v4** (save.c, fp.h): v3 + `cards_open[13]` (records x20 == 2) + `ach[56]` (records x2 y1..56, 0 / 1 / 2). 1361 bytes
  (6 VMU blocks with the header and the 3-frame icon). v3 and older load with taken cards face down and no achievements;
  a short, long or CRC-damaged file is ignored and kept until the next deliberate save. Card pickups only set the taken bit
  (pickup = max(old, 1)). Title cheat: progress, all 100 cards (face down) and achievements 1-3 = 2.
- **Host trace:** `gal ...` lines (entry dumps of cards / dirs / achievements / rows, card writes, plays, art load / free, marks,
  deletes, saves); tests/test_gallery.py.


## Session 15: fixes and cheat (gallery: see "Session 15: Gallery" above)
- **objdefs.c OD_HIDDEN:** placed objects (objects_spawn) and relic_gima.c runtime spawns start with o->hidden = 1
  (collision unchanged). Types: OT_COLFRONT, OT_WR_BARRIER, OT_SPIRAL_FRONT, OT_SPINLOG_MASK.
- **objects.c:** the bank-extent caches (8-entry and per-slot) are cleared when the objects reset (G.ens / G.objs keep
  their address across levels).
- **relic_gima.c gates():** create gates use a per-gate bit (`gates_created >> k`) and wait for PX > gate x + 28.
- **frontend.c:** options (difficulty, music volume, letterbox) are taken from SV only on the first fe_load of the boot.
  Cheat: cheat_step() on FE_PRESS and on the hub title menu (hub_on_title), cheat_music() returns to m_title /
  m_menu2 after 240 frames; save.c save_unlock_all() (progress = 15 for 4 slots, 100 cards, achievements 1-3 = 2).
- **game.c:** HUD life icon index 17 (hud_icon_cb, image 395) for Carol on foot and on the bike; VMU life icon 2.
- **Host:** `<f> FLAGS v` sets G.stage_flags. Tests: test_frontend cheat_* (sequence, timeout, reset, title menu, save
  bytes), test_gallery.py.

## Session 16: achievements (spec build/re/spec_achievements.md; port save.c, results.c, combat.c, fe_gallery.inc)
- **Helper (save.c):** `ach_earn(k)` = records x2 y(k) 0 -> 1 (RE "set 1" / "max 1": an announced 2 is kept); no VMU write.
  `ach_pending()` = any achievement at 1. Persisted by the existing writes: the results' D = 150 save (5394) and the pop-up.
- **Hooks (RE event, port place):**
  - 10-17 `ach_boss_finish()` (combat.c) on each frame of the boss finish moment: DV4 serpentine.c after 5531
    (`S4.K == 0 && S4.hG == 1`, hitbox defeated), AT4 aqua_boss.c after 4642 (`O == 1`), RM5 rm_mantalith.c
    defeat_events() at head `I == 2`. It reads MV.A (`G.character`, 3 on the bike), MV.M (`player_orig_m`), a live OT_SHIELD
    with direction 14 / 30 (Carol's big slash) and Milla's cube (`player_cube_state()`, `player_cube_beam()` = A 5 + anim 5).
  - 42 at the same RM5 moment (7118); 36 rm_mantalith.c intro_events() (6900: both arm hitboxes A <= 0 while the intro
    group runs); 35 relic_boss.c after the 7116 decrement (gem defeated, HEAD B > 0); 34 relic_boss.c ge_frame() 5955 block
    hit; 19 rm_claw.c 6272 (Claw hitbox A > 0 when the engine is wrecked); 33 chase.c 5453-5460 respawn branch; 38 aqua.c
    dig_events() (dug spot, AT1 and RM1); 18 switches.c OT_DOOR (level_def_t.door_skip_ach = 18 on DV2: the 5098 E counter
    and the 5099 test, SW.skipE / skip_once).
  - 23-25 / 43-45 results.c `ach_results()` at ScoreValues D = 149 (once): `cards_commit()` first, then per stage index
    (DV 1: cards 1-10, 23, 43, 300000 ms; RM 2: 11-20, 24, 44, 480000; AT 0: 21-30, 25, 45, 300000) on
    `R.time_frames * 1000 / FPS`. Any mode, any character.
- **Pop-up (frame 8 -> 9):** results.c stage_end(): `ach_pending()` -> `fe_enter_ach_popup(ta, next)` (frontend.c): the
  game is unloaded, the front end and the gallery screen bank (scr_load(2)) are loaded, state FE_ACH (fe_trace_info index 9).
  fe_gallery.inc `gal_bind()` (scene objects + text, shared with gal_enter) and `pop_begin / pop_step / pop_draw` (shared
  with the gallery's Card Collector entry): 3 per page, sound 100, each page's entries written 2 when shown, "CONTINUE"
  waits for A / Start (presses in the first 30 frames ignored), one save_store() after the last page. Then: the route's next
  stage (`fe_unload(); game_next_stage()`), the Time Attack stage select, or the title menu, as stage_end() would have gone.
  Black background, no music. If the gallery data cannot load, the stage flow goes on without it (the gallery announces
  pending ones later). The gallery's own pop-up now waits for confirm too (the 150-frame auto-advance is gone).
- **Host trace:** `ach earn k`, `ach finish M A slash cube beam`, `ach results stage s ms t`, `gal popup enter n next x`,
  `gal toast k` / `gal toast done`. tests/test_achievements.py; test_gameplay dv4_end_results / dv4_exit and test_aqua
  snake_defeat press A on the pop-up (their runs earn a speedrunner achievement).

## Session 16: renderer, importer, VMU
- **render_pvr.c:** OFF_X 0.25, OFF_Y 60.25 (letterbox) / 0.25; letterbox bars at fixed 60 px. r_rect: alpha 0 draws
  nothing, alpha < 255 goes to the translucent list (dcmd kind 3) and keeps its alpha there.
- **tools/import_frame.py:** backdrop pieces (drawn and collision) at the instance x, y (hx = hy = 0).
- **objects.c:** obj_launch_crystal / obj_throw_petal / throw_items (and Serpentine petals) set layer 2.
- **relic_gima.c relic_gima_draw_overlay():** 8 rings × 4 rects, ring k alpha = blight_alpha × (1 − k/8), 85 × 48 px.
- **vmu.c vmuscr_char(character):** frame = ncrystal + {Lilac 0, Carol 1, Milla 2} × 4 + (tick / 360) % 4; called from
  game_update in MODE_PLAY and on the file select (fe_hub.c hub_vmu_file_char). vmu_lcd.bin: 6 crystal + 12 portraits.

## Session 17: Fortune Night flow (port: src/game/levels.c fn1-fn6, fn_flow.c, game.c, results.c; tests/test_fn_flow.py)
- Rows: stage 3, frames 29-34, music m_stage3 / m_stage3b / m_boss (slots 4 / 6 / 7 per act: level_music_slot), cards 31 /
  33 / 36 / 37 / 39 (FN6 none), checkpoints FN1 7700 / FN2 6520 / FN3 9536 / FN4 7670 / FN5 8430 (g4 2) + 10880 (g4 3),
  camera E = sign.x + 213 (12205, 15133, 12205, 15501), FN4 / FN5 exit X > W + 20, FN3 scripted exit, FN6 fixed box C 224
  D 0 E 960 F 311 (view (214, 71), RV) and walk-in X >= 400 without B, FN6 death / restart -> FN5 with g4 2.
- FN3 exit (fn_flow.c): host-measured with the RV placement (15060, 190): Serpentine at 14560, brakes to 14910 (RV 14910),
  taunts ~100 frames, passes, fade, FN4 282 frames after the trigger (RV 283); player walks to 15117 (RV). Sounds 17 + 134,
  61, 6, 61. HUD up 2 px a frame (7712), timer and pause off, Carol off the bike (7711).
- FN3 start: Lilac loses the samurai set (boss types 840-843) and the 8 MonitorBombs; Carol / Milla lose the Serpentine
  parts (821-825) and get the limit markers at 10831 / 11441 / bottom 1424.
- Hooks for the boss module: level_lock_markers / level_release_camera, level_music_slot, player_bike_off(p, park),
  results_begin_boss (FN6 row: group voice 8545, wipe 8546-8549), G.cam_nD / cam_nF, par_set_drift, fn_flow_panther_view.
- Host: row fields `g4` (act-start mode) and `tm` (stage timer); script `<f> RESBOSS` (results_begin_boss now).

## Session 17: Fortune Night bosses (port: src/game/fn_boss.c; tests/test_fn_bosses.py)
- **Types 800-859** (fp.h): UFO set 800-805, bulldozer 810-818, Serpentine 820-829, samurai 840-845; reused: OT_FN_BFLOOR /
  LAMP / COLUMN / RAMP (fn_gima.c), OT_WR_BARRIER, OT_FN_LASER / OT_FN_STURRET (fn_enemy.c), OT_FLAME (relic.c),
  OT_BOMBX (aqua_blast). Importer: FN_BOSS block (FNBOSS_TYPES_ID only in json 30-32 / 34, FNBOSS_SUB, rows fb_* / fs_*).
- **Hooks (one line each):** objects.c fn_boss_spawn / fn_boss_pre (test_collisions handlers, before combat) / per-object
  dispatch / draw (UFO rig order: lights behind the hull while sin B < 0, y-scaled beam, additive glow); game.c
  fn_boss_frame after fn_gimb_frame and fn_boss_input (CTRL.M during the escape); combat.c fn_boss_q20.
- **Level data:** level_def_t.fnb (fnboss_t): FN3 chase window / zones / cut A / Floor2 MV.B, samurai lock / music / CAM.D /
  g4 limit / wreck y / turrets (FN3, FN5). Flow interfaces used: level_lock_markers, level_release_camera,
  level_music_slot(4 / 6), music_fade, CK_FN3 / CM_FN3 (class sets and markers by character).
- **Sounds:** SFX_FN += 62 70 75 165 166 168 169 191-200 317 (11025 Hz; convert_sfx.sh).
- **Host:** BOSSHP sets the bulldozer, the Serpentine hitbox and the samurai (not the UFO); the `ob` line adds a5-a7, c, hd,
  lay, ang; FP_DBG_FNB prints the boss camera bounds per frame.
## Session 17: the Panther (port: src/game/fn_panther.c; tests/test_fn_panther.py)
- **Types 860-899** (fp.h OT_FNP_*): values, 13 rig parts (head .. foot FR) + ribs / tail, hitbox (Q20 via
  fn_panther_q20), laser, target, glow, wheel, scrolling floor (OD_SOLID|OD_BOXCOL), speed back, plane platform
  (OD_OBSTACLE), the assist set, ships / cables (FN5), runtime plasma / missile / rapid bullet / warning / speed line /
  shield / insta-shield. Importer: FN_PANTHER block (FNP_TYPES_ID, rows fp_*, FN6-only rows tied to 'scrollingfloor').
- **Hooks (one line each):** objects.c fn_panther_spawn / fn_panther_draw(pass); game.c fn_panther_input (the floor's
  CTRL.D auto-run, the FN5 pose lock) in the scripted-input chain, fn_panther_frame after fn_flow_frame,
  fn_panther_draw(3) (the white fade) after the front passes; combat.c fn_panther_q20; plat_host.c BOSSHP += OT_FNP_HIT.
- **Renderer:** r_set_xscale / r_get_xscale (an x scale for rotated sprites; render_soft.c and render_pvr.c, kept in the
  PVR translucent queue) for the laser's set_x_scale(16).
- **Flow interfaces used:** fn_flow_panther_view (8370), level_lock_markers / level_release_camera, music_fade,
  level_music_slot(4), player_bike_off, par_set_drift (8762), results_begin_boss(kill time) (8785), G.timer_stop,
  G.cam_nF, G.input_lock.
- **Per frame (SH4):** the rig is computed once (no per-part object scans; parts cached at spawn); runtime objects in one
  pass over the object list. FN6: fn6_ens 1024x512 + 256x256, 109 placed objects; fn5_ens 688 frames (27 panther rows) with a 1024 second atlas.
- **Sounds:** SFX_FN6 = 134 99 126 137 165 166 169 191 193 194 195 200 219 254 256 257 258 259 261 436 (228 KB at
  11025 Hz); SFX_FN45 += 126 193 (242 KB); convert_sfx.sh converts 137 254 256-259 261 436 at half rate.
- **Host:** FP_DBG_FP=1 prints `fp` (state), `fpdmg` (damage source / amount), `fpev` (rapid fall / missile blast);
  FP_DBG_FP=2 adds `fpp` (every part's x, y, angle).

## Session 17: Fortune Night lead notes (sounds, parallax, performance)
- Stage sounds: `level_def_t.sfx` is per act in FN (`FN(d, f, n, mus)` pastes `SFX_FN##n`; SFX_FN4 / SFX_FN5 are both
  SFX_FN45). snd_stage_load keeps ids shared with the next act and the character group, frees the rest, and logs
  `stage sounds N, sound RAM largest free block X KB` (game.c).
- parallax.c: `PMAXCH 32` chunks per parallax image (FN2's 5_1802 needs 30; over the limit par_load fails and the act
  draws no parallax).
- aqua.c bombs_frame: the enemy scan that arms a bomb monitor (3245 / 4056) tests distance before enemy_type and the
  image box; same result (a frame is at most 1024 px), much cheaper with FN's 600-870 objects.
- The AT / RM family passes (aqua_update, relic_frame, relic_gima_post) still run in FN because FN places RM-range
  reusable types (camera limits 368-371, chairs 336, layer switches 283, ...); about 2 ms per frame on the host profile.
  A per-type object index would remove that cost (not done: passes also act on runtime-created objects).

## Session 18: pellet sounds, shield sprites, gallery voices (port: objects.c, player.c, shields.c, game.c, fe_gallery.inc)
- **Sounds on a pellet (RE col_4_20 / col_7_20 / col_20_20, RV .snd):** shot 28; pellet death (hit or ground) 17 +
  explosion02 (objects.c explode2, was 80 = Lilac's hurt voice); a hit adds 2414's 25 + the character's hurt voice
  (Lilac lilac_hurt<n> = Assets 80-83, Carol 114 / 115 / 118 / 119, Milla 179-182; die 128 / 178). player_damage no longer
  plays 17 (2414 plays 25 + voice only). Host check: `FP_TRACE_SID=1` prints `sid <id>` and now also `voice <name>` for the
  resident voices (game.c voice_play). tests/test_gameplay.py `pellet_hit_sounds_*` / `pellet_miss_sounds_*`.
- **Shield sprites:** G.shl = `<dir>/<dir>_shl` (tools/import_frame.py, acts without an on-map shield monitor: DV1 DV4 AT3 RM1
  RM3 RM5 FN3 FN6), loaded by shields_bank_load() after level_start when G.p.shield != 0, or on the first frame a shield
  exists without sprites (shl_need, once per act), freed in level_free. VRAM 256 KB while loaded. Host:
  `FP_SH_DBG=1` prints `shv <frame> <dir> <OTHERS.B> <sprite found> <_shl loaded>` (tests/test_levels.py `shield_*`); the log
  line `shields: no sprites for <dir> (<dir>/<dir>_shl missing)` means the act's data predates session 18.
- **Gallery sound cards:** gal/<name>.wav (11025 Hz sample, <= 65534 samples) or gal/<name>.adp (longer: streamed once on
  the music channel by music_play_once, idle when music_active() drops; Y.play_t = -1 marks it). Only sx_08b02 (card 70).

## Session 18: the Panther fight fixes (port: src/game/fn_panther.c; tests/test_fn_panther.py s18_*)
- **Hook order:** fn_panther_input (before player_update) now also runs the FN6 floor events that precede the movement:
  8508 / 8509 (belt latch), 8512 (left edge), 8515-8517 (belt speed, virtual right press); floor_mode (after the movement)
  keeps 8513 / 8514 / 8518 (belt) / 8519 / 8523 and 8550's walk-in X >= 400.
- **rig():** mode-2 branch (FP.g_m2 in act 6): pose_m2 (8613), the 8614 angles, head y - 0.5 I, hip rumble, tail, unrotated
  cannon / jaw, the wheel object placed at the hip (drawn by fn_panther_draw pass 0 at its own x / y / angle). body_moves:
  8620 / 8621 follow the player. No rig after 8762 (fight group off).
- **runtime_pass:** 8657 target each frame, plasma re-aim / spin / player burst / Milla target burst / 300-frame cap,
  missile birth-frame turn, blast_step (Explosion06 pattern, x - 10), warnings by age (31 frames). debris_fx = OT_DEBRIS
  (bosses.c motion). post_boss: 8759, 8769 wreck.
- **Shared files:** fp.h + game.c: `fn_panther_hud_dy()` added to hud_dy (FN6 8764: -80 from the closing pose); game.c
  host-only `FP_DIFF` (global 12 for tests).
- **Host:** FP_DBG_FP=3 prints `fpo frame slot born type x y st tm hidden` for every runtime panther object; the `fpp` row
  (FP_DBG_FP=2) ends with `w:x,y,shown,scale,angle` (the wheel).
- **Per frame (SH4):** unchanged structure (one pass over the object list for runtime objects); a missile blast now adds up
  to 45 short Explosion06 effects (as the original; at most ~4 blasts overlap).
