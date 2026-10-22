#!/usr/bin/env python3
"""Host gameplay regression tests on the recovered Dragon Valley 1 (frame 21).
Runs host/fpdc_host with scripted input and asserts on the per-frame trace.
usage: test_gameplay.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env=dict(os.environ, FP_LEVEL=os.environ.get("FP_LEVEL", "dv1"))).stdout
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
r = run('5 EN 100\n10 14 B\n', 60)
check('boost_tap_nothing', all(x['st'] != 4 for x in r) and abs(r[-1]['gsp']) < 0.01)
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
check('boost_energy_cycle', len(starts) == 2 and 278 <= starts[1] - starts[0] <= 320, 'charges at %s' % starts)
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
slope = [x for x in r if x['gnd'] == 1 and x['ang'] < -0.6]
check('slope_rotation', slope and all(abs(((x['rot'] + 180) % 360) - 180) > 20 for x in slope), '%d slope frames' % len(slope))
r = run('0 400 R\n', 260)
air = [x for x in r if x['gnd'] == 0]
steps = [abs(b['rot'] - a['rot']) for a, b in zip(air, air[1:]) if a['rot'] != 0]
check('air_rotation_eases', steps and max(steps) <= 4.01, 'max step %.2f' % (max(steps) if steps else -1))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
