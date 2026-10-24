#!/usr/bin/env python3
"""Host tests for Aqua Tunnel (session 12): Milla-only routes (Classic, Continue, Time Attack icon in Dragon Valley's
slot), the four acts loading and playing, act transitions, and the stage's enemies / gimmicks / bosses as they are
ported. Frame-exact behaviour is checked against the original by tools/orig/cmp_at.py; these tests guard the integration.
usage: test_aqua.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env, raw=False):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ, FP_TRACE_SID='1'); e.pop('FP_LEVEL', None); e.pop('FP_CHAR', None); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e); out = r.stdout + '\n' + r.stderr
    os.unlink(name)
    rows, sid = [], []
    for line in out.splitlines():
        t = line.split()
        if t and t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
            rows[-1]['sid'] = sid; sid = []
        elif t and t[0] == 'sid': sid.append(int(t[1]))
    return (rows, out) if raw else rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def loads(out): return [l.split('[', 1)[1].split(']', 1)[0] for l in out.splitlines() if l.startswith('load[')]

# ---- routes: only Milla reaches Aqua Tunnel (RE 799 Classic G1 == 4 -> frame 16; RE 623 TA icon E 0 in DV's slot)
_, out = run('40 42 S\n200 201 J\n230 231 J\n260 261 D\n280 281 J\n370 371 D\n380 381 J\n', 520, {}, raw=True)
check('classic_milla_starts_at1', loads(out)[-1:] == ['at1'], str(loads(out)))
_, out = run('40 42 S\n200 201 J\n230 231 J\n260 261 D\n280 281 J\n380 381 J\n', 700, {}, raw=True)
check('classic_lilac_starts_dv1', loads(out)[-1:] == ['dv1'], str(loads(out)))
_, out = run('40 42 S\n200 201 D\n220 221 J\n320 321 D\n330 331 J\n470 471 J\n490 491 D\n520 521 J\n630 631 J\n', 820, {}, raw=True)
check('time_attack_milla_aqua_tunnel', loads(out)[-1:] == ['at1'], str(loads(out)))

# ---- act transitions (spec_at_flow 2): AT1 X > 12462, AT2 X > 11534, AT3 top exit, as Dragon Valley's (fade, walk-in)
M = {'FP_CHAR': '4'}
_, out = run('5 TP 12400 1170\n10 300 R\n', 320, dict(M, FP_LEVEL='at1'), raw=True)
check('at1_exit_loads_at2', loads(out)[-1:] == ['at2'], str(loads(out)))
_, out = run('5 TP 11450 300\n10 300 R\n', 320, dict(M, FP_LEVEL='at2'), raw=True)
check('at2_exit_loads_at3', loads(out)[-1:] == ['at3'], str(loads(out)))
_, out = run('5 TP 400 300\n10 300 R\n', 320, dict(M, FP_LEVEL='at3'), raw=True)
check('at3_top_exit_loads_at4', loads(out)[-1:] == ['at4'], str(loads(out)))

# ---- bosses (aqua_boss.c): the worm head lands on the arena plate and holds the gate open; the snake's death runs the
# results and the raft exit back to the front end; the Chomper swallows and spits out (LIFE.B 10 = 2 half-petals)
_, out = run('55 SETV others E 99999\n60 TP 11760 866\n100 130 R\n400 BOSSHP 0\n', 800, dict(M, FP_LEVEL='at1', FP_TRACK='227:11967,211:11592,185:11488'), raw=True)
obs = [l.split() for l in out.splitlines() if l.startswith('ob ')]
head = [o for o in obs if o[2] == '227']; plate = [o for o in obs if o[2] == '211']; gate = [o for o in obs if o[2] == '185']
check('worm_defeat_head_lands', head and head[-1][9] == '9', ' '.join(head[-1][4:10]) if head else 'none')
check('worm_head_holds_plate', plate and plate[-1][19] == '1', ' '.join(plate[-1][17:20]) if plate else 'none')
check('worm_gate_open', gate and float(gate[-1][7]) > 896 + 100, ' '.join(gate[-1][4:8]) if gate else 'none')
rows, out = run('60 TP 6340 1186\n61 75 R\n', 420, dict(M, FP_LEVEL='at2'), raw=True)
st = [int(r['st']) for r in rows]
check('chomper_swallows_and_spits', 28 in st and rows[-1]['hp'] == 12, 'hp %s held %d' % (rows[-1]['hp'], st.count(28)))
_, out = run('60 TP 4952 254\n60 SETV others E 99999\n950 BOSSHP 0\n', 2000, dict(M, FP_LEVEL='at4'), raw=True)
check('snake_defeat_results_exit', loads(out)[-2:] == ['at4', 'frontend'], str(loads(out)))

# ---- Lilac and Carol through every act (host only; they never reach AT through the menus): no crash, the act plays
for ch, nm in (('0', 'lilac'), ('2', 'carol')):
    for a in (1, 2, 3, 4):
        rows = run('160 640 R\n170 172 J\n', 700, {'FP_CHAR': ch, 'FP_LEVEL': 'at%d' % a})   # AT1: charge the start jelly, jump off it
        xs = [r['x'] for r in rows]
        check('%s_plays_at%d' % (nm, a), len(rows) >= 600 and max(xs) - xs[0] > 100, 'x %.0f..%.0f' % (xs[0], max(xs)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
