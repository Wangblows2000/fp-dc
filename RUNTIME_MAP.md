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
| Down kick | Down + x in air | M19 / 7, then M0 / 19 | windup 15 frames at gravity 0.125, then vx = 5·facing, vy = +3.875, normal gravity until landing |
| Dragon Cyclone | z again in the air | M26 / 18 | vy = −3, gravity 0.1 **until landing** (releasing z does not end it; ran 78 frames in the trace) |
| Dragon Boost | hold c | M5 / 7, then M5 / 13 | a tap does nothing. Holding: 30-frame charge with speed frozen (2.5807 kept while running), then auto-fires: B = 12, held constant on flat (slope force still applies: 12 → 12.82 down a 45° slope) for 33 frames, then normal friction from the carried speed |

## Animation selection (playerskinsash_341, RV indices)
idle 0 · walk 1 (0 < |B| < 5) · run 2 (5 ≤ |B| < 9.5) · fastrun 22 (≥ 9.5) · jump 24 · skid 17 · lookup 11 · crouch 10 · attack 26 · airatk 25 · uppercut 28 · charge / down-kick windup 7 · down kick 19 · cyclone 18 · boost 13 · hurt 16 · defeat 38. Walk/run playback speeds are fitted (≈30 and ≈35+|B|), not read from code.

## Comparison harness
`tools/orig/run_all.sh` traces every `tests/orig/*.txt` in the original; `tools/orig/cmp_all.sh` runs the same inputs in the fpdc host build and checks key outcomes (apex, top speed, time to top, airtime, distance, stop timing). **16 / 16 sequences pass** (host-test vs original-trace).

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
