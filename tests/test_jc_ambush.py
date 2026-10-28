#!/usr/bin/env python3
"""Host tests for Jade Creek act 5's ice ambush (session 22; src/game/jc_boss.c M_AMBUSH, RE 11043-11072 group 0x22afe,
RV build/orig_traces/jc_boss5 amb_l / amb_c / amb_m: the act start, holding right). Per character (Lilac, Carol, Milla):
 - the trigger (11055, X > Neera.x - 200): sound 297 (the missile), Neera lands 25 frames later (11058: sound 33; RV 289 -> 314),
   the missile bursts past her at +52 (11060: sounds 77 + 98; RV 341) and freezes her (11063: E 2 the same frame), the
   Hovercop reaches barrier.x + 120 at about +133 (11067: E 3; RV 422), the gate opens 30 frames later (11069: sound 40; RV 451);
 - the barrier at x 1536 opens and the player gets past x 1600 (the user report: Lilac stopped at x 1519.7 forever);
 - a checkpoint restart (g4 2, CP_JC5 x 9800-9999) removes the whole set (11043): the way through x 1536 is open.
usage: test_jc_ambush.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(char, frames, script):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_JCB_SET')]
    e.update({'FP_LEVEL': 'jc5', 'FP_CHAR': str(char), 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_DBG_JCB': '1'})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev, nE = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'jcbamb'): ev.append((len(rows), line))
        elif t[0] == 'jcb':
            d = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
            nE.setdefault(int(float(d['nE'])), len(rows))           # first row of each Neera state
    return rows, ev, nE

def sids(ev, i): return [f for f, l in ev if l.split()[0] == 'sid' and int(l.split()[1]) == i]
def near(a, b, tol): return a is not None and b is not None and abs(a - b) <= tol

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

for ch, cn in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, ev, nE = run(ch, 900, '20 900 R\n')
    t0 = (sids(ev, 297) or [None])[0]
    rel = lambda i: [f - t0 for f in sids(ev, i) if t0 is not None and f >= t0][:1]
    opened = [f for f, l in ev if l.startswith('jcbamb') and l.endswith('open')]
    check('ambush_%s_trigger' % cn, t0 is not None and nE.get(0) is not None and near(nE[0], t0, 1), str((t0, nE.get(0))))
    check('ambush_%s_landing_33' % cn, rel(33) and near(rel(33)[0], 25, 2) and near(nE.get(1, -99) - t0, 25, 2), str((rel(33), nE.get(1))))
    check('ambush_%s_burst_freeze' % cn, rel(77) and near(rel(77)[0], 52, 2) and rel(98) == rel(77) and near(nE.get(2, -99) - t0, 52, 2),
          str((rel(77), rel(98), nE.get(2))))
    check('ambush_%s_wait' % cn, near(nE.get(3, -99) - t0, 133, 4), str(nE.get(3)))
    check('ambush_%s_gate_opens_40' % cn, opened and rel(40) and near(rel(40)[0], 162, 4) and opened[0] == t0 + rel(40)[0] and near(nE.get(4, -99) - t0, 162, 4),
          str((opened, rel(40), nE.get(4))))
    after = [r['x'] for r in rows[opened[0]:]] if opened else []
    check('ambush_%s_past_1600' % cn, after and max(after) > 1600, 'max x %.1f' % (max(after) if after else -1))

# ---- the checkpoint restart: g4 2 at (9826, 780), a death, the restart there; the set is gone (11043: no crystals, no sound 40),
# the barrier too: back at x 1300 the player walks through x 1536
for ch, cn in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, ev, nE = run(ch, 700, '5 TP 9826 760\n20 HP 1\n22 HURT 99\n350 TP 1300 1690\n360 700 R\n')
    kills = [l for f, l in ev if l.startswith('jcbamb') and 'kill' in l]
    xs = [r['x'] for r in rows[351:]]
    check('ambush_%s_checkpoint_open' % cn, len(kills) >= 2 and not sids(ev, 40) and not sids(ev, 297) and xs and max(xs) > 1600,
          str((kills, sids(ev, 40), max(xs) if xs else None)))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
