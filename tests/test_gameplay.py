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
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True).stdout
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

# 6 combat: spin jump destroys the first Turretus on this route
check('turretus_destroyed', r[-1]['k'] >= 1, 'kills %d' % r[-1]['k'])

# 7 moves
# RV: hold special -> 30 f charge -> auto-fire at speed 12 for 33 f (a tap does nothing)
r = run('10 90 B\n', 110)
b = [x for x in r if x['st'] == 4]
check('dragon_boost', 30 <= len(b) <= 36 and max(abs(x['gsp']) for x in b) >= 11.9, '%d boost frames' % len(b))
r = run('10 14 B\n', 60)
check('boost_tap_nothing', all(x['st'] != 4 for x in r) and abs(r[-1]['gsp']) < 0.01)
r = run('10 12 X\n', 90)
check('attack_and_recover', any(x['st'] == 1 for x in r) and r[-1]['st'] == 0)
r = run('10 20 J\n30 45 J\n', 140)
check('dragon_cyclone', any(x['st'] == 2 for x in r) and r[-1]['gnd'] == 1)

# 8 recovered animations are driven by state (idle -> run anim index changes, frames advance)
r = run('0 200 R\n', 200)
check('anims_change', r[0]['an'] != r[-1]['an'] and len({x['fr'] for x in r[100:]}) > 3)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
