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
  - Lilac, Carol and Milla rows. Since session 11 Lilac, Carol and Milla are playable (D-040).
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

## Session 10: Carol (specs build/re/spec_carol_moves.md, spec_carol_combat.md, spec_carol_bike.md; port src/characters/carol.inc, carol_bike.inc)
Carol is MV.A = 2 on foot and MV.A = 3 on the motorbike. Everything below is RE from the executable and was then
checked against original traces (53 scripts in tests/orig_carol, CSVs in build/orig_traces/carol, compared by
tools/orig/cmp_carol.py) unless it is marked INFERRED. The user's PC saves unlock Carol in the original; they were used
only to run the original from /home/claude/fprun_user (the port never reads them).

**Trace alignment (harness rules, RV):** port row f is compared with original row f + 2 (port frame f = original
logic frame f + 1). Animation frames: port row f against original row f + 3, because the port ticks the animation at
the end of its frame. Original sound frame F = port frame F − 1. Script `a b KEY` holds the key for frames a..b−1, so
`100 100 X` sends nothing (use ≥ 2 frames).

**Identity and constants (RE 1555/1543, 1560-1563):**

| | on foot | on the bike |
|---|---|---|
| C top run speed | 5 | 8 (10 with nitro) |
| D speed cap | 12 (15 under water) | 15 |
| E acceleration | 0.07375 | 0.06 with a direction held, 0.00575 idle, 0.36 during nitro (RV; the RE table alone gave 0.00575) |
| I gravity | 0.25 | 0.3 |
| jump | −7, release cap −3, sound 8 | same, sound 232 |
| energy | +0.25/frame outside M44 (1663) | same |

**OTHERS.Y drives Carol's animations.** The port keeps a copy (`p->cY`) and selects animations from it (2426-2470):
force_speed persists until restore_speed (2490 runs while Y is not in {1, 2, 8, 15, 26, 40}); force_frame(6) is
clamped to the animation's frame count; on the bike the bike skin's speeds are restored except M32 (100) and the pad
hop (35). Carol's tail and her on-foot skin (PSC) keep animating while hidden on the bike, and PSC's frame times the
claw, kick and wall events (`carol_skin_frame`).

**Moves (RE, all traced):**
- Pounce (her air move): B = ±max(|B|, 5) toward the facing side, Y = 24, M = 0, sounds 154 (+ voice 133 with a
  10 s cooldown); while Y = 24 |B| ≥ 6 every frame. From the crouch: B = 8Q, G = −3. Re-pounce after a cancel is
  allowed (c_pounce_chain).
- Wild Claw combo (M20, skin anims 25/26/29, sounds 410-412 + 50 % voice), big slash after a pounce/roll/ball jump
  (shield dir 30/14, sound 414). Shields: counter 200 at creation, the first update advances two frames, lifetime =
  number of images (RV c_claw_hit2).
- Wild Kick (M44, special held, energy ≥ 20): energy −1.5/frame while L ≤ 20, shields on skin frames 2/5/7, kick E
  reads 2, 10-frame recovery.
- Roll (M6) from Down at |B| ≥ 2; roll jump keeps the speed rules (S = 1); S = 0 only in the roll and in a kick started
  from the roll (1862/1863 set S = 1).
- Wall cling M47 (1771/1793/1905, 2173-2183): 2177 lets go when the toward-wall direction is NOT held (that branch
  needs A = 2, so the bike only lets go at the floor or at the wall end); grab sound 33 on PSC frame 1, once; wall
  jump B = −3Q, G = −7, sound 117.
- M1 converts Y 0-2/4-7/40 to 3; landing sound 161 only on the M1 landing (1807); skid Y = 6 from the 2nd skid frame,
  sound 6 (shared with Lilac).

**Bike (RE + RV):**
- Fuel can (ObjectFuel, OT_FUEL 158): touching it on foot mounts at once (sound 126); the pick-up runs after the
  movement and the animation events. A can restores the bike's 3 hit points.
- Engine: samples 159 (ch 9) and 160 (ch 10) looping; ch10 = 22000 + 7000·rev Hz, ch9 vol max(0, 100 − 20·rev),
  ch10 vol clamp(60·rev); starts the frame after A = 3. Rev on the wall follows G before the climb. Skid loops 162.
- Air jump M32 (sounds 232 + 409): 2167 caps G = min(G, 1 − BSK.B) while BSK.B > 2; PAV_B.O is reset only in the
  grounded M0 group (1741).
- Nitro: special + a direction on the ground: B ≥ 10, sound 387, G timer decremented before the end test, no clamp.
- Ram (2738/2739): |B| > 5 into an enemy, enemy A −= |0.25B| + 1, sound 24, cooldown K = 3.
- Dismount (crouch + jump, 1828) leaves a parked bike (OT_PBIKE 159) that collides with its whole image box; falling
  onto it remounts (2109, sound 126, keeps its damage). Third hit wrecks it (1285: C ≤ 1 tested before the decrement):
  explosion, sound 29, 4 debris.
- Item pick-ups while riding use the position before the move.
- Exhaust (2129): EffectsDust01 every 8th frame standing / 4th moving, not in water or nitro. Its constructor (RE
  create_effectsdust01_369) sets blend alpha 0x74, a random direction of 32 (one cross_seed step) and BallMovement
  speed 9 (1.125 px/frame). The port drew these puffs from the wrong atlas (invisible) until session 10's fix.

**Jump pads (ObjectJumppad, M48 2184-2196, 4931-4949):** the first M48 frame still moves with the jump; in flight G
gets gravity; on a pad G = 0.25 and a 2 px push-out; PSC.L is both the kick counter and the pad count; the launch keeps
Y = 23 (M40 sets 40 on the next frame). Lilac on a pad: it goes dark and two crystals pop out (4944, 3275).

**Sound groups (port, Flycast RV):** 1812 KB of samples at boot overflowed AICA RAM (ARM CPUUndefinedException). A
shared set loads at boot and one character group at level load: Lilac 9 21 31 174 176 288-290 314 396 405 + 16 voices;
Carol 33 61 113-122 126 128 133 139-141 154 159-162 232 387 409-414. Largest free block after loading: 389 KB (Lilac),
351 KB (Carol).

**Still INFERRED or not ported:** stopping sample 413 at the kick end; claw pitch; DV4 block pad 5356; the 2443 Y12
timer (33 frames); bored-idle variants; spin-shield angle; debris direction; monitor smash on the bike through
attack_kind; a few bike-skin animations without original data. Random voice/pitch choices differ (the port's random
seed is not synced with the original).

