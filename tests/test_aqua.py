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

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
