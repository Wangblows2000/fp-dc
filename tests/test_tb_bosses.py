#!/usr/bin/env python3
"""Host tests for the Thermal Base bosses (session 21c; src/game/tb_boss.c and the encounter files tb_boss_*.c;
build/re/spec_tb_bosses.md). Each encounter from entry to defeat to progression, for Lilac, Carol and Milla, against the original's
traces (RV numbers below, build/orig_traces/tb_bosses: s1_l the RoboShrimp, w1_l the wall pusher, p1_l the Syntax Spider, q1_l the
RoboSquid, x1_l the TB6 arena). The port's scripts teleport at 59 for the trace's TP at 60: port frame = RV frame - 2 (the shrimp's
path is frame-exact under that offset).
usage: test_tb_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, csv
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
TR = os.path.join(HERE, '..', 'build', 'orig_traces', 'tb_bosses')
DBG = ('tbs', 'tbw', 'tbp', 'tbq', 'tbx')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACK': '1', 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_TRACE_HUD': '1'})
    e.update({'FP_DBG_' + d.upper(): '1' for d in DBG}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('sid', 'music', 'voice', 'hud'):
            ev.append((len(rows), line))                     # (the frame the event happened in)
        elif t[0] in DBG and len(t) > 2:
            dbg.setdefault(t[0], {})[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, dbg

def sids(ev, *ids):
    out = []
    for f, l in ev:
        if l.startswith('sid ') and int(l.split()[1]) in ids and (f, int(l.split()[1])) not in out: out.append((f, int(l.split()[1])))
    return [f for f, _ in out]
def music(ev, name): return [f for f, l in ev if l.startswith('music') and name in l]
def near(a, b, tol=1): return len(a) == len(b) and all(abs(x - y) <= tol for x, y in zip(a, b))
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)
def rv_rows(name):
    p = os.path.join(TR, name)
    return {int(r['frame']): r for r in csv.DictReader(open(p))} if os.path.exists(p) else {}
def rv_snd(name, sid):
    p = os.path.join(TR, name)
    return [int(l.split()[0]) for l in open(p) if len(l.split()) > 2 and l.split()[1] == 'id' and int(l.split()[2]) == sid] if os.path.exists(p) else []

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 300, 600, 900, 1200, 1500, 1800, 2100))
