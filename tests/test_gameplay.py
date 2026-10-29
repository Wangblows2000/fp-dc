#!/usr/bin/env python3
"""Host gameplay regression tests on the recovered Dragon Valley 1 (frame 21).
Runs host/fpdc_host with scripted input and asserts on the per-frame trace.
usage: test_gameplay.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_LEVEL': os.environ.get("FP_LEVEL", "dv1"), **(env or {})}).stdout
    os.unlink(name)
    rows = []
    for line in out.splitlines():
        t = line.split()
        if t and t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
    return rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# 1 spawn: player start found, grounded on recovered terrain, stable
r = run('', 120)
check('spawn_grounded_stable', all(x['gnd'] == 1 for x in r[2:]) and len({x['y'] for x in r[2:]}) == 1,
      'x=%.0f y=%.0f' % (r[-1]['x'], r[-1]['y']))

# 2 acceleration / cap
r = run('0 200 R\n', 200)
check('accelerates_to_top', max(x['gsp'] for x in r) >= 5.9, 'max gsp %.2f' % max(x['gsp'] for x in r))
check('speed_capped', max(abs(x['gsp']) for x in r) <= 14.0)

# 3 jump height from the start area (JUMP^2/2g ~ 96.6 px)
r = run('10 70 J\n', 100)
base = r[5]['y']; apex = min(x['y'] for x in r)
check('jump_height', 88 <= base - apex <= 104, '%.1f px' % (base - apex))
check('jump_lands_back', r[-1]['gnd'] == 1 and abs(r[-1]['y'] - base) <= 2)

# 4 variable jump
r = run('10 14 J\n', 100)
check('variable_jump_lower', base - min(x['y'] for x in r) < 70, '%.1f px' % (base - min(x['y'] for x in r)))

# 5 running route over recovered terrain: rides the quarter-pipe without stalling
script = open(os.path.join(HERE, 'scripts', 'dv1_right.txt')).read()
r = run(script, 600)
curve = [x for x in r if 900 <= x['x'] <= 962 and x['gnd'] == 1]
# original (run_right trace) rides this quarter-pipe to ~82 deg before leaving the lip; entry speed
# depends on the route, so only the ride itself is asserted
check('quarter_pipe_ridden', bool(curve) and max(x['ang'] for x in curve) > 1.4,
      'max ang %.2f' % (max(x['ang'] for x in curve) if curve else -1))
check('passes_curve', max(x['x'] for x in r) > 1100, 'max x %.0f' % max(x['x'] for x in r))
check('no_fall_out', all(x['y'] < 1864 for x in r))

# 6 combat (RV turretus_attack: a ground attack next to the first Turretus destroys it; whether a plain
#   jump damages enemies is not verified, so the old jump-kill check was replaced)
r6 = run('5 TP 1190 420\n60 64 X\n', 140)
check('turretus_destroyed', r6[-1]['k'] >= 1, 'kills %d' % r6[-1]['k'])

# 7 moves
# RV: hold special -> 30 f charge -> auto-fire at speed 12 for 33 f (a tap does nothing)
r = run('5 EN 100\n10 90 B\n', 110)
b = [x for x in r if x['st'] == 4]
check('dragon_boost', 30 <= len(b) <= 36 and max(abs(x['gsp']) for x in b) >= 11.9, '%d boost frames' % len(b))
# RV: the meter starts EMPTY at stage start and fills at 0.4/frame (250 frames): no boost before that
r = run('10 300 B\n', 320)
first = next((i for i, x in enumerate(r) if x['st'] == 3), None)
check('energy_starts_empty', r[0]['en'] < 1 and first is not None and first >= 240, 'en0 %.1f charge at %s' % (r[0]['en'], first))
# RE (no release cancel in the M5 events) + original trace boost_tap: a 4-frame tap still charges and fires
r = run('5 EN 100\n10 14 B\n', 80)
check('boost_tap_fires', any(x['st'] == 4 for x in r) and max(abs(x['gsp']) for x in r) >= 11.9)
r = run('10 12 X\n', 90)
check('attack_and_recover', any(x['st'] == 1 for x in r) and r[-1]['st'] == 0)
r = run('10 20 J\n30 45 J\n', 140)
check('dragon_cyclone', any(x['st'] == 2 for x in r) and r[-1]['gnd'] == 1)

# RV upper bound (boost_upright: re-hurt 61 frames after a hit): hits every frame from frame 50 land
# at 50 and again once invulnerability (60) has run out
r = run('5 HP 14\n' + ''.join('%d HURT 10\n' % f for f in range(50, 140)), 150)
hits = [i for i in range(1, len(r)) if r[i]['hp'] < r[i - 1]['hp']]
check('invuln_window', len(hits) >= 2 and 55 <= hits[1] - hits[0] <= 61, 'hits at %s' % hits[:3])

# springs (RV spring_yellow_up / _upleft / _right traces in DV1)
r = run('5 TP 3984 1300\n', 60)
vys = [x['vy'] for x in r]
check('spring_up_vy', abs(min(vys) + 10.75) < 0.01, 'min vy %.2f' % min(vys))
r = run('5 TP 5615 1000\n', 40)
l = next((x for x in r if x['vy'] < -8), None)
check('spring_upleft_vel', l is not None and abs(l['vx'] + 9) < 0.01 and abs(l['vy'] + 8.75) < 0.01, str(l and (l['vx'], l['vy'])))
r = run('5 TP 6130 440\n60 120 L\n', 130)
g = [x['gsp'] for x in r]
check('spring_side_gsp', max(g) > 9.6 and max(g) <= 10.0, 'max gsp %.3f' % max(g))

# RE Turretus + pellet + damage (events 4766-4774, PinballMovement, event 2414; hurt_turretus trace):
# with the original's start phase the pellet hits on frame 189 with B = -2.2, G = -4.2
# (DV fixes: the pellet's mask against the sensor box at (int X, int Y); RV hurt_turretus.snd / dv_fixes tp_hurt: shot 28 at
# frame 186, hit 17 25 at 189, the same frame numbers as the host's sounds. The whole 16x16 box with the unfloored X hit at 188,
# which matched the shift-2 Lilac comparison only because the hit came one frame early.)
r = run(open(os.path.join(HERE, 'orig', 'hurt_turretus.txt')).read(), 200, env={'FP_SPAWN_X': '0', 'FP_OBJ_PRETICK': '2'})
hit = next((x for x in r if x['hp'] < 14), None)
check('turretus_pellet_hit', hit is not None and hit['f'] == 189   # stdout numbering = the original's .snd frame of the hit
       and abs(hit['vx'] + 2.2) < 1e-6 and abs(hit['vy'] + 4.2) < 1e-6,
      hit and 'f%d vx %.3f vy %.3f' % (hit['f'], hit['vx'], hit['vy']))
# RE camera (MasterCamera_408): after a teleport the view snaps (centre within X .. X+16); running fast the
# look-ahead G reaches 64, so Lilac sits 64 px left of the centre (213)
r = run('5 TP 1190 420\n', 30)
check('camera_snaps', 1190 - 213 <= r[-1]['cx'] <= 1190 + 16 - 213, 'cx %.1f' % r[-1]['cx'])
r = run('10 400 R\n', 400)
sx = [x['x'] - x['cx'] for x in r if 100 <= x['f'] <= 180]      # grounded at top speed (G saturated)
check('camera_lookahead', sx and max(abs(v - (213 - 64)) for v in sx) < 1.5, 'screen x %.1f..%.1f' % (min(sx), max(sx)))

# RE handlebar grab (event 4065): the diagonal spring at 5600,1088 throws Lilac into the hang sensor at
# 5361,936; she hangs at X 5361, reference Y 968 (feet 982)
r = run('5 TP 5615 1000\n', 90)
h = [x for x in r if abs(x['x'] - 5361) < 0.01 and abs(x['y'] - 982) < 0.01]
check('hang_grab', len(h) >= 20, '%d hanging frames' % len(h))
# RE fall cap (event 1577): G is clamped to 15 before gravity, so it tops out at 15.25
r = run('5 TP 3000 -30\n', 120)   # 913 px clear drop
check('max_fall_15_25', abs(max(x['vy'] for x in r) - 15.25) < 1e-6, 'max vy %.3f' % max(x['vy'] for x in r))

# RE act change (events 4135/5045 trigger at X > width - 50; fade 3310/2905; next frame 3578; walk-in
# 4526/5047): DV1 -> DV2 without results, petals/energy/timer carried, Lilac enters at start - 50 = -29
r = run('5 TP 14600 980\n7 HP 5\n10 400 R\n', 300)
trig = next((x for x in r if x['x'] > 14766), None)
new = next((x for x in r if x.get('act') == 2), None)
check('act_change_dv1_dv2', trig and new and new['mode'] == 1 and abs(new['x'] + 29) < 0.01 and new['hp'] == 5 and new['en'] > 20
      and 50 <= new['f'] - trig['f'] <= 58, new and 'after %d f, x %.1f hp %d en %.1f' % (new['f'] - trig['f'], new['x'], new['hp'], new['en']))
walk = [x for x in r if x.get('act') == 2][:33]
check('act_walk_in_b3', walk and all(abs(x['gsp'] - 3) < 1e-9 or abs(x['vx'] - 3) < 1e-9 for x in walk[1:32]), 'walk-in frames %d' % len(walk))
# DV4 path B (5381): the results overlay starts at X > width - 50, tally state A = 2 at C = 220 with no crystals,
# the wipe after D = 150 (RV dv4_results: entry f72, A = 2 at f292, wipe f441, next frame f493)
# session 16: the run earns "Dragon Valley Speedrunner" (43), so the achievement pop-up (frame 9) waits for A after the wipe
r = run('5 TP 16000 2280\n20 700 R\n540 541 J\n', 600, env={'FP_LEVEL': 'dv4'})
st = [x.get('res', 0) for x in r]
f_on = next((x['f'] for x in r if x.get('res', 0) > 0), None); f_a2 = next((x['f'] for x in r if x.get('res', 0) == 3), None)
f_wipe = next((x['f'] for x in r if x.get('res', 0) >= 11), None)
check('dv4_end_results', f_on and f_a2 and f_wipe and f_a2 - f_on == 220 and f_wipe - f_a2 == 149 and r[-1]['mode'] == 1 and r[-1].get('act') == 1,   # Classic: on to Relic Maze 1 (frame 8 routing, session 14)
      'on %s A2 %s wipe %s mode %d' % (f_on, f_a2, f_wipe, r[-1]['mode']))

# 8 recovered animations are driven by state (idle -> run anim index changes, frames advance)
r = run('0 200 R\n', 200)
check('anims_change', r[0]['an'] != r[-1]['an'] and len({x['fr'] for x in r[100:]}) > 3)

# 9 Section-2 fixes (2026-10-22, all rules runtime-verified, see RUNTIME_MAP "Moves")
ST_CYCLONE, ST_BOOST, ST_DKWIND, ST_DIVEKICK, ST_CHARGE = 2, 4, 8, 9, 3
# A grounded cyclone: run, Down + attack -> cyclone on the ground for 79 frames, speed kept
r = run('0 140 R\n80 200 D\n85 88 X\n', 200)
cyc = [x for x in r if x['st'] == ST_CYCLONE]
check('grounded_cyclone', len(cyc) in range(78, 81) and all(x['gnd'] == 1 for x in cyc[:30]) and cyc[0]['gsp'] > 4,
      '%d frames' % len(cyc))
# A standstill Down + attack = crouch kick, not a cyclone
r = run('0 100 D\n40 43 X\n', 80)
check('crouch_kick_not_cyclone', not any(x['st'] == ST_CYCLONE for x in r))
# B aerial cyclone -> down-kick cancel, and down-kick (kick phase) -> cyclone, one cyclone per airtime
r = run('5 TP 68.715 440\n60 75 J\n80 84 J\n90 200 D\n91 95 X\n', 200)
check('cyclone_to_downkick', any(x['st'] == ST_DKWIND for x in r) and any(x['st'] == ST_DIVEKICK for x in r))
r = run('5 TP 68.715 440\n60 75 J\n78 200 D\n79 83 X\n98 100 J\n', 160)
i = next((k for k, x in enumerate(r) if x['st'] == ST_CYCLONE), None)
check('downkick_to_cyclone', i is not None and abs(r[i]['vx'] - 5) < 0.01 and abs(r[i]['vy']) < 0.3)
r = run('5 TP 68.715 440\n60 75 J\n80 84 J\n90 200 D\n91 95 X\n112 116 J\n', 200)
check('one_cyclone_per_airtime', sum(1 for a, b in zip(r, r[1:]) if b['st'] == ST_CYCLONE and a['st'] != ST_CYCLONE) == 1)
# C boost needs a full meter; drains to 0 over the boost, refills 0.4/frame (250 frames)
r = run('10 EN 100\n20 600 B\n', 600)
starts = [k for k, (a, b) in enumerate(zip(r, r[1:])) if b['st'] == ST_CHARGE and a['st'] != ST_CHARGE]
check('boost_energy_cycle', len(starts) == 2 and 290 <= starts[1] - starts[0] <= 315, 'charges at %s' % starts)   # original: 301 (boost_cycle)
r = run('10 EN 60\n20 100 B\n', 100)
check('boost_needs_full_meter', not any(x['st'] in (ST_CHARGE, ST_BOOST) for x in r))
# D left boost: sprite angle 180 (direction of motion), right boost 0
r = run('10 EN 100\n20 TP 700 430\n60 62 L\n70 200 B\n70 200 L\n', 120)
b = [x for x in r if x['st'] == ST_BOOST and x['gnd'] == 0]
r2 = run('10 EN 100\n20 TP 700 430\n60 75 J\n70 200 B\n75 200 L\n', 130)
b2 = [x for x in r2 if x['st'] == ST_BOOST and x['gnd'] == 0]
check('boost_left_orientation', b2 and all(abs(x['rot'] - 180) < 0.5 and x['face'] == -1 for x in b2), 'rot %s' % (b2[0]['rot'] if b2 else None))
# E slope rotation: ground angle follows the surface, air eases back to 0 by 4 deg/frame
r = run('0 400 R\n', 140)
slope = [x for x in r if x['gnd'] == 1 and 200 < x['ang'] < 326]   # N in degrees (down-slope steeper than ~34 deg)
check('slope_rotation', slope and all(abs(((x['rot'] + 180) % 360) - 180) > 20 for x in slope), '%d slope frames' % len(slope))
r = run('0 400 R\n', 260)
air = [x for x in r if x['gnd'] == 0]
steps = [abs(b['rot'] - a['rot']) for a, b in zip(air, air[1:]) if a['rot'] != 0]
check('air_rotation_eases', steps and max(steps) <= 4.01, 'max step %.2f' % (max(steps) if steps else -1))
# RV (cyclone traces): the cyclone drains 0.25 energy per frame (100 -> 80.5 over its 79 frames)
r = run('5 EN 100\n60 70 J\n80 140 J\n', 170)
cyc = [x['en'] for x in r if x['st'] == 2]
check('cyclone_energy_drain', cyc and abs(min(cyc) - 80.5) < 0.3, 'min %.2f' % (min(cyc) if cyc else -1))

# port change (momentum): uppercut and down-kick keep a faster run speed; FP_ORIG_MOVES=1 restores the original
ST_UPPERCUT = 7
up = '0 80 R\n80 81 UX\n'
for env, name in (({}, 'port'), ({'FP_ORIG_MOVES': '1'}, 'orig')):
    r = run(up, 130, env)
    i = next((k for k in range(81, len(r)) if r[k]['vy'] < -6), None)
    vx = r[i]['vx'] if i is not None else None
    if name == 'port': check('uppercut_keeps_momentum', vx is not None and vx > 3.0, 'launch vx %s' % vx)
    else: check('uppercut_original_vx2', vx is not None and abs(vx - 2) < 0.07, 'launch vx %s' % vx)
dk = '0 90 R\n60 62 J\n70 71 DX\n'
# (the original's wind-up drags vx 4.8 -> 3.84; the port keeps it)
for env, name in (({}, 'port'), ({'FP_ORIG_MOVES': '1'}, 'orig')):
    r = run(dk, 110, env)
    i = next((k for k in range(71, len(r)) if r[k]['st'] == ST_DIVEKICK), None)
    vx = r[i]['vx'] if i is not None else None
    w = [x['vx'] for x in r if x['st'] == ST_DKWIND]
    if name == 'port': check('downkick_windup_keeps_momentum', w and min(w) >= w[0] - 0.01 and vx is not None and vx >= 5, 'windup vx %.3f..%.3f kick %s' % (w[0], min(w), vx))
    else: check('downkick_original_vx5', vx is not None and abs(vx - 5) < 0.01, 'kick vx %s' % vx)

# combat (RE spec_combat.md; original trace build/orig_traces/combat_t1.csv, script tests/orig_extra_combat_t1.txt):
# a standing whip 41 px from the first Turretus: the shield hits on the next frame (A 8 -> 4, C 5, invert flash),
# hits again 5 frames later when K and C are back to 0 (A 0), then the corpse flies with X 3, Y -3 (+0.25 per
# frame) spinning 8 deg/frame and explodes when its rotated image touches the ground (orig: frame 172 at 1287,415)
def run_en(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_LEVEL': 'dv1', 'FP_DBG_ENEMY': '1', **env}).stdout
    os.unlink(name)
    return {int(t[1]): {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2)} for t in (l.split() for l in out.splitlines()) if t and t[0] == 'en'}
en = run_en(open(os.path.join(HERE, 'orig_extra_combat_t1.txt')).read(), 200,
            {'FP_ORIG_MOVES': '1', 'FP_SPAWN_X': '68.715', 'FP_SPAWN_Y': '450', 'FP_SPAWN_L': '1', 'FP_OBJ_PRETICK': '2'})
hits = [f for f in sorted(en) if f - 1 in en and en[f]['A'] < en[f - 1]['A']]
check('whip_two_hits_5_frames_apart', len(hits) == 2 and hits[1] - hits[0] == 5 and en[hits[0]]['A'] == 4, str(hits))
check('hit_flash_3_frames', hits and sum(1 for f in range(hits[0], hits[0] + 5) if en[f]['inv'] == 1) == 3)
f0 = hits[1] if len(hits) > 1 else None
check('defeat_flyaway_spin', f0 and en[f0]['Y'] == -2.75 and en[f0]['ang'] == 8 and en[f0 + 10]['x'] - en[f0]['x'] == 30,
      str(en.get(f0)))
last = max((f for f in en if en[f]['fl'] == 40), default=None)   # fl printed in hex: 40 = defeated flag   # corpse destroyed in the next frame at (x+3, y+1)
check('corpse_explodes_on_ground_at_1287_415', last == 170 and en[last]['x'] + 3 == 1287 and en[last]['y'] + 1 == 415, str(last))
# a plain jump into an enemy never damages it (RE: no shield, Z = 0): jump into the first Dragonfly's path

# half corkscrew M45 (RE spec_traversal §2; original trace build/orig_traces/cork_dv1.csv): DV1 pair 1 at x 2328..2856.
# Original: state 45 from f99 (x 2328.1, y 666.9), apex y 566.9 (100 px), plateau, back on the ground at f187-188 (y 666.4),
# B rises 0.1/frame to 6.085 while right is held at top speed
r = run(open(os.path.join(HERE, 'orig_extra_cork_dv1.txt')).read(), 260, {'FP_ORIG_MOVES': '1', 'FP_SPAWN_X': '2150', 'FP_SPAWN_Y': '665', 'FP_SPAWN_L': '1'})
ck = [x for x in r if x['st'] == 15]
check('cork_rides_state_45', len(ck) > 60, '%d frames' % len(ck))
if ck:
    check('cork_rise_100px', abs((ck[0]['y'] - min(x['y'] for x in ck)) - 100) < 2, 'rise %.1f' % (ck[0]['y'] - min(x['y'] for x in ck)))
    check('cork_speed_to_6', abs(max(x['gsp'] if x['gnd'] else x['vx'] for x in ck) - 6.085) < 0.01)
    check('cork_ends_after_far_end', ck[-1]['x'] > 2850 and ck[-1]['x'] < 2870, 'end x %.1f' % ck[-1]['x'])

# roll M6 + spiral tunnel (spec_roll_gimmicks; original traces build/orig_traces/extra_spiral*_dv1.csv). The host's
# TP lands 4 frames before the original's, so host frame f = original frame f + 4.
def at(rows, f): return next(x for x in rows if x['f'] == f)
r = run(open(os.path.join(HERE, 'orig_extra_spiralb_dv1.txt')).read(), 380, {'FP_LEVEL': 'dv1'})
# original: bottom entry, the climb slows (B -0.15 per 4 frames ...) and reverses, Lilac drops out of the bottom at f318
# (X 8233.38, B 3.94066 -> floor 3), rocks break with OTHERS.K = 5, the slow roll stands up at f372 (M0, X 8356.01)
o = [(150 - 4, 8164.78, -3.33033), (246 - 4, 8271.4, 1.46967), (322 - 4, 8246.38, 3.0), (358 - 4, 8328.72, 3.0)]
check('spiral_bottom_ride_matches_orig', all(abs(at(r, f)['x'] - X) < 0.01 and abs((at(r, f)['gsp'] if at(r, f)['gnd'] else at(r, f)['vx']) - B) < 0.001 for f, X, B in o),
      str([(at(r, f)['x'], at(r, f)['gsp'], at(r, f)['vx']) for f, X, B in o]))
check('roll_stands_up_when_slow', at(r, 376 - 4)['st'] == 0 and abs(at(r, 376 - 4)['x'] - 8357.02) < 0.01)
r = run(open(os.path.join(HERE, 'orig_extra_spiral_dv1.txt')).read(), 380, {'FP_LEVEL': 'dv1'})
ride = [x for x in r if x['st'] == 16 and x['gnd'] == 0 and 8150 < x['x'] < 8300 and x['y'] < 640]
check('spiral_top_ride', len(ride) > 80 and max(x['vx'] for x in ride) == 12.0, '%d frames' % len(ride))
check('spiral_exit_breaks_rocks_keeps_speed', any(x['x'] > 8500 and (x['gsp'] if x['gnd'] else x['vx']) > 10.8 for x in r))
# DV3 ball tunnel (original trace extra_balltunnel_dv3: host f = orig f - 2): enters M6 at f87, stands up at f150, pushes at 943
r = run(open(os.path.join(HERE, 'orig_extra_balltunnel_dv3.txt')).read(), 320, {'FP_LEVEL': 'dv3'})
o = [(96 - 2, 784.087, 2.3318), (126 - 2, 841.516, 1.95702), (146 - 2, 875.012, 1.3237), (166 - 2, 919.731, 3.10555)]
check('ball_tunnel_matches_orig', all(abs(at(r, f)['x'] - X) < 0.01 and abs(at(r, f)['gsp'] - B) < 0.001 for f, X, B in o),
      str([(at(r, f)['x'], at(r, f)['gsp']) for f, X, B in o]))
# DV2 pushable block (original trace extra_pblock_dv2: host f = orig f - 2): block and Lilac step 1 px per frame from
# the frame after the push starts, the block stops at x 2846 (wall), Lilac at 2809.99; released: block 2845, Lilac 2807.99
r = run(open(os.path.join(HERE, 'orig_extra_pblock_dv2.txt')).read(), 330, {'FP_LEVEL': 'dv2'})
check('pushable_block_matches_orig', abs(at(r, 79 - 2)['x'] - 2766.99) < 0.01 and abs(at(r, 100 - 2)['x'] - 2787.99) < 0.01 and
      abs(at(r, 200 - 2)['x'] - 2809.99) < 0.01 and abs(at(r, 325 - 2)['x'] - 2807.99) < 0.01)

# cards (ObjectCodec_430) persist in the save: take DV1's card at (2090, 1085), clear the act (TP near the end), one save
# write at the act change, and the next boot reads the total back
import tempfile as _tf
_sv = os.path.join(_tf.mkdtemp(), 'sv.bin')
with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f: f.write('60 TP 2090 1070\n100 TP 14700 990\n105 900 R\n'); _n = f.name
_p = subprocess.run([BIN, DATA, '900', _n], capture_output=True, text=True, env={**os.environ, 'FP_LEVEL': 'dv1', 'FP_SAVE': _sv, 'FP_TRACE_SND': '1'})
_writes = [l for l in _p.stdout.splitlines() if l.startswith('save ') and '@' in l]
_p2 = subprocess.run([BIN, DATA, '2', '-'], capture_output=True, text=True, env={**os.environ, 'FP_LEVEL': 'dv1', 'FP_SAVE': _sv})
check('card_saved_once_at_act_change', len(_writes) == 1, str(_writes))
check('card_total_persists_across_boot', 'save cards 1' in _p2.stdout, [l for l in _p2.stdout.splitlines() if 'save cards' in l][:1].__str__())

# shields (spec_shields): a wood monitor broken at f70 gives the shield 64 icon frames later; on Normal it takes two hits
# (absorb, break) with no hurt; the third hit hurts (host MON debug command places the monitor)
r = run('60 MON 151\n70 73 X\n200 HURT 10\n300 HURT 10\n400 HURT 10\n', 460, {'FP_LEVEL': 'dv2'})
check('shield_given_after_icon', at(r, 141)['sh'] == 1 and at(r, 140)['sh'] == 0 and at(r, 100)['sh'] == 0)
check('shield_absorbs_then_breaks', at(r, 250)['sh'] == 1 and at(r, 250)['shc'] == 0 and at(r, 350)['sh'] == 0 and at(r, 350)['hp'] == 14 and at(r, 450)['hp'] < 14)

# water (spec_water, original DV3 traces): entry G x0.2 (1.2) then gravity 0.15 to the cap 2; walk 9E then E = 0.056875;
# hold jump: -6, swim held at -3, leap out at -6.5, re-entry 1.4 then swim -1.0 (host frames = original - 2)
cp = lambda x, k: x['gsp'] if x['gnd'] else x['vx']
r = run('60 TP 1560 2640\n200 260 L\n360 480 J\n', 470, {'FP_LEVEL': 'dv3'})
check('water_entry_slows_fall', abs(at(r, 83)['vy'] - 1.2) < 1e-6 and abs(at(r, 84)['vy'] - 1.35) < 1e-6 and at(r, 92)['vy'] == 2.0)
check('water_walk_accel', abs(at(r, 200)['gsp'] + 0.51187) < 1e-3 and abs(at(r, 204)['gsp'] + 2.057) < 1e-3)
check('water_jump_swim_leap', at(r, 360)['vy'] == -6.0 and at(r, 384)['vy'] == -3.0 and at(r, 404)['vy'] == -6.5 and abs(at(r, 458)['vy'] - 1.4) < 1e-6 and at(r, 459)['vy'] == -1.0)
r = run('60 TP 1493 2860\n', 3800, {'FP_LEVEL': 'dv3'})
check('water_drowns_after_62s', at(r, 3600)['air'] == 28 and at(r, 3790)['st'] == 6)

# DV4 boss defeat (spec_dv4_boss_events 5547-5578; FP_SN_AUTOKILL deals the hits): the head rises up the shaft and explodes on
# the surface, the cinematic camera stays there (normal camera group off), Lilac rises as the boost ball (anim 7) at
# 10 px/frame, resumes physics at y < 1230 in M6, lands on the surface, then the results run and Relic Maze follows (Classic)
r = run(open(os.path.join(HERE, 'play', 'dv4_to_boss.txt')).read() + '4560 4561 J\n', 4700, {'FP_LEVEL': 'dv4', 'FP_SN_AUTOKILL': '1'})   # A: the achievement pop-up (session 16)
rise = [x for x in r if x['gnd'] == 0 and x['y'] - 14 < 1300 and x['x'] > 12500 and x['x'] < 13300]
land = [x for x in r if x['gnd'] == 1 and abs(x['y'] - 14 - 1260) < 8 and x['x'] > 12500 and x['x'] < 13300]
view = [x for x in r if x['res'] >= 1]
check('dv4_exit_rises_to_surface', bool(rise) and bool(land), '%d air / %d grounded frames on the surface' % (len(rise), len(land)))
check('dv4_exit_camera_on_surface', bool(land) and land[0]['cy'] < 1300 and land[0]['y'] - land[0]['cy'] > 0 and land[0]['y'] - land[0]['cy'] < 240, str(land[0]['cy'] if land else None))
check('dv4_exit_results_then_rm1', bool(view) and max(x['res'] for x in r) >= 3 and r[-1]['mode'] == 1 and r[-1].get('act') == 1)   # Classic routing: DV -> RM (session 14)

# Break sounds (RV original traces mon / inv / spiral_dv1; RE Frames::event_func_2623): a monitor plays 307 + 12, a rock 13.
# None of them may play 80/81/82, which are Lilac's own hurt set (RV: hurt = 25 + one of 80/81/82).
def sids(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_TRACE_SID': '1', 'FP_TRACE_SND': '1', **env}).stdout
    os.unlink(name)
    return [l.split()[1] for l in out.splitlines() if l.startswith('sid ') or l.startswith('music ')]
m = sids('5 60 R\n10 13 X\n30 33 X\n', 120, {'FP_LEVEL': 'dv1', 'FP_SPAWN_X': '6150', 'FP_SPAWN_Y': '830'})
i = m.index('bgm/m_invincibility.adp') if 'bgm/m_invincibility.adp' in m else -1
check('invincibility_monitor_shatter_sound', i >= 2 and m[i - 2:i] == ['308', '316'] and '25' not in m[:i], ' '.join(m))   # RE 2624 (D = 0): 308 + 316 (session 12; was 307 + 12 INFERRED)
r = sids(open(os.path.join(HERE, 'orig_extra_spiral_dv1.txt')).read(), 380, {'FP_LEVEL': 'dv1'})
check('rock_break_sound_not_hurt', '13' in r and not {'80', '81', '82'} & set(r), ' '.join(r))

# Pellets (session 18; RE col_4_20 / col_7_20 / col_20_20 play Media slot 136/8 = 17 with explosion02, RE 2414 plays 25 +
# sound_hurt<n> only; RV hurt_turretus 17 25 81, c_claw_hit 17 25 114, m_super_turret 17 25 181, a miss 17 alone): a hit plays
# 25 + 17 + the character's own hurt voice once, a miss 17 only; never one of Lilac's 80-83 for Carol / Milla, nor twice
def sid_frames(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_TRACE_SID': '1', **env}).stdout
    os.unlink(name)
    res, pend = {}, []
    for l in out.splitlines():
        t = l.split()
        if t and t[0] in ('sid', 'voice'): pend.append(t[1])
        elif t and t[0] == 'f':
            if pend: res[int(float(t[1]))] = pend
            pend = []
    return res
HURTV = {0: {'lilac_hurt0', 'lilac_hurt1', 'lilac_hurt2', 'lilac_hurt3'}, 2: {'114', '115', '118', '119'}, 4: {'179', '180', '181', '182'}}
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    hit = sid_frames('5 TP 68.715 440\n100 TP 1190 420\n', 230, {'FP_LEVEL': 'dv1', 'FP_CHAR': str(ch)})
    miss = sid_frames('5 TP 68.715 440\n100 TP 1000 420\n', 230, {'FP_LEVEL': 'dv1', 'FP_CHAR': str(ch)})
    hf = [s for f, s in sorted(hit.items()) if '25' in s]
    check('pellet_hit_sounds_' + nm, len(hf) == 1 and hf[0].count('17') == 1 and len(set(hf[0]) & HURTV[ch]) == 1 and
          len(hf[0]) == 3 and not {'80', '81', '82', '83'} & set(sum(hit.values(), [])), str(sorted(hit.items())))
    ms = sum(miss.values(), [])
    check('pellet_miss_sounds_' + nm, '28' in ms and '17' in ms and not {'25', '80', '81', '82', '83'} & set(ms) and
          not HURTV[ch] & set(ms), str(sorted(miss.items())))

# DV2 pressure plate (5182-5186 + 4971): the door sinks 8 px/frame while pressed (RV plate3: 1016 -> 1144, top stays in
# view), and a pushable block resting over the plate presses it (4971; reach over the pit = port deviation D-036)
def objs_at(script, f, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as fh:
        fh.write(script); name = fh.name
    out = subprocess.run([BIN, DATA, str(f + 1), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_DBG_OBJS': str(f), **env}).stdout
    os.unlink(name)
    return {int(l.split()[3]): (float(l.split()[5]), float(l.split()[7])) for l in out.splitlines() if l.startswith('obj ')}
d = [objs_at('58 TP 2900 1120\n', f, {'FP_LEVEL': 'dv2'}).get(96, (0, 0))[1] for f in (74, 75, 76, 92)]
check('dv2_door_sinks_when_pressed', d == [1016, 1024, 1032, 1144], str(d))
push = '1 140 L\n200 215 L\n215 222 LJ\n222 280 L\n320 566 R\n596 597 L\n'
o = objs_at(push, 800, {'FP_LEVEL': 'dv2', 'FP_SPAWN_X': '2895', 'FP_SPAWN_Y': '520'})
check('dv2_block_presses_plate', o.get(96, (0, 0))[1] == 1144 and o.get(98, (0, 0))[1] == 1137 and o.get(98, (0, 0))[0] > 2895, str(o.get(96)) + ' ' + str(o.get(98)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
