# RE notes: logic recovered from the original executable

Source of truth: the user's Linux build (v1.21.5, Chowdren). The binary is not stripped: every Clickteam event is a named `Frames::event_func_N`, collision handlers are `Frames::col_<n>_<frame>`, per-frame dispatch is `Frames::handle_frame_<N>_events`, object constructors are `create_<class>_<id>`. This file holds **prose and formulas only**. Decompiled or disassembled output lives in `build/re/` (gitignored) and is never committed or pasted into `src/`.

Labels: **RE** = read from the executable's code; **RV** = measured in the running original; **INFERRED** = estimate.

## Method (repeatable for any enemy, gimmick or character)
1. **Find the writer.** In a trace script, `<f> WATCH <obj> <alterable index|cnt>` sets a hardware write watchpoint and logs every write with a backtrace to `<trace>.watch` (`tools/orig/trace_orig.py`). Objects: `mv` (PlayerMovementValues_253), `skin`, `player`, `others` (PlayerOthers_126), `life` (GUILifeBaseMeter_128), `en`, `turr`, `cam`, HUD counters. The backtrace names the event function.
2. **Identify members.** `<f> WHO <func addr> <off,off>` logs the class (vtable symbol), position and alterables of `this+off` on each call of a function, e.g. the selected instance in a collision handler (`+0x213f0`, `+0x213e8`). One-shot version: `tools/re/who.py`.
3. **Read the code.** `tools/re/decomp.sh 'Frames::event_func_N()'` (Ghidra headless on an unanalysed import, ~10 s per call) → `build/re/*.c`; `tools/re/annot.py` resolves `_UNK_` constants to their double values and names alterable offsets (`alt + 0x50 + 8k` = letter k). Ghidra drops assignments in min/max patterns (empty `if` blocks): read those few lines with `gdb -batch -ex "disassemble 'Frames::event_func_N'"` (look for `maxsd`/`minsd`). Constants used by movement classes are **floats**, not doubles.
4. **Find related code.** `tools/re/callers.py '<function>'` (who calls it), `tools/re/calls_from.py 'Frames::handle_frame_21_events()'` (every event run in DV1). Events touching one object list share its `Frames` member offset (grep the disassembly for it).
5. **Confirm** with an original trace and a comparison sequence; the trace must start from a fresh game (probes can drain petals and lives, which changes later traces: restart with `start_orig.sh` + `route_dv1.sh`).

Engine facts (RE):
- Per frame: `Frame::update` = `handle_pre_events` → `Frames::update_objects` (built-in movements and animations) → `handle_events` (the event list, including collision tests) → display centre. Objects created by events move first on the next frame.
- `Active::update` (animation): counter += speed; the frame advances only when the counter goes **above** 100, then −100 per frame. `Active::force_animation` resets the frame index but **keeps** the counter.
- Selection loops in events keep an instance when the condition is true.
- `FrameObject::is_near_border(b)`: false iff the object's box is inside the 426×240 view grown by −b on every side.
- `Frame::set_display_center(cx, cy)`: view left = clamp(cx − 213, 0, frame_w − 426), top = clamp(cy − 120, 0, frame_h − 240).

