#!/usr/bin/env python3
"""Host tests for the Sky Battalion enemies (session 19; src/game/sb_enemy.c, build/re/spec_sb_enemies.md) with the original's
numbers (RV traces sbe/*): the Hopper Mk II (117-frame cycle, the frag bomb at the throw frame, sound 237), the Axle Turret
(96-frame cycle, three bullets per volley, sound 28), the DNA cannon (181-frame cycle, sound 265), the Pelletizer (a pellet pair
every 100 frames, sound 438), the Rotor Blade (pops out of its tube within 99 px, sound 7, lands with 54).
usage: test_sb_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=300)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'sbflow'):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sids(ev, n): return [i for i, l in ev if l.split()[:2] == ['sid', str(n)]]
def gaps(v): return [b - a for a, b in zip(v, v[1:])]

# ---- Hopper Mk II (SB2 1339, 632): throws a frag bomb every cycle (sound 237); RV cycle 117 frames
rows, ev, out = run('5 TP 1200 600\n', 500, {'FP_LEVEL': 'sb2'})
s = sids(ev, 237)
check('hopper_throws', len(s) >= 2, str(s[:5]))
check('hopper_cycle_117', len(s) >= 2 and all(abs(g - 117) <= 4 for g in gaps(s)), str(gaps(s)))   # port 120-121 (residual: the landing frame)

# ---- Axle Turret (SB2 2176, 648): volleys of 3 bullets (sound 28 per volley); RV cycle 96 frames
rows, ev, out = run('5 TP 2100 600\n', 400, {'FP_LEVEL': 'sb2'})
s = sorted(set(sids(ev, 28)))
check('axle_fires', len(s) >= 2, str(s[:6]))
check('axle_cycle_96', len(s) >= 2 and all(abs(g - 96) <= 3 for g in gaps(s)), str(gaps(s)))

# ---- DNA cannon (SB3 1999, 756): charge and fire (sound 265); RV cycle 181 frames
rows, ev, out = run('5 TP 1900 740\n', 600, {'FP_LEVEL': 'sb3'})
s = sids(ev, 265)
check('dna_fires', len(s) >= 2, str(s[:4]))
check('dna_cycle_181', len(s) >= 2 and all(abs(g - 181) <= 3 for g in gaps(s)), str(gaps(s)))

# ---- Pelletizer (SB4 1248, 563): a pellet pair every 100 frames (sound 438)
rows, ev, out = run('5 TP 1180 540\n', 400, {'FP_LEVEL': 'sb4'})
s = sids(ev, 438)
check('pelletizer_fires', len(s) >= 2, str(s[:4]))
check('pelletizer_cycle_100', len(s) >= 2 and all(abs(g - 100) <= 2 for g in gaps(s)), str(gaps(s)))

# ---- Rotor Blade (SB2 2656, 984): pops out when she is within 99 px (sound 7), lands (sound 54)
rows, ev, out = run('5 TP 2600 960\n', 300, {'FP_LEVEL': 'sb2'})
check('rotor_pops', len(sids(ev, 7)) >= 1, str(sids(ev, 7)[:3]))
check('rotor_lands', len(sids(ev, 54)) >= 1, str(sids(ev, 54)[:3]))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
