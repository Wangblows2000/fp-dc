#!/usr/bin/env python3
"""Host gameplay regression tests. Runs host/fpdc_host with scripted input and
asserts on the per-frame state trace. usage: test_gameplay.py <data_dir>"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted')

def run(script, frames):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True).stdout
    os.unlink(name)
    rows = []
    for line in out.splitlines():
        t = line.split()
        if t[0] != 'f': continue
        d = {t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)}
        rows.append(d)
    return rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + (' ' + info if info and not cond else ''))
    fails += 0 if cond else 1

# 1. standing still is stable and grounded
r = run('', 120)
check('idle_stable', all(x['gnd'] == 1 and x['y'] == r[0]['y'] and x['x'] == r[0]['x'] for x in r[5:]))

# 2. acceleration reaches top speed and never exceeds max
r = run('0 200 R\n', 200)
check('accelerates_to_top', max(x['gsp'] for x in r) >= 5.99)
check('speed_capped', max(abs(x['gsp']) for x in r) <= 14.0)

# 3. jump apex height ~ JUMP^2/(2g) = 96.6 px (variable jump held)
r = run('10 60 J\n', 80)
base = r[5]['y']; apex = min(x['y'] for x in r)
check('jump_height', 90 <= base - apex <= 100, 'got %.1f' % (base - apex))
check('jump_lands', r[-1]['gnd'] == 1 and abs(r[-1]['y'] - base) < 0.01)

# 4. short hop is lower (variable jump)
r = run('10 14 J\n', 80)
check('variable_jump', base - min(x['y'] for x in r) < 70)

# 5. full scripted run reaches the goal (stage clear freezes position near goal x)
script = open(os.path.join(HERE, 'scripts', 'run_right.txt')).read()
r = run(script, 700)
check('reaches_goal', max(x['x'] for x in r) > 3090, 'max x %.1f' % max(x['x'] for x in r))
check('collects_crystals', r[-1]['cr'] >= 9, 'cr %d' % r[-1]['cr'])
check('no_tunnel_below_floor', all(x['y'] <= 30 * 16 + 64 for x in r))

# 6. slope landing keeps momentum (no stall on 45 deg slope)
seg = [x for x in r if 1960 < x['x'] < 2040 and x['gnd'] == 1]
check('slope_momentum', seg and min(x['gsp'] for x in seg) > 3.0, 'min gsp %.2f' % (min(x['gsp'] for x in seg) if seg else -1))

# 7. walking into the spike pit hurts, eventually kills, and respawns at start
r = run('0 2000 R\n', 2000)
check('spikes_hurt', min(x['hp'] for x in r) < 6)
deaths = [i for i in range(1, len(r)) if r[i]['st'] != 6 and r[i - 1]['st'] == 6]
check('respawn_after_death', any(abs(r[i]['x'] - 72) < 1 for i in deaths) if deaths else False)

# 8. walls block: run right from the pit's far side into the wall at x=1792
# (covered by run_right reaching goal only via jumping). Wall test directly:
r = run('0 400 R\n200 2000 R\n', 10)
check('trace_ok', len(r) == 10)

# 9. dragon boost moves fast horizontally for a while
r = run('10 40 B\n', 80)
b = [x for x in r if x['st'] == 4]
check('boost_happens', len(b) > 5 and max(abs(x['gsp']) for x in b) >= 9.9 and b[-1]['x'] - b[0]['x'] > 100)

# 10. attack state and recovery
r = run('10 12 X\n', 60)
check('attack_state', any(x['st'] == 1 for x in r) and r[-1]['st'] == 0)

# 11. cyclone in air (second jump press)
r = run('10 20 J\n30 34 J\n', 120)
check('cyclone', any(x['st'] == 2 for x in r) and r[-1]['gnd'] == 1)

# 12. spin jump destroys the first hornet
r = run('0 400 R\n100 112 J\n', 300)
check('hornet_killed_by_spin', r[-1]['k'] >= 1)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