## Session 11: Milla (specs build/re/spec_milla_moves.md, spec_milla_combat.md; port src/characters/milla.inc)
Milla is MV.A = 4 (character select G1 4, voice 186). She uses Lilac's movement table (1543: C 5, D 15, E 0.07375,
I 0.25, jump -7, cut -3), no landing sound (1657 turns group 0x21a62 off), energy +0.5/frame outside M20 with the meter
hidden. Checked against 41 original traces (tests/orig_milla, tools/orig/cmp_milla.py); the tracer runs her with CHAR=4
(PlayerSkinMilla_461 0x20bdcf0, tail 0x20bdd90, PlayerReflect_463 0x20bde30, PlayerDig_464 0x20bded0). RE = spec,
RV = trace.
- **Puppy Float M22 (RE + RV):** jump held, G >= 0, MILLA.E < 100 (1782 airborne M0 / 1811 M1 / 1915 springs). Each
  float frame: OTHERS.Y 14 (anim 18, speed 100 - max(0, 80 - 2N)), L = 0, MV.V = 0 (no landing), E += 0.9 (+20 and
  sound 244 on the 2nd frame), G = max(G - 0.5, -2) after gravity: she rises 2 px/frame. Ends on E >= 100 (89 frames
  from an empty meter) or jump released (anim 24 from frame 6). The meter refills on the ground, springs, M41/M46.
  Flap sound 170 every 0.1 s of frame time from N > 15 (the tracer's frame times make the original's cadence
  irregular: the port plays it every 6 frames, INFERRED).
- **No double jump, no crouch jump, no crouch attack; look-up jump allowed (RE).**
- **The cube, PlayerReflect_463 (RE + RV):** one reused object, state A: 0 parked, 1 held, 2 flying, 3 burst, 4 shield,
  5 shield burst / beam, 6 cool-down.
  - Attack pressed in M0 -> M20 (one frame) -> M30 with MILLA.F = 2; attack held: G counts to 40 (sound 422 at G 1),
    the cube appears at p + 42 (A = 1, anim 10, sound 415, RV m_attack_hold). Releasing before G = 40 cancels.
  - Held at MILLA's image action point - 15 along the skin angle (int coordinates). Carry top speed C = 4; jumping
    from M30 gives M1 with F kept (RV m_super_air).
  - Throw (attack pressed): X = cos(O)(5Q + B), Y = -sin(O)(5Q + B), gravity 0.25 (cap 15), sound 417; dropped when
    hurt (3Q + B). It bursts (A = 3, sound 416, 21 frames, invisible 40x40 hitbox) on background, solid / obstacle
    objects, enemies or monitors. The flying cube itself does no damage.
  - Burst damage (H - 1) + max(DIFF, 1) = 4 on Normal, knock-back 0.75 x the cube's velocity, no hit-stop (RV
    m_turr_throw: Turretus 8 -> 4 -> defeated).
- **Barrier Shield M31 (RE + RV):** special held with the cube parked and F <= 0. Arc anim 11, sound 420; Up / Down aim
  +-45; Left / Right turn her; no D-pad acceleration, shared ground friction + 0.2/frame (RV m_shield_walk /
  m_shield_decay). **It neither blocks nor reflects projectiles in v1.21.5 (RV m_shield_pellet: a pellet hurts her
  through it).**
- **Shield Burst (RE + RV):** releasing special: A = 5, anim 8 (cone), sound 418, hits while F < 3 for 4 on Normal
  (RV m_turr_burst: two hits, defeated), then A = 6 and M31 ends when anim 8 has finished; the shield is usable again
  11 frames later.
