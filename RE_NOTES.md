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
| 1584-1590 | sensor loops: wall collision, `loop_player01up` (air: Y += G, then up to 10 one-pixel steps out of a ceiling), `loop_player01down` (one-pixel push-out from the floor), edges, landing-angle detection (1590: N from `loop_player01detectangle` + `loop_toolsanglecalculator`; e.g. atan(4/7) = 29.74 deg) |
| 1591 | landing: if V = 1, L = 0, G >= 0 and 22 < N < 338: B = B - G * J * sin(N) |
| 1592 | landing: W = G, G = 0, L = 1, K = 1 |
| 1603 | ground angle detection (different span from 1590; e.g. atan(1/14) = 4.0856 deg on the DV1 start slope) |
| 1605-1607 | angle smoothing: O = fmod(O + (N - O) * max(0.175, abs(B) / D), 360) within 45 deg, else O = N |
| 1610 | detach (no floor under the sensors): N = floor(N), G = -B sin(N), B = B cos(N), N = O = L = 0 |
| 1621-1636 | ground acceleration / friction (constants are multiples of E) |
| 1644 / 1646 | slope force with N in (35, 325) deg and state != 6: B -= 0.38 F sin(N) if 0 < abs(B) < C, B -= F sin(N) if abs(B) >= C |
| 1647 | airborne: G += I |
| 1648 | airborne, -4 < G < 0, abs(B) >= 0.125: B *= 0.96875 (a value read from PlayerActionValuesB) |
| 1649 | airborne, G < 0, ceiling sensor: G = 0 |
| 1651 | grounded: G = 0 |
| 2015 | down-kick launch: B = 5 (facing), M = 0 |
| 3047 | jump from the ground (B, G from N; jump 7) |

**Sensors (RV positions relative to the movement object, airborne):** PlayerSensorTopBig_332 (11x11, hotspot 5,5) at (floor X, floor Y - 13); PlayerSensorTop_331 (9x9) at Y - 12; PlayerSensorBottomBig_334 (11x11) at Y + 13; also left/right/slope/main sensors (`loop_player01detectsensor*`). Ground contact is pixel overlap of these boxes with backgrounds or the **obstacle group** (QualifierList +0x218d8: bridge logs, the three yellow spring classes, ladder tops, LevelValleyPlatform1, LevelMovingPlatform2/3). So the landing reach is floor(feet) + 4. The angle routines (1590, 1603) and the push-out loops are **not ported yet**; they explain most remaining comparison differences (landing angle 29.74 vs our 36.87; start slope 4.0856 vs ~0.6; teleports into rock).

## Handlebar (gimmickhangingidlesensor_674; events 4057-4066)
4065: the top-big sensor overlaps the hang group (QualifierList +0x22040), G > 0, M != 9, controller B = 0 (Down not held): M = 41, X = sensor x, Y = sensor y + 32. 4062/4065 rewrite the position every frame; B becomes 0 the next frame while G keeps gaining gravity. Hang sensor image 50x15, hotspot (25, 0). Release and moving along the bar are in 4057-4064/4066 (with `Frame::test_ladder`): not read yet.

## Results screen (DV4 / frame 24; located, not ported)
Lists: LevelCardActClear1 +0x8bf0, LevelCardActClear2 +0x8c30, LevelCardBonus1-3 +0x8cb0/+0x8cf0/+0x8d30, TotalBonus +0x8d70, BossSerpentineHead +0x9070. Sequence events 4560-4589 + `loop_tally_19` + 5389/5395, driven by a controller timer (B, counts to 200): 140 HUD shifts up (`loop_gui_shiftup_14`), 150 music strings (`play_music`, `stop_music`) and counters, 180 clear-card animation, tally sub-states (A = 1, 2) with their own timers (120, 100), character voice lines (e.g. Milla's `yay` / `thatwasfun`), then (+150) records.dat save, Steam upload and the frame change. Loops are started by name through a hash table, so call graphs miss them: find them by string (`str_*` symbols) or by object-list offsets as above.
