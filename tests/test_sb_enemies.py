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


# ---- session 19b: SB1's deck fire (8996-9004, col_9_35; RV hub) and SB3's DNA warnings (9854-9858; RV dnaw)
def track(script, frames, env, spec):
    """'ob' rows of FP_TRACK=spec: (frame, type, x, y, hidden)"""
    e = dict(env); e['FP_TRACK'] = spec
    rows, ev, out = run(script, frames, e)
    obs = []
    for l in out.splitlines():
        t = l.split()
        if t and t[0] == 'ob': obs.append((int(t[1]), int(t[2]), float(t[5]), float(t[7]), int(t[t.index('hd') + 1])))
    return rows, ev, obs
rows, ev, obs = track('', 700, {'FP_LEVEL': 'sb1'}, '1039:-1,1067:-1,1066:-1')
warn = [o for o in obs if o[1] == 1039]
shown = {}
for f, t, x, y, hd in warn:
    if not hd: shown.setdefault(round(y), []).append(f)
# warning shown in streaks of 58 frames (B 60, 8997 / 8998; RV hub: B 59 .. 2); three warnings, RV first shots of I 2 / 1 / 0 at A > 60 (A = 10 I start)
streaks = []
for k in range(3):                                # the k-th warning (FP_TRACK keeps the object order)
    vis = [f for i, (f, t, x, y, hd) in enumerate(warn) if i % 3 == k and not hd]
    st = None
    for f in vis:
        if st is None or f != st[1] + 1: st = [f, f]; streaks.append(st)
        else: st[1] = f
streaks = [s_ for s_ in streaks if s_[1] < 690]
check('hub_warning_58', streaks and all(b - a + 1 == 58 for a, b in streaks), str([(a, b - a + 1) for a, b in streaks[:4]]))
first = sorted({min(f for f, t, x, y, hd in warn if not hd and abs(x - c) <= 100) for c in (2156, 1180, 1460)
                if any(not hd and abs(x - c) <= 100 for f, t, x, y, hd in warn)})
check('hub_first_shots_A10I', first[:3] == [41, 51, 61] or (len(first) >= 3 and first[1] - first[0] == 10 and first[2] - first[1] == 10), str(first[:3]))
balls = [o for o in obs if o[1] == 1067]
b0 = [o for o in balls if o[0] == 42]
check('hub_ball_from_800_above', b0 and all(abs(y - (1192 - 800 + 12)) <= 1 for f, t, x, y, hd in b0), str(b0[:2]))
dy = [b[3] - a[3] for a, b in zip(balls, balls[1:]) if b[0] == a[0] + 1 and a[2] == b[2]]
check('hub_ball_falls_12', dy and max(dy) == 12 and min(dy) == 12, str(sorted(set(dy))[:4]))
check('hub_blast_sound_38', len(sids(ev, 38)) >= 2, str(sids(ev, 38)[:5]))
shots = sorted(set(f for f, t, x, y, hd in obs if t == 1066))
check('hub_ship_shots', len(shots) > 100, str(len(shots)))
# a done red ship (ML 0x10) removes the three warnings at the start (9223, all I still 0: RE)
rows, ev, obs = track('1 FLAGS 16\n2 LOAD sb1\n', 200, {'FP_LEVEL': 'sb2'}, '1039:-1')
check('hub_no_warnings_after_sb2', not [o for o in obs if o[0] > 20 and o[1] == 1039], str(len(obs)))
# SB3 DNA cannon 0 (1999, 756) turned left by the player below-left, off the right edge: warning at view x + 395 (RV dnaw: 1898 at f203)
rows, ev, obs = track('5 TP 1960 770\n6 SETV others E 99999\n200 TP 1700 714\n', 260, {'FP_LEVEL': 'sb3'}, '1039:2052')
w203 = [o for o in obs if o[0] == 203]
check('dna_warning_right_edge', w203 and w203[0][2] == 1898 and w203[0][4] == 0, str(w203))
w199 = [o for o in obs if o[0] == 199]
check('dna_warning_hidden_on_screen', w199 and w199[0][4] == 1, str(w199))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
