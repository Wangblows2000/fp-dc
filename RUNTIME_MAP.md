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
| Title menu | 3 (cam 43,0) | fe_hub.c title_events | 63/64 | 380-392, console layout | GALLERY inert; QUIT hidden (console) |
| File / mode select | 3 (cam 43,240) | file_events, delete_events | 63/64/56 | 393-437 | Adventure skips the cutscene; continue starts DV1 |
| Options | 3 (cam 523,0) | options_events | 63/64 | 464-511 | Controls / screen size / sensitivity hidden (console); SFX volume display-only on host |
| Character select | 6 | frontend.c FE_CHSEL | 63, 220, 56 | spec_charselect | Lilac and Carol playable (session 10); Milla locked |
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