- **Super Shield / beam / recoil (RE + RV):** special held while the cube is held (2213, any state): sound 421, anim 2;
  release: the beam (anim 5, 160x64, sound 419, 6 per hit on Normal every frame of the beam gated by the enemy's C),
  camera shake 4, **recoil B -= 7Q** (Q = facing on the last shield frame), aimed up G = max(G + 5, 5), aimed down
  G = min(G - 5, -5) and L = 0. The aim is read on the release frame (RV m_super_down: Down released with special ->
  neutral shot). Recoil decays by the shared friction + 0.2 on the ground, air drag in the air; walls stop it (RV
  m_wall_recoil: B = +7 into the wall, X unchanged, no clipping). Matched frame by frame: ground, aimed up / down,
  air neutral / down, into a wall.
- **Animation details (RV):** a finished one-shot keeps its id and last frame ("playing" is false); the frame counter
  carries over an animation change and a finishing update leaves it unchanged; Chowdren shows the nearest existing
  direction (anim 0 has no direction 30: the cube-holding idle shows 31).
- **Rocks / monitors (RE + RV):** the cube burst and the beam break BreakableRock(2) (sound 13, RV m_rock_throw /
  m_rock_beam); the plain burst does not. The flying cube and the bursts open monitors.
- **Dig M27 (RE + RV in Aqua Tunnel):** crouch + attack with MILLA.D = 1, which only the dig-zone events of Aqua Tunnel
  1 / Relic Maze 1 set: **no digging in Dragon Valley** (crouch + attack does nothing there in the original). Trace
  m_dig_at1 (FRAME=16) recorded the mash timing (A 20, -1/frame, +20 per press, B > 120 done, sounds 184/185).
  The port implements the dig state; host tests enter it with the debug command `<f> DIGZONE 1`. Dig spots and their
  rewards are level objects of stages that are not ported.
- **Voices (RE loop_sound_* Milla branches):** hurt 179-182, die 178, dig 186/202-204, idle 205-207, select 186.
- **HUD (RE):** GUIScoreTimeRings direction 4 (B box: cube / charging / lifting / dig icons, 2197-2201), energy fill
  hidden; life icon GUIIcon direction 4.
- **Not ported (RE, documented):** lifting and throwing monitors / fuel cans (2202-2205, 2230, 2233, 2246, 3135), the
  shield-hit sound 315 path (2412), idle voice variants, alternative palettes (1566-1569).

## Session 12: Aqua Tunnel (specs build/re/spec_at_flow.md, spec_at_enemies.md, spec_at_gimmicks_a.md, spec_at_gimmicks_b.md, spec_ironworm.md, spec_chomper.md, spec_chompersnake.md, at_inventory.md; port src/game/aqua.c, aqua_gim.c, aqua_boss.c)
Aqua Tunnel = original frame index 16-19 (handle_frame_17..20_events, frame_17..20.json, port at1..at4; loop and
collision names use the index, e.g. `_16` = AT1). Traces: `FRAME=16..19 CHAR=4`, scripts tests/orig_at, CSVs
build/orig_traces/at, compared by tools/orig/cmp_at.py (port row f vs original row f + 2; world objects get PRERUN 122
frames of the card's object events; GT = GV[3] phase from the trace's gv3). run_trace.sh clears the AT checkpoint
globals 4-6 before a restart (`CP=keep` keeps them): stale checkpoints had polluted the first traces.
- **Flow (RE spec_at_flow + RV):** checkpoints g4/g5/g6 (AT1 7600,1100 / 11300,988; AT2 5720,270 and the Chomper's
  6144,1101 (g4 10) / 7648,1124 (g4 11); AT4 3264,200); walk-in after an act change from X = -32, Y = start + 16
  (AT2 B = 0, AT3/AT4 B = 3); AT1 fresh start at start + (16, 16) falling; act ends AT1 X > 12462, AT2 X > 11534,
  AT3 X > 376 with Y < 380 (top exit), AT4 through the boss results; per-act camera bounds (E = sign.x + 213), boss locks
  (AT1 11368..12479 x 600..903, AT2 6032..6863 x 976..1231, AT4 6464..7039 x 8..319) and the mode-2 return; water
  lines AT1/AT2 1200, AT4 1480, none in AT3. Music: AT1 m_stage0; AT2 m_stage0 -> m_midboss (Chomper trigger 4410)
  -> m_stage0b (4452, g4 >= 10); AT3 m_stage0b; AT4 m_stage0b -> m_boss2 (4601); no command on act changes.
- **Enemies (RE + RV a1_slime, a1_current):** fireflies (patrol still runs on the defeat frame), slimes (animation
  counter 50 at the act start; a finishing animation update leaves the counter unchanged), slime bursts, whisps,
  breakable aqua bricks, monitor bomb (arm, beeps, 120-frame fuse, explosion 20 to the player, -15 to enemies, bricks,
  chains) and crate. Monitor break sounds by D (2623-2626, D-050).
- **Gimmicks A (RE + RV):** water jets (4136; 4141/4142 shake M12), current (4137-4140 -> roll B 15), jet maker, jelly
  springs (charge > 20 frames, launch G = -9 - 0.1 A), red springs (side springs: 3-frame input lock in the air).
- **Gimmicks B (RE + RV, aqua_gim.c):** aqua brick platforms (obstacle box), collapsing platform (rotated mask about
  its bottom-left hotspot, 0 -> -40 degrees, B += 0.02, shake 5, sound 51), AT1 pressure plates -> floor doors (slide
  224 px left, non-solid from the first frame) and the arena gate (momentary, +-4 px/frame), AT2 Milla block (cube +
  grounded) -> doors, wall plates (RightBig + push M11 or Carol's cling M47) -> every pipe turns a quarter (0.03/frame
  ramp, sounds 49/62/51; AT3 auto-return below y 2900), spinning beam (+1 degree/frame; TopBig touching while standing
  on it kills, ignoring invulnerability), AT2 crusher pillars (collision copy on the play layer, drawn copy behind;
  360-frame cos bob of 96 px, dip and carry), AT3 sliding slabs (3 px/frame, 40-frame stops, push / carry), axels
  (AT1/AT4 makers every 60 GT frames, J +-2, one-way and anti-crush flags 4088-4092; AT3 bobbing), swings (pendulum
  E -= F cos D, D += E; carry by last frame's integer motion; jump or walk-off adds the tangential speed), slime floor
  (scroll, 5 damage, 99-frame green tint), rafts (264 px at 4 px/frame, carry; boss raft 1784 px).
- **Placement correction (RV, D-048):** the AT gimmick block matches the traces only after the port's player update
  (a4_raft, a1_plates, a3_crusher, a2_beam, a2_crusher).
- **Iron Worm (RE + RV a1_worm, exact):** the head is link 7 of the 6-segment chain: x = Bx + 6 E sin(theta),
  y = By + 7 E cos(theta) - 14, F0 = -140 (the spec read 5 / 6 / -120); the chain has run 62 frames of the card when
  play starts. Wake at PLAYER.x > 11804, 61-frame rumble, sound 242, 8 rocks on GT % 3, emergence E -> 24, idle 101 /
  161 frames, spit (anim 7, ~22 acids on GT % 4), thrash at HP < 25, defeat (140 frames of explosions, swing up, slam,
  thrown head lands; camera released, trigger flag kept). Host test: the left slam puts the head on the arena plate,
  which holds the gate open (4095, spec U1).
- **Chomper (RE + RV a2_chomper and a2_swallow, exact):** enters from x 7020, swims 6320..6720, mouth shut 181 then
  open 181 / shut 121, lure on a horizontal spring (the vertical one is inert, RE), impulses J += 4X + 2Y from the
  shared hits, bite when the lure enters the open mouth moving right (HP 30, 10 per bite), cube bite (thrown cube into
  the open mouth), swallow M43 49 frames then spat out (B -8, G -2, 10 damage, tint), defeat at +120 / +170, bricks
  swap, g4 = 10, water-jet maker on.
- **Chomper Snake (RE + RV a4_snake: head exact, player differs from frame 1044):** raft ride 446 frames to x 6736,
  lunge / dive / home cycle (~652 frames), bubbles at I 30/60/90 (the third drops a bomb), acid bursts (they splash only
  once on the play layer, RV), damage by a thrown bomb (-10) or a thrown cube in the open mouth (-5), 148-frame blink,
  defeat timeline (explosions, sink, white fade at +160, results at +340 on the raft, raft exit and wipe at x 7024).
  RV a4_snake_bomb: at frame 1048 Milla catches the falling bomb (M30) - monitor lifting (2202) is needed there.
- **Milla in AT (RE 799 / 623-625, D-046):** Classic / Adventure / Continue start at AT1; the Time Attack AT icon
  replaces DV for her only. Dig zones and rewards of spec_milla_moves §6 are live in AT1.

## Session 14: Relic Maze (specs build/re/spec_rm_flow.md, spec_rm_enemies.md, spec_rm_gimmicks_a.md, spec_rm_gimmicks_b.md, spec_rm_golems.md, spec_rm_claw.md, spec_rm_mantalith.md, rm_inventory.md; port src/game/relic.c, relic_gima.c, relic_gimb.c, relic_boss.c, rm_claw.c, rm_mantalith.c)
Frame indices 24-28 (frame_25..29.json, handle_frame_25..29), stage s = 2, cards 11-20, title card "Relic Maze /
STAGE 2". Labels as everywhere: RE read from code, RV original trace, INFERRED reasoned.

- **Routing (RE, spec_rm_flow 1.1):** frame 8 sends Classic DV4 (ML.Q 3) and AT4 (ML.Q 11) to RM1 with g4 = 0, so
  Lilac / Carol arrive from Dragon Valley and Milla from Aqua Tunnel. RM1 right edge: MV.Y <= 260 or >= 420 → RM2,
  the middle band → RM3 (blocked by barriers until ML 0x2 and 0x4, set in RM2). RM2 left edge (X < 80, B < 0) → RM1 at
  its right edge; RM2 death / restart → RM1 (loop_death_25). RM3 X > W + 20 → RM4; RM4 gateway (wall plate) → RM5;
  RM5 Mantalith → results → frame 8. The graph is the same for every character (no G1 / MV.A test on routing).
- **Per character (RE 1.6):** RM2 6013 / 6014 remove the other character's crystal set; RM3 6198 takes bike-Carol off
  the bike; 6374 destroys the hand-car unless Milla (**the mine-cart ride is Milla's**); 6202 starts the Claw sequence
  only for MV.A != 4 (**the Claw is Lilac / Carol only**, INFERRED from the group chain whose only entry is 6202); the
  golems and the Mantalith are for everyone; RM5 results voices per character (6589-6592).
- **Music (RE 3):** m_stage2 from RM1 (kept across act changes: no command at g4 = -1); RM3 switches to m_stage2b at
  g4 >= 10, m_chase / m_outdoor in the Claw sequence; RM4 / RM5 m_stage2b; RM5 m_trouble (Adventure only), m_boss
  for the Mantalith.
- **Enemies (RE, spec_rm_enemies):** iris (hunts, dives; contact only while diving), slug + fire sprites (contact never
  hurts), hopper + spores, extractor head / base + rocks / sparks, fire spitter + flames / shell, sentinel + spikes,
  pots (take every shared hit). Shared rules through combat.c (enemy_type, F_ARMOURED 0x20, defeated 0x40).
- **Gimmicks acts 1-2 (RE, spec_rm_gimmicks_a):** relic panels and pieces, RM2 flip panels, fireball spawners / turrets
  (BallMovement speed / 8 px a frame), hanging bars (path platforms as hang handles), hanging switches → moving floors,
  elevators, rolling platform + wheel, spin ball, basement plates (a third plate rule: radio buttons), axle tunnel,
  clock, maze gates and break-out, RM2 loops and layer switching, RM1 wall plate → switch column, ruins, dig hole.
- **Gimmicks acts 3-5 (RE, spec_rm_gimmicks_b):** hanging switch → moving pillars, rising swing, pickaxe trap, spinny
  cup, elevator, Milla's hand-car and track switches, rolling cave and drill platforms, ruins, RM4 gateway exit, RM5
  light stones, wall plates, spike ceiling, crush boxes, chair; Carol's RM jump pads (6092-6095).
- **Bosses:** RM2 Eye Golem and RM5 Bone Golem (spec_rm_golems: blocks fall, eye / core assembly, step counter, flame
  and shot attacks, shockwaves 10 damage, defeat); RM3 Claw (spec_rm_claw: shrine scene, corridor destruction, caravan
  ride, hover / shoot / lunge, engine wreck); RM5 Kingdom Stone scene and Mantalith (spec_rm_mantalith: skeleton rig of
  16 beads, swipes, slam, bites, combo and high jumps, five head defeats, boss results path, post-results scene).
- **INFERRED in the port:** the camera release after the Eye Golem (D-066); Mantalith death clock, limb-droop limits,
  knee angles, arm amplitudes, debris speeds, ruin offsets, face-crack image, voice timings, Lilac's post-results boost
  (jump + auto-run) and Carol's wall pounce; the RM5 room warp fade for a right exit (B > 0); Spade's identity.
- **UNKNOWN (trace to settle):** shockwave ageing during the Bone Golem: spec_rm_golems reads B as rising only in the
  Mantalith group (7055, so golem waves never expire), spec_rm_mantalith reads 7112 as a second, ungrouped +1. The port
  follows the golem spec (waves leave the level). Others: gimmicks_b U1-U10, claw U1-U8, records[2][36] condition.
- **Not ported:** the RM5 Adventure dialog cutscene (D-064), achievement / unlock records (D-065), HUD slide and timer
  pause during the RM5 scene, Mantalith sounds 147 / 214 / 215 (D-063), monitor breaking by the Mantalith's arms.
  **Carol / Milla result voices are 1-second placeholders** (the voice files available here are placeholders).

## Session 16: rendering and RM3 findings
- **Backdrops ignore the image hotspot (RE):** Backdrop::draw passes x + hotspot_x, y + hotspot_y to
  FrameObject::draw_image, which subtracts the hotspot again (chowdren.dis 0x202a3c0 / 0xa33650); collision follows
  the same box. Images with a hotspot used as backdrops: caravantruck3 (154,120), caravanplatform8, shafts (56,48), AT
  atcolumn (32,0), teeter-totter axle (12,12), others in frames 26/27. RV: the RM3 truck matches an original
  screenshot after the importer change.
- **Thrown items (INFERRED):** created on the play layer like their spawners; the port had left them on layer 0.
- **PVR sampling (INFERRED, from the hardware report):** an edge exactly on a sample point may take the neighbouring
  texel; offsets of 0.25 px keep edges off both pixel-corner and pixel-centre sample points.
- **Earth shield (RE spec_shields 2.2):** magnetized items (flag 2) home in with any shield or none (2542); the port
  only moves them while the earth shield is held, and the user's report of a persisting pull is not reproduced.

## Session 15: bug fixes, gallery (spec build/re/fe/spec_gallery.md; port src/game/fe_gallery.inc)
- **Invisible-at-creation classes (RE):** constructors that call set_visible(0) include SwitchColumnFront1_1157 (RM1
  column collision box; image 43 is a solid 176×224 green placeholder), MinibossBarrier_969 (gate / arena wall; the
  railings are the visual), SpiralTunnelFront, SpinningLogMask, and many markers / sensors (list in the session-15 scan of
  build/re/create_*.c). The port marks the ported ones with registry flag OD_HIDDEN.
- **guiicon_346 directions (RE anims):** 0 Lilac (311), 1 image 112 (not Carol), 2 and 3 Carol on foot / on the bike
  (395), 4 Milla (507), 5 image 5116. The direction is MV.A.
- **Difficulty (RE 2414):** on Casual (global 12 = 3) the hurt event never runs: no damage. Global 12 is shared by the
  options and the file screen and is not reloaded from records during a session.
- **RM2 entrance gates:** 5922 / 5923 create the gate at (116, 336) / (116, 1424) at fade A ≥ 32; the walk-in speed
  remains UNKNOWN (spec_rm_flow 1.3). Original trace attempt (session 15): restarting frame 25 directly does not
  reproduce the walk-in (player placed at x 155, controllable from frame 3), and the tracer cannot follow a frame change
  from RM1, so it is still unverified. The port creates the gate once the player is past it (INFERRED).
- **Gallery (RE + RV, spec_gallery):** one screen with CARDS (10×10, one row per stage, each card unlocks one music
  track, voice / sound or artwork; records x20 0 / 1 face down / 2 opened), ACHIEVEMENTS (56, drawn at state ≥ 2) and
  DATA (delete records / cards / achievements on leave). Unlock rule: each card by its own id; 14 type-3 cards (44, 63,
  82, 89, 90, 92-100) need achievement 1, 2 or 3. Card Collector (achievements 23-32): on entry, a complete row sets 1,
  saves and shows the pop-up, which sets 2. User's records.dat (RV): all 100 cards taken, 95 opened, 55/56 achievements.
  INFERRED in the port: type-3 tracks loop, DATA panel cursor, dimmed header alpha, pop-up as an overlay (frame 9 not
  ported), save on leave only when something changed. UNKNOWN items in spec_gallery 9.

## Session 16: achievements (spec build/re/spec_achievements.md; port: RUNTIME_MAP "Session 16")
- **Implemented (RE conditions from the spec; host-tested, not traced in the original unless marked):**
  - 10-17 boss finishes, DV4 (5581-5589) / AT4 (4663-4671) / RM5 (7120-7128) at the finish moment: Lilac M21 (10), M26 (11),
    any M5 (12, no A test), M20 with a big-slash shield dir 14 / 30 (13), M44 (14), MV.A 3 (15), cube A 3 (16), cube A 5 with
    anim 5 (17). The player state is read on the finish frame (RE); whether a boost kill still reads M5 there is UNKNOWN
    (trace `dv4_finish_boost` advised).
  - 18 Switch Skipper (DV2 5098 / 5099): **INFERRED reading** (spec 4.1): zone (door x + 32, door x + 128), MV.Y below the
    door's top, G4 < 2, plate not pressed, within 10 frames of entering; layer offsets taken as 0, no character test (RE).
    Host: a teleport past the closed door earns it, the plate / block route through the opened door does not. Not traced.
  - 19 Claw Bypass (6272), 33 Rolling Stones (5453-5460), 34 Push Block KO (5955), 35 All Bone, No Brain (7117, after the
    7116 decrement), 38 Treasure Hunter (4189, AT1 / RM1 spots), 42 Milla Unlocked! (7118, any character / mode).
  - 36 Preemptive Strike I (6900): the port's arm hitboxes do take hits during the intro (L 61, the fight group is already
    on: Lilac's attacks lower Hitbox2 there, host), so the check is live; reaching A <= 0 on both arms in that window by
    play is not demonstrated (host test uses the cheat).
  - 23 / 24 / 25 Card Collector and 43 / 44 / 45 speedrunners at results D = 149 (5580 / 7119 / 4662, 5579 / 7115 / 4661):
    the port commits this run's cards first (it saves them at D = 150). Thresholds <= 300000 / 480000 / 300000 ms of the
    stage time (host: 300000 ms earns, 300016 does not). Any mode incl. Time Attack, any character, no difficulty test.
  - Pop-up: after the results wipe, before the next stage / menu (RE frame 8 -> 9, RV it waits for input). The port writes
    the records once after the last page (the original saves on each page: INFERRED deviation, same data); its layout and
    black background are INFERRED (frame 9's scene is not ported).
- **Unresolved / out of scope (need unported stages or the ending):** 1-3 (FD4 results per character), 4-6 (FD4 results,
  file totals), 7-9 (Shang Mu targets), 20 / 21 / 41 (Fortune Night), 22 (Boost Perfectionist, frame index 60), 26-32
  (other stages' Card Collector; the gallery entry already awards them once those cards exist), 37 (Prince Dail), 39
  (Spade ledge), 40 (Freefaller), 46-56 (other stages' speedrunners, thresholds in the spec 2.2), y57 (hidden).
  Achievements earned mid-stage and then left by pause QUIT / game over stay at 1 until the next stage end or the gallery
  announces them (the original's frame 8 check would too; the quit route is not traced).

## PC save files (reference for the VMU save; the port does not read or ship them)
`file1..8.sav` and `records.dat` are Clickteam ArrayObject files (RV, parsed from the user's copies):
`"CNC ARRAY\0"`, u16 2, u16 0, i32 X, Y, Z, i32 flags (5 = numeric + base 1), then X·Y·Z int32 (x fastest).
- `file<n>.sav`: X 3, Y 99, Z 1 (1218 bytes). Column x1 holds the file fields read by the hub (spec_hub_menu_file §6.4):
  y1 lives, y4 crystals, y5 stage progress (StageIDList index), y6 play time in ms (> 0 = file exists), y7 deaths,
  y8 character (0 Lilac, 2 Carol, 4 Milla in these files), y9 crystal counter, y12 mode (0 Adventure, 1 Classic).
  Column x3 holds blocks of 5 rows from y11 on: time in ms, a count, and a small number per stage (INFERRED meaning:
  best time, crystals, deaths per stage).
- `records.dat`: X 20, Y 100, Z 1 (8030 bytes): options and records (y35 + 1 = default difficulty; x2 y42 decides the
  opening cutscene).
- Session 13 (RE build/re/spec_records_saves.md, RV-file checks in place): column x3 holds **one block per stage s at
  rows 6+5s, 7+5s, 8+5s** (time ms, crystals, deaths of the last clear; Aqua Tunnel = s 0 at y6, so the earlier "from
  y11" was Dragon Valley's block); the file totals y4 / y6 / y7 are their sums (RE 954; exact in all five files).
  records.dat x(3+J) y(2s+1) / y(2s+2) = Time Attack best time ms / best crystals per character group J (Lilac 0,
  Spade 1, Carol 2, Milla 3, Torque 4); x1 y(40+J) = furthest completed stage (the stage-select padlocks); x20 = the
  100 cards. The port's save v3 mirrors this (see "Session 13").

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

## Session 13: bug fixes, reusable objects and levels, save v3
- **Milla's hang (RE 2461, RV m_hang_dv1):** OTHERS.Y 41 selects skin animation 15 (her hang, with the tail). The port
  showed her idle because her A_HANG entry is a0 (her table maps every A_ name to her own skin through OTHERS.Y).
- **Springs keep MV.Q (RV spring_yellow_upleft, m_hang_dv1):** a diagonal spring does not turn the player.
- **Results backdrop (RE):** the results images (ActClear 1 / 3 panels, bars, digits) were imported only for frame 24
  (DV4). Every frame that places LevelCardActClear1 now gets them (AT4 shows its panels; DV4 unchanged).
- **Title cards (RE build/re/spec_stage_cards.md):** LevelCardIntro2 (name) and LevelCardIntro3 ("STAGE n") pick their
  frame per stage: name AT 3, DV 0, RM 1, FN 2, SB 4, JC 5, TB 6, PL 8, TH 7, BG 10, FD 11-14; "STAGE n" AT 0, DV 0,
  RM 1, FN 2, SB 3, JC 4, TB 5 (6 for Carol / Milla), PL 6, TH 5, BG 7, FD 8 (ROUND n). No act number is drawn. The
  importer used frame 0 of both for every level; it now takes them from STAGE_CARDS (import_frame.py) per level.
- **Time Attack preview (RE 612/613 + RV screenshots):** the preview window is masked by solid quick backdrops
  (no image: size 0x4c / 0x50, colour 0xc4-0xc7, gradient 0xc8 → 0xcc-0xcf); the UI importer dropped them, so the
  neighbouring stages' strips showed (half AT with its padlock, half DV). They are imported as solid rects (UI kind 2).
  Strips narrower than the 272-px window repeat (INFERRED). The menus' checker layer wraps vertically (RV
  orig_tasel_s13 / orig_chsel_s13) and their bottom bar is semi-transparent (image alpha 135; record panel 87): UI
  images with one partial alpha are now drawn as an opaque part plus a translucent layer (UI kind 3).
- **DV4 boss counter:** the bottom-right hit tally was a port addition of session 10 (the original's GUIBoss bar is
  never visible, U-G1); it is removed. Boss logic, damage, defeat and results are unchanged (cmp_all, test_gameplay).
- **Object identity = class + handle (RE, frame JSON):** some class names are used by two different objects with their
  own handles, constructor values and events: SwitchDoor 770 (AT, slides) vs 997 / 1066 (DV, sinks), SwitchWallPlate
  831 (AT, pipes) vs 1029 (DV3, boulder), AquaBrickPlatform 731 vs 803, LevelCrusher1 808 (vertical) vs 847 (AT3
  horizontal slab), LevelAxelPlatform1/2 680/737 (made by makers) vs 845/846 (placed, AT3), GimmickSwingSpawner1 824
  (8 links) vs 848 (6). The importer gives each its own engine type (ENGINE_TYPES_ID); the runtime no longer tests the
  act to tell them apart.
- **Same object, different events (RE):** PressurePlate_769 is one object in DV2, DV3 and AT1, but DV presses it while
  the player stands on it every frame (4971-4973, + resting block / parked bike, click once 5183) and AT1 keeps it
  pressed while anything touches a plate (4093-4096, + the Iron Worm's head, click per press). That rule is level data
  (plate_rule), the plate itself shared (src/game/switches.c).
- **Aqua Tunnel cards (RE spec_at_flow 1.2):** ids 21-30, AT1 from 21, AT2 24, AT3 27, AT4 28 (numbering by creation
  order INFERRED as in DV). They were placed but never numbered or saved by the port; now they are (level data).
- **Saves (RE spec_records_saves):** one stage index s for records, files and progress (§1 table). Record rules 907 /
  908 have no mode test (story clears update the Time Attack table too), times count only over 10 s, crystals are the
  best ever independently of the time. 954 / 955 (story only) write the stage block, the last stage, the progress,
  lives, shield and the crystal counter; Time Attack never writes a file. Still UNKNOWN: records x8..x12, flag G31,
  whether ML.Z stops at the results (INFERRED: it does; the port saves the time frozen at the results).

## Reusable objects and levels (session 13; the rule for every new level)
**Rule:** every newly added level asset and gameplay object is evaluated for reuse, generalised where appropriate,
optimised through the established pipeline (ASSET_PIPELINE "Adding a level"), and configured so it stays present and
functional in the levels that place it. Concretely:
1. **Object = Fusion class + handle.** A class that is already ported works in a new level as soon as the level places
   it: tools/import_frame.py maps it to its engine type (ENGINE_TYPES, or ENGINE_TYPES_ID when the class name is shared
   by another handle) and exports its sprites with the level's banks (the ENS rows are taken when the class or one of
   its parents, ENS_WITH, is placed); src/game/objdefs.c holds its shared properties (qualifier groups, image-box
   collision, spawn sprite); its behaviour code runs on its type in every level. No code may test the level name or act
   number to decide how an object behaves.
2. **Per-level parameters are data** in src/game/levels.c (one row per act): water line, act count and order, music,
   walk-in, exit rule, edge results, checkpoints, boss camera locks, act-start item sets, camera bounds, start rule,
   Carol's jump pads, Milla's dig zones, card ids, switch rule, monitor hop, stage sound group, AT axel / pipe values.
   Fill them from the frame's own events (RE) when an object depends on them.
3. **Genuinely level-specific scripting** (a boss, an intro like AT2's current, a cinematic) stays in that stage's file
   and is attached through the level's stage index, never through string tests in shared code.
4. **Subsystems follow presence:** a pass runs only where its objects exist (objects_have / objects_have_from), so a
   level pays nothing for objects it does not place.
5. **Evidence:** a reused object in a new level is checked against an original trace of that level when its events
   differ (e.g. the plate rule); otherwise its existing traces stand.
OBJECTS.md (tools/object_catalog.py) lists every type, its classes, registry flags, implementation and placements.

## Session 17: Fortune Night flow (spec build/re/spec_fn_flow.md with spec_fn_bosses.md Y; port src/game/fn_flow.c, levels.c)
- RE as the spec; RV from the spec's probes (FN1 start, FN3 / FN4 / FN6 walk-ins, FN3 exit, FN5 checkpoint 3) matched by the
  host (tests/test_fn_flow.py). Original screenshots this session (FN1, FN5, FN6 starts) match the port's parallax layout.
- INFERRED in the port: 7716's "animation frame > 3" uses the port's animation clock (dash 6 frames after B 0, RV ~6);
  the NPC is removed 1000 px past the frame; MV.M = 0 at 7710 only ends the boost / charge states; FN6 death without an FN5
  checkpoint starts FN5 fresh; FN3 timer stop uses game_timer (the HUD shows the stopped value).
- Not ported here (boss module): FN1 UFO lock / release, FN3 boss chains and their music (7726 / 7958 / 8048), FN5 samurai
  camera D 336, the panther intro / chase (8370-8419, uses fn_flow_panther_view / player_bike_off), FN6 fight modes, post-boss
  scene and 8785 (results_begin_boss). FN4's 5839 (g4 0 -> 1) has no port path (g4 is never 0 at an FN4 start).

## Session 17: Fortune Night bosses (spec build/re/spec_fn_bosses.md A-E; port src/game/fn_boss.c)
Static RE only (spec Part Z: no original run of these fights); host-tested (tests/test_fn_bosses.py). Labels as everywhere.
- **FN1 UFO (A, every character):** armoured with C = 5 while G = 0 (7367); Player_326 on the CameraTriggerBoss column ->
  CAM.M 1 to the limit markers (6984 / 824 / 7559 / 1831); G 0-5 exactly as 7377-7403 (pendulum on D / F, bob, hatches,
  beam y-scale A to 12, 220 frames of L); the beam hurts 10 and cuts BreakableFloor1/2 (A >= 5 -> fn_gima_floor_hit, 13);
  release at G 4 or when the player is 300 px below (7369); no music. INFERRED: the armour is cleared once G != 0 (no event
  clears flag 5; the spec's summary reads armour only before the encounter).
- **Bulldozer (B, FN1 / FN2):** J cycle drive / raise / stop (boomerang at L 50) / slam, limiters, the door (1 HP, armoured
  while closed, one hit while open breaks it), boomerang through the broken door -100 / -200, defeat J 6-8 with 15
  explosions, the MonitorIcon blast, 10 static crystals, the wreck. **E = +1, not the spec's -1 (INFERRED):** with -1 the
  7433 formulas put wheel 1 at X + 38 (placed at X - 36), the exhaust in front and the arm swinging down when "raised";
  with +1 every part matches its placement and the claw rises. UNKNOWN until T-B1. A plain hit on the body also takes HP.
- **FN3 beam chase (C, everyone):** 7794 window / zones / floors at A >= 4 / lamppost are level data (level_def_t.fnb).
- **Serpentine (D, Lilac only via the flow's CK_FN3):** lock 7811, trigger 7812 (16 x 288), m_midboss 7726, the UFO as his
  ship at (9967, 8) with the beam wall (barriers moved there), states dash / shoot (3-laser fan, dir 3) / flame jump
  (MinibossFlame) / shield + mortar / missile (Hard) / stagger (30 D thresholds, beam wall, boost); defeat 7866 and the whole
  escape 7930-7960 (columns topple, crash at 13904, fadeout 600, walk back, m_stage3c, barriers at 13948, CAM.F 248).
  INFERRED: one state at a time (the groups are not shown to overlap), the dash's stop OR (passed the player by 80 / the
  arena edge / 90 frames), the beam's y-scale at 7819 (12), the probe rising out of a higher floor, his rise speed (4) and
  the dash to the ship, the 199-after-198 delay (60 frames), the auto-run as a pad press + B floor.
- **Samurai (E):** FN3 for Carol / Milla (armoured until 7815, CAM.M 1 between the moved markers), FN5 for everyone (8422
  g4 < 3, CAM.D 336 and MV.Y >= 336, no music). Rig 7962-7964, intro descends (E += G with G 8 -> 0: the spec's "rises" is
  read as the sign of E), hover, hand smash (floors under the hands break, Y -6), lasers, body charge (15 px/frame, wall
  stop 17 / Z 8), defeat 8040-8050 (25 explosions, wreck rises, gates +4 px/frame to A 48, Super Turretus at A 24 / 48,
  CAM.M 2, FN3 music fade 1500 + m_stage3c). **UNKNOWN U-E1 (attack selector, K timings): the port rotates charge /
  smash / lasers every 90 hover frames (INFERRED).** FN5's 8045 stop y: home + 16 (INFERRED). FN3 samurai music: 7726
  fires for it too (a UFO with G < 3 exists) - RE literal, UNKNOWN in the original (flow trace 10).
- **Not ported:** EffectWarning2's off-screen missile tracker (7889-7891), the HUD slides (7821 / 7868), the timer stop
  during the escape (0x2205a), the Adventure dialog / voice (7728-7791, 7955), the beam's rotation at G 3.
## Session 17: the Panther (spec build/re/spec_fn_bosses.md F / G / X; port src/game/fn_panther.c)
RE plus original traces (tools/orig/trace_orig.py, new `GBYTES=<addr,...>`: the frame's group bytes per frame -> `.grp`):
T-F1 FRAME=33 at (12000, 210) holding right (Lilac, Milla); T-G1 FRAME=34 Lilac idle, invulnerable; T-G2 the same with
SETV hitbox A = 0 repeatedly; T-G3 CHAR=2 / CHAR=4. Host-tested (tests/test_fn_panther.py). The spec file is in the
shared build/ (not in git): this section is the session's addendum to it (spec §G.12 to be pasted there).
- **U-G1 settled (RE 8557-8559 + RV T-G1)**, the chain evaluated once per frame (A = rib / tail index): ribs (8554) ->
  shoulder F at rib 0's action point; head x = K + amp sin(M - 20) sin I, y = L + amp sin(M - 20) cos I (amp 32 in A6,
  32 - 32 ShoulderF2.A in A5); back cannon at rib 0; hip F at rib 6; leg FR at shoulder F's action point, foot FR at leg
  FR's; tail k at hip - 24k (cos, sin)(30 sin(20k + J) - I); knee F at hip F's action point, leg FL at the knee's, foot FL
  at leg FL's; shoulder F2 at shoulder F; the rider at the head's action point (33, -25); front cannon / jaw = head +
  rot(B, C) (ctor B 56 C 48 / B 71 C 21); charge glow at the cannon's action point; hitbox on the jaw; then M, J -= N.
  RV check: leg FR predicted (170.6, 188.6), port (171, 189).
- **Laser (RE disasm 8581-8583):** set_x_scale(16.0) (the 50 px image -> 800 px), angle = cannon angle - 22.5 (float at
  0x21b69e8), hidden with x scale 1.0 at D 5. Renderer: new r_set_xscale (both renderers, the PVR queue keeps it).
  INFERRED: the hit test is an 800 x 24 segment (not the rotated mask), the additive blend.
- **8561-8566 run while 0x227c9 is OFF** (jne in the disasm): the body follows the player outside the rider phase;
  0x227c9 is the rider phase. The hits (8635-8645) stay on after the defeat (only 0x227ba).
- **RV T-G1 cadence:** plasma f117 (191, then 436 every 5 frames x12), rider f392-565 (193, then 194 x30 every 3 frames;
  anim 12 8 frames / 13 95 / 12 8), back cannon f977 (254, voice 258, 11 x 195), alternating. Floor: B +0.2 a frame to
  the top speed during the walk-in (CTRL.D held), -0.569 a frame to < 10 after the control returns, then 10.08 (X +0.08).
- **RV T-G2:** pools 60 / 60 / 70 / 80 / 90; exposed (A 1, C 30) 15 frames after the jaw starts opening; the laser after
  the first hit (D 3 100 frames, D 4 200); mode 2 at F 3; the jaw hidden at F 4; the partner's strikes ~4 HP every 6
  frames (137 x10); defeat F 7 -> white flash at +513, groups off +525, C 2 +612, results (g4 0) at **defeat + 883**.
- **RV T-G3:** Milla on the plane at X 520, Y 170-183; Carol like Lilac (top speed 12 during the walk-in).
- **RV T-F1:** trigger X ~12545; Ship2.B jumps to 270 (not Milla); 13 at B 280 / 320-360; K 11663 at B 360, +16 a frame,
  stops at ~12475 L 106 at B 440 (193); control back B 490 (Lilac M 5, B 12, sound 9); B 520 the run (N -> 15); the exit
  at X ~15485. Milla: K 13817 at once, Milla moved to K + 200. U-F1: no hurt event found or seen (the port: no damage).
- **U-G3 (assist) partly settled (RE 8668-8747):** offense list +0x10b30; I cooldown (8668 I > 2 -> I - 1; 8669 I = 2, not
  mode 2, cannon D not 3..5 -> I = 1); 8670 / 8671 arrival voices; 8680 the strike = ShieldInstaShield, 137, flag 16 (the
  shared Q20 hit); 8684 knocked by a projectile (25, anim 8, D = C = -4); 8687 / 8701 I = 600; subgroups 0x227c4 /
  0x227be / 0x227c1. Images: plane 2045; offense dir 0 Lilac, dir 1 Carol on the bike; defense Milla; projectile purple
  slashes. **Port INFERRED (reduced):** I starts at 1200 (RV first strike ~f1200), 10 strikes every 6 frames at -4 HP,
  knocked by shots, then 600; the defense / projectile helpers and the per-character helper choice are not ported.
- **INFERRED elsewhere:** the plasma aim (atan2; the decompiled offsets are garbled), the floor pieces' slot reuse, the
  mode-2 body x step, the hit flash images (dir 1), achievement 21's window (8 frames after a partner strike), Milla
  carried by the plane when within 3 px, the FN5 back-cannon / rapid images loaded with FN6 only.

## Session 17: Fortune Night lead notes
- json frame vs set_frame: fn1-fn6 are json frames 30-35 (build_level.sh) and the original's set_frame indices 29-34
  (goto_frame.sh, level rows' `.frame`).
- RV screenshots (original, set_frame 29 / 30 / 32, build/orig/fn_start_*.png): FN2's start shows parallax piece 5_1802
  (image 3612, the blue-roofed towers over water) behind the starting wall, confirming spec_fn_flow 4's FN2 table.

## Session 18: Fortune Night playtest fixes (FN5 arena floor, FN1 UFO armour, FN5 dance timing, rolling arches)
Original traces (set_frame 29 / 33, `flock` run of tools/orig/run_trace.sh; CSV / .xl / screenshots in
build/orig_traces/fn_s18/, scripts session1.sh / session2.sh there): arch_l, arch_r, arch_lf, arch_rf, arch_spr (FN1 arch at
pivot (1896, 416)), ufo_hit (FN1), disco_c0 / c2 / c4 and samu (FN5).
- **FN5 samurai arena floor (RE + RV).** The arena floor is 8 x 7 BreakableFloor1/2 at x 9920..10368, y 576..864 over the
  spikes at y 928, placed under handles **1996 / 1997** (ctors: the same object ids 0x39a / 0x39b = Frames +0xe6b0 / +0xe6f0
  as acts 1-3's 1766 / 1767, images 7662 / 8683; piece 1995 = id 0x399, image 8684). The importer only mapped 1766 / 1767,
  so FN5 had no floor (fn5_report "skipped classes: breakablefloor1 28, breakablefloor2 28"): the player dropped onto the
  spikes and could not climb back to the exit (the playtest report). A5 runs the samurai's floor breakers 8031 / 8032 (body
  charge, A3 events reused) and its own 8439 / 8440 / 8447 / 8448 (hands), and shatters flagged floors with 8343-8346 (A5
  copies of 7220 / 7221 with piece 1995). RV samu (Lilac idle, invulnerable): the floors exist and break progressively
  (56 -> 49 at f300, 39 at f600, 35 at f1100; she reaches the spikes after ~1100 frames). **Port (INFERRED smash / charge
  timings, U-E1) digs slower than RV** (idle: one row per ~600 frames); not changed here.
- **FN1 UFO armour (RE + RV).** Among the events that select MinibossUFO (Frames +0xe838) only 7367 writes flag 5, and only
  to set it (with C = 5) while G = 0; flag 5 is cleared only by 7441 (bulldozer door) and 7815 (FN3 samurai lock) - never for
  the UFO, and none of the Q20 hit events 2730-2767 clears it. RV ufo_hit: flag 0x20 and A 200 in every state G 0-5 (flag 1
  joins at the camera release). The session-17 port cleared the armour at G 1 (INFERRED), so 200 HP of hits made it fly away
  (2765) before the beam had cut the floor. Now armoured for the whole encounter: hits clang (sound 30) and set C = 5, which
  only pauses L and the beam growth (7387 / 7395).
- **FN5 dance trigger (RE + RV).** 8306-8308 test the frame of the object at Frames +0x47b0 (Lilac's PlayerSkinSash, the
  same object for every character) for "not anim 40 and frame > 10". RV disco_c0 / c2 / c4: Lilac, Carol and Milla all
  reach M 50 / skin anim 40 dir 8 at the same frame, 79 frames after landing (the sash idle: 12 frames at speed 14, frame 11
  at 78.6 frames), 29 frames after DiscoLight A = 64; OTHERS.Y = 50 the frame after. The port's 10 idle frames (INFERRED)
  became the sash idle clock. **The reported "no dance" did not reproduce on the host** (the dance starts for all three in
  every arrival tested: TP, walk-in, run + skid, jump, after FN4 -> FN5): emulator check needed.
- **Rolling arch entry (RE + RV).** 7225 / 7226 keep the arches with **A >= 175 / A <= 5** (the selection loops remove A < 175
  / A > 5); spec_fn_gimmicks_a §4 and the session-17 port had them inverted, so an arch resting at 180 / 0 never caught the
  player. RV arch_lf: Lilac sprung up from the spring at the tower's left side holding right is caught at the ring's left
  foot (frame of 7225: X 1750 Y 416 B 5 N 90; next: X 1754 Y 411 B 4.797 N 85.9, arch A 178.2), runs round the ring's upper
  half (top X 1893 44 frames later, A ~90) and down the far side to N 270 (82 frames after the catch) while the arch follows
  her (7229); A = 0 and the base not solid again when she leaves it. RV arch_l / arch_r: running across the tower top under
  the resting arch is not caught (both sensors on it). The other arch rules (7229-7234) read as in the spec (checked).
- Collision semantics left as they were: the arch is a box object; the port tests its rotated mask (Chowdren: rotated
  bounding box, INFERRED); both agree at rest (0 / 180), where the entry tests run.