## Damage (event 2414, col_4_20)
- Collision handlers (e.g. `col_4_20`, pellet vs Lilac's sensor) set movement **R = 1** and add the source's damage to `GUILifeBaseMeter_128.B`, only if R = 0 and **PlayerOthers.E ≤ 0**. The pellet is destroyed with `effectsexplosion02` and a sound.
- Event 2414 (needs R = 1, petals > 0, difficulty global < 3): half-petals lost = mult × B / 5, where mult = 2 − difficulty if that is ≥ 1, else 1 (Hard 2, Normal 1, Easy 1; Casual takes no damage). **B = 0.75·B − 2.2·Q, G = 0.75·G − 4.2** (Q = facing: the knock-back is away from the facing, not from the source), M = 8, L = N = R = S = 0, sound 25 plus `sound_hurt_<0..3>`. PlayerOthers **D = 2, E = 60**; E then counts down 1 per frame (RV f189 → f249), so **invulnerability is 60 frames**. Petals then fly out (`loop_petal_loss_13`, not ported yet).
- Lilac's hurt sensor `PlayerCharacterSensor_375` sits at her reference point. It has 32 direction images (rotated boxes); direction 0 is 15×46 with hotspot (7, 34), i.e. x −7…+8 and from 48 to 2 px above the feet.

## Turretus (enemyturretus_878; events 4766–4777, 4773 fires)
Created with A = 8, D = E = 0, direction 14. Alterable D is the state, E a timer. Per frame, in event order:
| Event | Condition | Action |
|---|---|---|
| 4766 | flag 6 off | D = 3 |
| 4767 | D = 0 | animation 0 (idle, speed 20) |
| 4768 / 4769 | D = 0 and player left / right of it | direction 15 / 1 (11.25° above horizontal) |
| 4770 | D = 0 and within 200 px of the view | E += 1 |
| 4771 | D = 0, E > 60, player state Y ∉ {10, 11} | E = −40, D = 1 |
| 4772 | D = 1 | animation 7 (shoot, 15 frames, speed 25, plays once) |
| 4773 | D = 1 and animation frame = 7 | sound 28, create EnemyPellet at the action point (±18, −14), with the Turretus's direction; D = 2 |
| 4774 | D = 2 and animation 7 no longer playing | D = 0 |
| 4775 | D = 3 | animation 8 |
| 4776 / 4777 | hit by Lilac's attack | not ported yet (current kill rule is RV-tuned) |
RV check (hurt_turretus): first E increment in update 101 (after the teleport), D = 1 at row 162, shot at row 187; the port matches all three.

## EnemyPellet (enemypellet_934; PinballMovement)
Max speed 50, deceleration 0, gravity 25; direction from the Turretus. `PinballMovement::update` (constants are floats): vy += gravity/10; speed = |v| clamped to 100; position += v/10, with the integer step taken by **floor** and the fraction carried. Reproduces all 30 logged pellet positions exactly. Destroy on walls / outside the view: INFERRED (handlers `col_7_20`, `col_20_20` not identified yet).

## Camera (MasterCamera_408; loop_camera_mode0_13, events 1357, 2873–2886, 3539)
Alterables at DV1 start: I −16, J 0, K −48, L 16, P 120, Q 64, R 96, X 213 (half width), Y 120 (half height), bounds C 0 … E, D 0 … F. Per frame:
- **Horizontal:** if Lilac X < A + I, A = X − I; if X > A + J, A = X − J (each only within C + 213 … E − 213). So the centre stays between X and X + 16 and snaps after a teleport.
- **Vertical, grounded** (movement L = 1): on landing W = B − Y; W eases toward 0 by 4 per frame; B = clamp(Y + W, D + 120, F − 120). **Airborne:** if Y < B + K, B = Y − K; if Y > B + L, B = Y − L.
- **Look-ahead G:** +2 per frame up to +Q while B ≥ 5 facing right on the ground (mirror for left); ±2.5 per frame in state M5 (boost); ±2 in M19 (kick windup). Decays 2 per frame toward 0 when airborne, or when |B| < 5 and not in M5/M19 (event 1357). If camera M ≠ 0, G and H return to 0 by 4 per frame (2873–2876).
- **Look up / crouch H:** counter N (state M2, grounded, B = 0) or O (M3); after P = 120 frames H moves 2 per frame toward −R / +R; otherwise it returns to 0 by 2 per frame.
- **Display offsets:** S = G + shake(Z), clamped so A + S stays within the bounds, and S = min(S, max(0, S − (A − X))) when S > 0 (mirror when S < 0). T is the same with H, B and Y.
- **Display centre** (3539) = floor(A + S), floor(B + T); Z (shake) −0.1 per frame.

## Lilac's movement block (frame 21 events ~1542-1940 + loop_player01* sensor loops)
795 frame-21 events touch the movement object (PlayerMovementValues_253). Recovered so far:
| Event | Rule |
|---|---|
| 1573 | a character constants table (this copy for another character: C 4.5, J 0.5; Lilac's values match the stage-start dump: C 5, D 15, E 0.07375, F 0.203125, H 15, I 0.25, J 0.84) |
| 1574 | B, G, N **floored to 4 decimals** every frame (start of the block). Needs doubles: in float, -2.9 floors to -2.9001 |
| 1577 | G = min(G, H) **before** gravity, so falling speed tops out at H + I = 15.25 |
| 1584-1611 | movement and sensor collision: see "Sensor collision" below (ported) |
| 1591 | landing: if V = 1, L = 0, G >= 0 and 22 < N < 338: B = B - G * J * sin(N) |
| 1592 | landing: W = G, G = 0, L = 1, K = 1 |
| 1603 | ground angle from the angle sensors (see "Sensor collision") |
| 1605-1607 | angle smoothing: O = fmod(O + (N - O) * max(0.175, abs(B) / D), 360) within 45 deg, else O = N |
| 1609 / 1610 | slip off walls/ceilings when slow; detach when the bottom-big sensor is clear: N floored, G = -B sin N, B = B cos N, N = O = L = 0 |
| 1621-1636 | horizontal acceleration / friction (see "Horizontal speed"); 1635/1636 wall stop |
| 1644 / 1646 | slope force with N in (35, 325) deg and state != 6: B -= 0.38 F sin(N) if 0 < abs(B) < C, B -= F sin(N) if abs(B) >= C |
| 1647 | airborne: G += I |
| 1648 | airborne, -4 < G < 0, abs(B) >= 0.125: B *= 0.96875 (a value read from PlayerActionValuesB) |
| 1649 | airborne, G < 0, ceiling sensor: G = 0 |
| 1651 | grounded: G = 0 |
| 2015 | down-kick launch: B = 5 (facing), M = 0 |
| 3047 | jump from the ground (B, G from N; jump 7) |

## Sensor collision (events 1400-1520, 1584-1611; port: src/physics/sensors.c)
The movement object's X, Y is the reference point (14 px above the feet on flat ground). 15 invisible
sensor objects are placed around it, tested for pixel overlap, and their alterable Z holds the result.

**Shapes** (Image data of the sensor classes, read from the running original; extracted images match):
discs of radius n/2 - 1/4 (9x9: d^2 <= 18, 11x11: <= 27, 27x27: <= 175), a 3x3 box, and the edge bar
with 8 direction images (dir 0: 3x25 hot 1,1; dir 4: 18x18 diagonal x - y in [-2, 1], hot 1,1; dir 8:
25x3 hot 2,1; dir 12: 18x17 x + y in [16, 19], hot 1,16; dir 16: 3x25 hot 1,24; dir 20: 17x18
x - y in [-2, 1] minus (0,0), hot 16,17; dir 24: 25x3 hot 24,2; dir 28: 18x18 x + y in [16, 19]
minus (0,17), hot 17,1). Collision masks are alpha != 0 (Image::upload_texture, collide_sprite_sprite;
the terrain art has no partial alpha, so the old >= 128 threshold gave the same map).

| Sensor | Shape | A (along) | B (normal) |
|---|---|---|---|
| Player_326 (main) | 27 disc | at (int X, int Y) | |
| Left / LeftBig, Right / RightBig | 9 / 11 disc | -12 / -13, 12 / 13 | -2 |
| Top / TopBig, Bottom / BottomBig | 9 / 11 disc | 0 | -12 / -13, 12 / 13 |
| Slopes, Platform | 11 disc | 0 | 24, -5 |
| LeftAngle / RightAngle | 3x3 | -7 / 7 | 0 + scan (alterable Y) |
| LeftEdge / RightEdge | bar, direction (int)(N / 11.25) | -8 / 8 | 12 |

**Placement:** x = floor((sin N * B + A * cos N) + X), y = floor((B * cos N + Y) - sin N * A), clamped to
the camera bounds C..E, D..F. The original's trig takes cos = 0 at exactly 90/270 and sin = 0 at exactly
180, and converts with 57.29577951. Edge direction: Active::set_direction with 8 images; directions 4k+1
use 4k, 4k+3 use 4k+4, 4k+2 keep turning toward the last exact direction (Active +0xd1).

**Test (Z = 1):** overlaps the background; or the always-solid group (QualifierList +0x218c0: monitors,
YellowSpringRight, spikes, spiral-tunnel mask, BreakableRock, MinibossBarrier); or, while grounded
(L = 1), the obstacle group (+0x218d8: bridge logs, YellowSpringUp/UpLeft/UpRight, LadderTop,
LevelValleyPlatform1, LevelMovingPlatform2/3). In the air with G >= 0 the bottom sensors also stand on an
obstacle when the platform sensor is not inside one. Edge sensors are cleared again by group +0x218f0
(monitors, springs, bridges, ladder tops, spikes, platforms).

**Per frame, in event order:**
1. 1574-1577: B, G, N floored to 4 decimals; B within +-D (15); G = min(G, H) (15).
2. 1584: X += cos N * B, Y -= B * sin N; wall loop (int)(|B| + 1) times: place Left/LeftBig and Right/RightBig;
   Right touching: X -= cos N, Y += sin N; Left touching: X += cos N, Y -= sin N; stop when neither touches.
3. Air only (L = 0): 1585 Y += G, then the up loop (10x: Top touching and G <= 0: Y += 1; stop when Top is
   clear); 1586 wall loop 20x; 1587 down loop (10x: Bottom touching and G >= 0: Y -= 1; stop when clear).
4. Landing 1588-1592 (L = 0, G >= 0, BottomBig touching): N = O = 0; edges; if both edges touch, N = O =
   the measured angle; if 22 < N < 338, B = B - G * J * sin N (J = 0.84); W = G, G = 0, L = K = 1.
5. Ceiling 1593-1600: rising faster than 1.75 into TopBig: P = 1, N = O = 180, all sensors; BottomBig
   touching: P = 2, edges; both edges: N = O = angle; N < 170 or N > 190: B -= G * J * sin N / 0.35, then
   W = G, G = 0, L = K = M = 1 (runs on the ceiling); otherwise N = O = 0.
6. Ground (L = 1), 1601: slopes-down loop (15x: main clear and Slopes touching: X += sin N, Y += cos N;
   main touching: one more step down and stop), slopes-up loop (15x: main touching: X -= sin N,
   Y -= cos N). 1602 edges; 1603 both edges: N = angle (O untouched).
7. 1605-1608 (L = 1, both edges): if |N + 360 - O| < |N - O|, N += 360; d = N - O: |d| < 45: O = fmod(O + d k,
   360), k = max(0.175, |B| / D); |N - O| >= 45: O = fmod(N, 360); N = fmod(N, 360).
8. 1609: 75 <= N <= 285 and |B| < 0.4 (+ 2|cos N| when cos N <= 0): leave the ground. 1610: BottomBig not
   touching (its last test, i.e. the previous frame's step 9 on the ground): leave the ground. Leaving:
   n = floor(N), G = -B sin n, B = B cos n, N = O = L = 0.
9. 1611: all sensors placed and tested (their Z values feed the speed events of this frame and step 8 of
   the next).

**Angle measurement** (loop_player01detectangle, 20 steps): both angle sensors' scan Y reset to 0; each
step places and tests them and adds 1 to the scan of a sensor that does not touch. A = Lx - Rx,
B = Ly - Ry (integer positions), then loop_toolsanglecalculator: C = -atan(B/A) in degrees (0 if A = 0),
+180 if negative, +180 if B < 0; C = 0 becomes 180 if A > 0 or 90 if B > 0; C = 180 becomes 270 if B < 0.
Example: the DV1 start slope gives atan(1/14) = 4.0856 degrees.

**Speed events using sensors:** 1635/1636 B > 0 with RightBig touching (B < 0 with LeftBig) and not in M5:
B = 0. 1649: in the air, G < 0, TopBig touching, not M5: G = 0. 1637-1640/1650: boost wall/ceiling bounce
(M5, not ported). 1752-1755: pushing state M11 against a wall (not ported).

## Horizontal speed (events 1621-1634; M0 group)
E = 0.07375, C = 5. Ground with no input toward the motion side: B >= 0 -> max(0, B - 5E), then if |B| < 2
max(0, B - 4E) (mirror for B <= 0). Holding a direction with B below C: B += E, and if |B| < 2 and N is not
within 80..280: B = min(2, B + 8E). In the air, additionally B += E (below C) and B = min(2, B + 4E) (below
2). The input rules are off in states M2-M6; all of them need the movement flag S = 1 (the skid, M4, sets
S = 0). OTHERS.O is a signed input lock (1627/1628 count it to 0).

## Controller (PlayerInput_21, frame member +0x470)
Alterables: A up, B down, C left, D right, E jump, F attack, N special held; G..K up/down/left/right/jump
pressed, L attack pressed, O special pressed (RV with WHO on events 1742/1574).

## States and their event groups
Each movement state M switches on its own event group (events 1681-1721: flag per M value; e.g. M0
0x21a61, M2 0x21a40, M3 0x21a55, M4 0x21a58, M5 0x21a3c). Transitions happen in event order inside a
frame, and a group switched on by an earlier event only runs from the next frame. Examples: 1742 (M0,
grounded, B = 0, Up, no direction) -> M2 look up and switches the M0 group off, so 1766 (M0: Up + attack
-> M21 uppercut) cannot fire on that frame; 1818 (M2 group: attack pressed with Up) -> M21. 1743: |B| < 2,
Down, no direction -> M3, B = 0. 1744-1747: input against B (|B| >= PAV_A.H) -> M4 skid, S = 0. 3047/1750:
jump from the ground: G = cos N * A - B sin N, B = cos N * B + A sin N (A = -7), N = 0, L = 0.

## Handlebar (gimmickhangingidlesensor_674; events 4057-4066)
4065: the top-big sensor overlaps the hang group (QualifierList +0x22040), G > 0, M != 9, controller B = 0 (Down not held): M = 41, X = sensor x, Y = sensor y + 32. 4062/4065 rewrite the position every frame; B becomes 0 the next frame while G keeps gaining gravity. Hang sensor image 50x15, hotspot (25, 0). Release and moving along the bar are in 4057-4064/4066 (with `Frame::test_ladder`): not read yet.

## Results screen (DV4 / frame 24; located, not ported)
Lists: LevelCardActClear1 +0x8bf0, LevelCardActClear2 +0x8c30, LevelCardBonus1-3 +0x8cb0/+0x8cf0/+0x8d30, TotalBonus +0x8d70, BossSerpentineHead +0x9070. Sequence events 4560-4589 + `loop_tally_19` + 5389/5395, driven by a controller timer (B, counts to 200): 140 HUD shifts up (`loop_gui_shiftup_14`), 150 music strings (`play_music`, `stop_music`) and counters, 180 clear-card animation, tally sub-states (A = 1, 2) with their own timers (120, 100), character voice lines (e.g. Milla's `yay` / `thatwasfun`), then (+150) records.dat save, Steam upload and the frame change. Loops are started by name through a hash table, so call graphs miss them: find them by string (`str_*` symbols) or by object-list offsets as above.

## Title screen (frame index 2 = on_frame_3, events 307-330; port: src/game/frontend.c)
- Starfield: Speed counter 5; up to 81 stars; A = ((r·5)>>9) − 20, B = ((r·240)>>15) − 220, C = 256; screen = planet + A·256/C; direction = int(C/8).
- 250 TitleStars: x1 = int(213 − 213·cos(i⁸) + i), x = 107 + ((x1 + 213) mod 426); y = (int(sin(i² − i)·120 + 120) + 120) mod 240; frame = i mod 21; removed when near the border.
- Moon: A from 180, −0.08 per frame; x = px + 48·sin A; y = py − 10 + 48·cos(A + 30). Planet starts at (163, 184): x −1 when timer % 40 = 20, y +1 when timer % 39 = 30.
- Fade-in: A +1 per frame, alpha = clamp((128 − 4A)·2). Press Start pulse: D +4 per frame, alpha = clamp((128 − (128·cos D + 64))·2).
- On press (jump / attack / start): sound 100, flash (direction 1, 10 loops), black fade, music to 60 %, shake Z = 5 (−0.4 per frame; X = X0 + cos(200Z)·Z, Y = Y0 + sin(200Z)·Z·0.5). After the flash: fade A −2 per frame, music −1 per frame; at A ≤ −100 the music stops (327) and the next frame starts. 32 s idle (325) does the same. The title menu frame restarts m_title (331).

## Items and monitors (events 2543, 1288-1291, 2527-2531, 2632-2670)
See RUNTIME_MAP "Monitors and items". The energy refill starts 41 frames after the stage start (RV). The cyclone drains 0.25 per frame and the boost 3 per frame; both use the state at the start of the frame (RV cyclone / cyc_down / cyc_then_dk traces).

## Move overlays (events 2043, 2834, 2844, 2845)
- 2043 (cyclone group, every 6 frames): ShieldInstaShield_407 at the player, direction 8, A = 2, **set invisible**; RingSparkle_142 at the player + rand(41) − 20 on x and y (cross_seed LCG 0x343fd / 0x269ec3).
- 2834: M = 5 and the global frame counter even → RingSparkle at ±8 (rand(17) − 8).
- 2844: Others D = 1 and flag bit → creates Sparkle1-4 at the player with C (radius) / D (angle) = 20/0, 20/180, 10/0, 10/180. Their own loops (RV): D +7 or +10 per frame, position anchor + C·(sin D, −cos D).
- 2845: Others D = 1 and the global counter % 6 = 0 → a Sparkle5_284 at each of Sparkle1-4.
- Lilac's hair whirl during the cyclone is the hair overlay's animation 18, not a separate object.

## Front end after Press Start (session 6; specs in build/re/fe/spec_*.md, port: src/game/fe_hub.c, frontend.c, fe_stsel.inc)
Method: `tools/re/member_map.py` (gdb) maps each frame's member offsets to object classes; Ghidra reading of `handle_frame_N_events`; confirmed with `tools/orig/trace_fe.py` (objects, alterables, sounds, frame changes per frame) and `tools/orig/cmp_fe.py`.
- **Frame inventory (index = on_frame_N − 1):** 2 Press Start, 3 hub (title menu / file select / mode select / options / controls, one 1440×480 frame), 5 Time Attack stage select, 6 character select, 7 dispatcher (picks the next frame from globals), 8 stage card, 10 gallery, 11 finished-file stage select, 12 mahjong (minigame). 20-23 Dragon Valley.
- **Menu input (RE, RV):** InputsP1_23 rebuilt at the end of each frame: I/J/K/L = up/down/left/right (1 on the press edge, 2 while held), M = jump (confirm), N = attack (back), O = special, P = start. Menus therefore react one frame after the press. Port: every menu, Press Start included, reads the previous frame's input.
- **Shared machinery (RE):**
  - **Cursor:** the Crystal object goes to the selected row object's action point. The action point now ships in the `.spr` `ACT1` section.
  - **Row shake on selection:** Z = 4, −0.4 per frame; x += floor(cos(200Z)·Z), y += floor(sin(200Z)·Z·0.5).
  - **ScrollDot camera:** moves 24 px/frame vertically and 30 horizontally; view = dot − (213, 120).
  - **Exit wipe:** Fade_50 + FadeEdge_77 (120×240 wedge) +32 px/frame; the frame change fires when F > 50.
  - **Entry:** 104 tiles shrink over 6 frames, 4 random tiles started per frame.
- **Console layout (RE):** global G[32] = 1 selects the original's console variant. QUIT and the Screen size / Controls / Sensitivity options are hidden and the cursor skips them. The port uses it (D-020).
- **Title menu (events 380-392):** START GAME → file select; TIME ATTACK → character select with the Time Attack flag; OPTIONS → options panel; GALLERY plays the move sound only (gallery not ported).
- **File select (393-437):**
  - 8-slot carousel, 120 px pitch, wraps; LEFT/RIGHT on the slot moves the carousel.
  - CLASSIC/ADVENTURE mode panel slides in.
  - The DELETE confirm sub-panel is there.
  - RETURN goes back.
- **Options (464-511):**
  - Difficulty is dirs 0-3 (HARD, NORMAL, EASY, CASUAL).
  - Music and SFX volume step by 10.
  - OKAY writes the settings (records.dat in the original; the VMU save in the port).
- **Character select (spec_charselect.md):**
  - Lilac, Carol and Milla rows. The port shows Carol and Milla locked with the padlock overlay (D-020) and the move sound when you try them.
  - Choosing Lilac plays 63 + her voice 220, then a 20-frame timer, then the wipe (56).
  - Exits: Time Attack → stage select (5); Classic/Adventure → dispatcher → DV1; RETURN → hub.
- **Time Attack stage select (spec_stageselect.md):**
  - Vertical list with icons 30 px apart, eased 1/8 per frame.
  - Locked stages show the lock.
  - Choosing slides START? / YES / NO in from x 613 to 320, 20 px per frame, one after another; YES → card → stage.
  - The record box reads the stage's best time and crystals.
  - **Port difference:** the confirm state is reached 3 frames later than in the original (f539 vs f542 in r_ta). The slide start/threshold is INFERRED.
- **Music (RV):** the hub plays m_menu2; character select plays m_menu1 (Time Attack) or m_results (Classic/Adventure); the stage select keeps m_menu1.
- **Sound ids (RV):** 100 press start, 63 confirm, 64 move, 56 wipe/back-confirm, 220 Lilac voice.

## Pause menu (DV1 = frame index 20; spec_pause.md; port: src/game/game.c pause_*)
- **Events:** 4878 open, 4879 close, 2928 open/close step timer, 4296/4299/4300 art, 4297/4298/4301/4302 cursor, 4303 confirm, 3551 wipe, 4881 restart → loop_restart_20 (event 1276), 4305 quit → loop_return_0 (frame 7 dispatcher; from Time Attack it then goes to stage select, RV).
- **Groups:** the gameplay group byte 0x21a5d is turned off while the pause group 0x21a4d is on, so gameplay freezes completely and the HUD stays.
- **Opening:**
  - Start edge, not during the card or a wipe.
  - Sound 63; music paused.
  - GUIPause at (214,101).
  - Art frame 3 (RESUME / RESTART / QUIT), or frame 4 with RESTART greyed when lives ≤ 1 (the lives snapshot is taken at frame start, events 4311/4312).
- **Cursor:**
  - Crystal at (166, 141 + 24·row).
  - Up/down 64, clamped, skipping RESTART when it is greyed.
  - The row is kept between pauses.
- **Closing:** Start or RESUME closes and the music resumes.
- **RESTART / QUIT:** sound 56, 50-frame wipe. RESTART reloads the act with lives − 1 and the stage-start crystal count. QUIT leaves to the front end.
- **Port:** QUIT from Classic goes to the hub (INFERRED; the original's dispatcher target for a file game was not traced).
- **Trace check (t_pause):** sounds 63 at f301, 64 at f321/f341, 56 at f361, frame change at f412 (51 frames). The port's sequence and the 50-frame wipe match in the host test.

## Save data (port design; the original uses records.dat / file*.dat on PC)
- **File:** `FPDC.SAV` in one VMU file, built with `vmu_pkg` (3-frame 16-colour crystal icon, derived at build time from the game's ObjectCrystal sprite).
- **Payload: 515 bytes**, CRC-16:
  - options: difficulty, music, sfx, sensitivity, unlocks;
  - 8 file slots: character, difficulty, mode, stage, lives, crystals, petals, time, deaths;
  - Time Attack records: 16 stages × 4 characters, time and crystals.
- **Size on the VMU:** 1 header block + 3 icon blocks + 2 data blocks = 6 blocks (3 KB) of the 200, well under the 50 KB target.
- **Behaviour:**
  - Loaded at boot.
  - Written on options OKAY and on a new Time Attack record.
  - Uses the card that already holds it, else the first card with 6 free blocks.
- **Not yet written:** file-slot progress at stage results (results screen not ported).

## VMU screen (port design)
- 48×32 1-bit LCD.
- **Menus and title:** a 6-frame crystal animation (ObjectCrystal_317 at 1.5× with a sparkle, thresholded to black and white), one frame every 6 ticks.
- **In a stage:** the current character's life icon (from GUIIcon_346, 2× line art), "×" and the lives count in a 3×5 font.
- **Pushes:** only when the image changes.
- **Data:** frames are made by `tools/make_vmu_icons.py` from the game's own images into `ui/vmu_lcd.bin` / `ui/vmu_icon.bin` (gitignored data).

## Session 7 additions (specs in build/re/spec_ladder.md, spec_boost.md, spec_platforms.md, spec_ganseki.md)
- **Ladders (RE + original traces ladder_grab_climb, ladder_down_jump, ladder_top_enter, ladder_fast_grab).**
  - LadderTop_428 sits at each ladder top (x = ladder centre). The grabs run at the end of the frame:
    4057 (Up, |B| < 4, LadderTop above within ±15, point inside a ladder piece) → M46, Y = 27;
    4058 (Down, |B| < 4, BottomBig sensor touching a LadderTop) → M25 enter from the top, snap to LT.y + 8;
    4059/4060 (Up at |B| ≥ 4, LadderTop within 25 px behind) → fast catch, Y = 47 until the catch anim ends (1928).
  - M46 frame: 4040 sensors, speed events with T = 0, 1933/1934 clamp ±4 and zero without left/right, 1935-1937
    climb ±2 (stop at TopBig), 4044 LadderTop under feet with Up → climb-over (O counts to 8, 4045 → Y = LT.y - 36,
    camera +24), 4046 off the ladder → normal, Jump → 1950 (G = -4) or 4047 drop with Down.
  - M25: 1888-1890 eight frames then M46. **RV:** the first M25 frame snaps back (modelled as `lad_o = -1`).
- **Dragon Boost (1637-1643, 1650, 1992-2012):** charge latch, fire, drain, min 10, wall bounce/reflect, ceiling flip, 30 invuln.
- **Level-edge clamp (1641-1643):** B < 0 and X ≤ 16; right edge at width - 16 (width + 32 while the clear timer runs).
- **Cyclone:** air gravity +0.25 then -0.15 (2041); grounded cyclone jump × 0.7 (2045).
- **Platforms (3270-3307, 4061, 4737/4738):** moving (types 2 and 3) and valley platforms carry Lilac by their delta; bridges
  are 12 logs that sag towards the standing point. Platforms, logs and LadderTop are in the obstacle group.
  Air landing on an object only when falling (G ≥ 0) and the 11-point platform sensor at B = -5 is clear of every obstacle.
- **Ganseki Jr (spec_ganseki.md):** walk / wake / curl / gravity / floor; HP 8 + damage; hurts on contact via player_contact(5).
  **Not ported:** crystal absorption and spin.
- **Push (M11, 1752/1753 enter; 1875-1881, 2322 leave):** ground, |G| < 0.2, held direction into a big side sensor.

## Session 8 additions (specs in build/re/spec_*.md; every item below was checked against an original trace unless marked)
- **Trig:** Chowdren divides degrees by 57.29577951308232 (not the truncated 57.29577951) and does not reduce large angles;
  sin(180) and cos(90/270) are exactly 0. Switching every gameplay sin/cos to this fixed the last Tredd/snake drifts.
- **Movements:** BallMovement moves speed·0.125 px/frame (floor steps, fraction kept); VectorMovement moves speed·cos·0.01,
  i.e. `int(dist·20)` = 20 % of the remaining distance per frame (the DV2 snake chain).
- **Tredd (DV3 miniboss, 5241-5260):** gait/turns matched 1100/1100 frames; column E = H at 5249; rock breaks use
  mask-vs-mask overlap; claws from L-1 at 140/200/280; bombs + blast; death sequence.
- **Serpentine DV2 encounter (5049-5093):** geysers/arcs/rest/fireballs, segment death; 16-px trigger column. RV snake_t1 271/271.
- **Serpentine DV4 boss (5461-5578):** boulder rain, arcs, wheel + bounce ball, hit cycle K = 3, retreat, final defeat, cinematic
  camera and the scripted exit into the results (path A).
- **Boulder chase (DV4, 5401-5460):** camera mode 3 (minimum bound), stages, ramp jump, BossTunnel2 walls solid until broken,
  breakables, Lilac shove/crush/respawn.
- **Results overlay (DV4, spec_cards_results C):** positions from screenshots, timings 72 / 292 / 441 RV; save at D = 150.
- **Parallax (3541-3543, 5094, 5101-5105):** F += G; sx = fmod(trunc(D·vl + F − vl), A); sy = trunc(E·vt + C) − vt; DV2/DV4
  switch pieces at view top 1360; water surface strip at y 2700.
- **Keystones (spec_keystones):** crash and chest-monitor spawns, rise/bob/magnet/homing, pickup effects, MasterLevel O.
- **Roll state M6 (spec_roll_gimmicks, RV spiral_dv1 / spiralb_dv1 / balltunnel_dv3):** entry 4739-4744 on the tunnel mask
  (q27: spiral mask in DV1/DV2, LevelBallTunnel1 in DV3); constants O 0.0334, P 0.2, Q 0.035, R 0.25; acceleration and slope
  force skipped, ground friction kept; group 1851-1864. **Correction:** 1862 is an OR event — (|B| < 2 and OTHERS.N > 30)
  OR Up pressed → M0; the slow roll stands up by itself (RV). Facing 2421/2422 run again once M ≠ 6 (RV spiralb f94).
  Skin anim 7 for OTHERS.Y 8 (2438). M1 keeps the ball (Z = 1) in the air until landing (RV).
  Monitors and BreakableRock are non-solid in M5/M6 (1581) and break when a big side sensor touches them with Z = 1 and
  OTHERS.K ≤ 0 (4762), each giving K = 5; the rock uses box collision (RV contact one frame earlier than its mask).
  5181 and 4749 write N on list +0x60b0 (another character's part), not MV.N (RV: no effect on Lilac).
- **Spiral helix (4748-4755):** top: C += B, B = min(B + 0.1, 12), X = Top.x + 72 sin C, Y = Top.y + 0.3 C (new C), exit at
  C ≥ 720 or C < 0; bottom: old C, B + 0.05, exit at C ≤ −720 or C > 0. T = U = 0, mask flag 0 on, pad locked; M stays 6.
  The front overlay shows while 90 < fmod(C, 360) < 270. RV: L = 0 during the ride, G accumulates to 15.25.
- **Spinning logs (DV2, 4980-4986):** log y = int(ML.C), mask y = int(ML.C + A); standing sinks A 0.5/frame, spins the log
  (direction += int(A/40)); the mask is ignored at A ≥ 15. Not traced (the teleport is blocked by the camera bounds).
- **Pushable block (DV2, 2711-2735):** RV pblock_dv2: block and Lilac step 1 px per frame from the frame after the M11 entry;
  B timer 4 (shows 3); dust every 3 steps; a resting block keeps its Y (no bob, so 1271 lifts only when its bottom row is in
  the ground); pushed into a wall it keeps D = 1 and Lilac stays put; 1876 leaves M11 only after the 4-frame grace PAV_B.S.

## Shields and water (session 8; specs build/re/spec_shields.md (static RE), spec_water.md (RE + 4 DV3 traces))
- **Shields:** OTHERS.B = 1 wood, 2 fire, 3 metal, 4 water, 5 earth (earth is 5, not 2). Monitor icon frames C = 10/3/4/5/11;
  the reward comes when the MonitorIcon's A reaches 64 (2640-2658): sound 0/2/1/3/57, B = type, C = clamp(difficulty, 0, 2).
  2412 (C > 0): C -= 1, E = 30, sound 315; 2413 (C ≤ 0): B = 0, E = max(60, 30·diff + 30), sound 11; both clear MV.R, so
  no hurt at all. Metal: no damage from spikes (3259-3262), bamboo spike 2 (5228), Tredd claws (5237/5238), Ganseki B = 0
  (4392). Fire: boss fireballs harmless (5071/5310), front flame burns one touching enemy every 8 frames (2746), lost in
  water (2691). Wood: petal magnet ±119 px and a petal every 15 crystals (2684); earth: crystal magnet; homing
  pos = (pos·(100 − D) + MV·D)/100, D += 1. Visual: additive 56×56 at (X − 10 sin a, Y − 10 cos a), anim 7 appear / 8
  vanish, hidden while invincible. HUD: none. Act change keeps the shield (OTHERS is global; INFERRED).
- **Water (DV2 5109, DV3 5314: ML.A = 1, ML.B = 2700; spec_traversal §10 was wrong):** entry/exit only while airborne at
  Y ≥ / ≤ 2708 (4150/4153): G ×0.2, B ×0.5 / G ×1.3 (−5 first when swimming: leap −6.5), sound 14, splash, J = −30.
  Next frame's tables: E 0.056875, C 4, I 0.15, jump −6, cut −2, roll O 0.0217 / P 0.12. 1678 fall cap 2 (+1 Down);
  1802 swim (up/jump held): G = min(max(G − 0.2, −3), −1); 1758 hold-jump re-jumps; no air cyclone; skin anim 39 (code 26).
  Air J: +1 at t % 60 == 6 below the surface + 8 (no water shield); dings 5/10/15, music fade at 20, digits 5..0 at
  20-30, drowning at 32 (RV 62 s). Bubble patches (DV3 only) refill J by 1/frame to −30 and cut J ≥ 20 to 18.
  RV checks: entry 1.2 → 1.35 → cap 2; walk −0.51187 (9E) then +E; jump −6, swim −3, leap −6.5, re-entry 1.4 → −1.0.

## Session 9 (rendering order, DV4 exit, sounds)
- **Blend modes (RE):** WaterfallBack blend alpha 0x9c; WaterfallFront (create_waterfallfront_1037) `set_shader(0x20)` =
  additive; GeneralWater tint dst·(1 − src); horizon strip additive. No level art has meaningful per-pixel semi-alpha
  (scan of the converted atlases: only the pause banner, results cards and the monitor icon), so the water/waterfall
  look is per object. Port: deferred translucent commands keep their draw-call depth (render_pvr.c `dcmd_t.z`).
- **DV4 exit (RE, spec_dv4_boss_events §9-10, not traced):** 5547 turns the normal camera group off (cinematic camera
  holds the surface), 5567 places Lilac as the boost ball (skin/hair anim 7 speed 50, angle 0, N = O = 0), 5569/5570
  release her at y < 1230 in M6 with G = -4. A forced original setup (SETV/SETB) fired 5547 with the wrong head/body
  instances, so there is no trace of the real sequence.
- **Sounds (RV, tracer `.snd`):** Lilac hurt = 25 + one of 80/81/82 (17 also on projectile hits). Monitor break = 307 +
  12 on the break frame (traces mon and inv, DV1 crystal monitors; RE Frames::event_func_2623 plays 307 for the whole
  monitor group, so the invincibility monitor shares it). Rock break = 13 (spiral_dv1). Attack hit on a monitor 21.
- **BGM:** ChowdrenAudio loops whole files (no loop points, SD); the port's ADPCM seam fix is D-033.

## Loading pipeline (session 7, measured with src/game/loadprof.c)
Per load the profiler logs: heap (mallinfo), largest free VRAM, and time in open / read / texture upload / sfx / decode /
music / object creation / other, plus allocation count and skipped work.
- Before: DV1 41.1 s, front end 17.7 s, character select 17.2 s, TA stage select 8.9 s (Flycast emulated time).
  Most time was per-file opens (ISO9660 directory walk), small fs_read calls, PAL8 twiddling on the SH4 and per-sfx loads.
- Fixes: packs (FPK1: one LZ4 pack per screen/level, CRC32 per entry, released at loading_end); textures pre-twiddled
  on the PC (FPTX fmt 2 → pvr_txr_load); 32-byte aligned 64 KB DMA reads; Lilac's banks kept resident across
  compatible transitions (CRC-checked); empty parallax layers skipped; mirrored left sprites drawn by UV flip.
- **KOS finding:** fs_iso9660's DMA path needs a 32-byte aligned destination; the < 32-byte tail goes through an
  unaligned buffer and can hang. All disc files are padded to 32 bytes.
