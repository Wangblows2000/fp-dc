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
r = run(open(os.path.join(HERE, 'orig', 'hurt_turretus.txt')).read(), 200, env={'FP_SPAWN_X': '0', 'FP_OBJ_PRETICK': '2'})
hit = next((x for x in r if x['hp'] < 14), None)
check('turretus_pellet_hit', hit is not None and hit['f'] == 188   # stdout numbering (= CSV / trace-aligned row 189)
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
r = run('5 TP 16000 2280\n20 700 R\n', 520, env={'FP_LEVEL': 'dv4'})
st = [x.get('res', 0) for x in r]
f_on = next((x['f'] for x in r if x.get('res', 0) > 0), None); f_a2 = next((x['f'] for x in r if x.get('res', 0) == 3), None)
f_wipe = next((x['f'] for x in r if x.get('res', 0) >= 11), None)
check('dv4_end_results', f_on and f_a2 and f_wipe and f_a2 - f_on == 220 and f_wipe - f_a2 == 149 and r[-1]['mode'] == 0,
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

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
