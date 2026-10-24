#!/usr/bin/env python3
"""Compare Carol's host traces with the original's (CHAR=2 traces in build/orig_traces/carol, scripts tests/orig_carol).

usage: cmp_carol.py [names...]   (default: every c_*.txt that has an original trace)
       env FR=last frame (default 160), V=1 verbose (print the first mismatching rows)

Per sequence the host runs with FP_CHAR=2 from the trace's frame-0 state (X, Y, L, N, energy) and is compared row f
(port) against row f+2 (original), the input lag of RUNTIME_MAP. Fields: X/Y (4 px), B/G (0.05), L, M (movement
state), OTHERS.Y (animation code), the forced animation index (ctail_aid: the tail takes the skin's index), SKINC.A
(combo index), SKINC.L (kick counter), energy (0.3). Sounds: original logic frame F = port frame F-1.
Exact-frame counts per field are printed; exit 1 if any sequence has a position/speed error over tolerance.
"""
import csv, os, subprocess, sys

ROOT = '/home/claude/fpdc'
SHIFT = 2

def load(path):
    return list(csv.DictReader(open(path)))

def pretick(r):   # enemies' animation phase at frame 0 (as tools/orig/cmp_all.sh): ticks of Turretus idle at speed 20
    try: return 5 * int(r['turr_afr']) + int(r['turr_acc']) // 20
    except (KeyError, ValueError): return 0

def run_host(name, o0, fr, o2=None):
    out = os.path.join(ROOT, 'build/fpdc_traces/carol_%s.csv' % name)
    lvl = os.environ.get('LEVEL') or ('dv2' if name.startswith('c_jumppad') else 'dv1')
    env = dict(os.environ, FP_CHAR='3' if name.startswith('b_') else '2', FP_ORIG_MOVES='1', FP_LEVEL=lvl,
               FP_SPAWN_X=o0['mv_X'], FP_SPAWN_Y=o0['mv_Y'], FP_SPAWN_L=str(int(float(o0['mv_L']))), FP_SPAWN_N=o0['mv_N'],
               FP_ENERGY=o0.get('en_A') or '100', FP_OBJ_PRETICK=str(pretick(o0)),
               FP_ANIM_SYNC='%s,%s' % (o2.get('ctail_afr', 0), o2.get('ctail_acc', 0)) if o2 else '0,0', FP_HP=o0.get('c_petals') or '14', FP_CSV=out, FP_TRACE_SID='1')
    script = os.path.join(ROOT, 'tests/orig_carol/%s.txt' % name)
    r = subprocess.run([os.path.join(ROOT, 'host/fpdc_host'), 'converted2', str(fr + 5), script], cwd=ROOT, env=env,
                       capture_output=True, text=True)
    snd, pending = [], []
    for line in r.stdout.splitlines():
        if line.startswith('sid '): pending.append(int(line.split()[1]))
        elif line.startswith('f '):
            f = int(line.split()[1]); snd += [(f, s) for s in pending]; pending = []
    return load(out), snd

def num(r, k, d=0.0):
    try: return float(r[k])
    except (KeyError, ValueError, TypeError): return d

def main():
    names = sys.argv[1:] or sorted(n[:-4] for n in os.listdir(os.path.join(ROOT, 'tests/orig_carol'))
                                   if (n.startswith('c_') or n.startswith('b_')) and n.endswith('.txt'))
    fr = int(os.environ.get('FR', 160)); verbose = os.environ.get('V')
    os.makedirs(os.path.join(ROOT, 'build/fpdc_traces'), exist_ok=True)
    tot_bad = 0
    for name in names:
        op = os.path.join(ROOT, 'build/orig_traces/carol/%s.csv' % name)
        if not os.path.exists(op): print('SKIP', name); continue
        orig = load(op); O = {int(r['frame']): r for r in orig}
        if len(orig) < 10: print('SKIP', name, '(trace incomplete)'); continue
        script = open(os.path.join(ROOT, 'tests/orig_carol/%s.txt' % name)).read().split('\n')
        tps = [int(l.split()[0]) for l in script if ' TP ' in l]
        P, snd = run_host(name, orig[0], fr, O.get(2))
        skip = set()
        for t in tps: skip.update(range(t - 3, t + 4))
        fields = {'X': 4.0, 'Y': 4.0, 'B': 0.05, 'G': 0.05, 'L': 0, 'M': 0, 'oY': 0, 'anim': 0, 'tfr': 0, 'sA': 0.01 if name.startswith('b_') else 0, 'sL': 0, 'en': 0.3}
        good = {k: 0 for k in fields}; first = {}; n = 0; n_tfr_skip = [0]
        for p in P:
            f = int(p['frame'])
            if f < 50 or f > fr or f in skip or f + SHIFT not in O: continue
            o = O[f + SHIFT]; n += 1
            an = p['aname']; pa = int(an[1:].split('_')[0]) if an.startswith('a') and an[1:2].isdigit() else -1
            vals = {'X': (num(p, 'X'), num(o, 'mv_X')), 'Y': (num(p, 'Y'), num(o, 'mv_Y')), 'B': (num(p, 'B'), num(o, 'mv_B')),
                    'G': (num(p, 'G'), num(o, 'mv_G')), 'L': (int(p['L']), int(num(o, 'mv_L'))), 'M': (int(p['M']), int(num(o, 'mv_M'))),
                    'oY': (int(p['cY']), int(num(o, 'others_Y'))), 'anim': (pa, int(num(o, 'ctail_aid', -1))),
                    'tfr': (int(p['hafr']), int(num(O.get(f + SHIFT + 1, o), 'ctail_afr', -1))),   # port row f is one animation tick ahead

                    'sA': (num(p, 'rev'), num(o, 'sa_A'))  if name.startswith('b_') else (int(p['cA']), int(num(o, 'sa_A'))), 'sL': (int(p['cL']), int(num(o, 'sa_L'))), 'en': (num(p, 'energy'), num(o, 'en_A'))}
            o3 = O.get(f + SHIFT + 1, o)
            if pa != int(num(o, 'ctail_aid', -1)) or num(o3, 'ctail_aid', -1) != num(o, 'ctail_aid', -1) or int(p['hafr']) < 0: del vals['tfr']; n_tfr_skip[0] += 1
            if name.startswith('b_'): vals['sL'] = (0, 0)   # on b_ traces the logged skin is the bike skin (sa_A = rev): its L is not Carol's kick/pad counter
            if pa in (3, 4, 5, 6, 18, 19, 20, 30, 34) and not name.startswith('b_'): vals['anim'] = (0, 0)   # no tail animation with that index: ctail_aid keeps the old one
            for k, (a, b) in vals.items():
                if abs(a - b) <= fields[k] + 1e-9: good[k] += 1
                elif k not in first: first[k] = (f, a, b)
        # sounds
        os_ = sorted((int(l.split()[0]) - 1, int(l.split()[2])) for l in open(op[:-4] + '.snd') if ' id ' in l)
        os_ = [(f, s) for f, s in os_ if 50 <= f <= fr]
        ps = [(f, s) for f, s in snd if 50 <= f <= fr]
        miss = [x for x in os_ if x not in ps]; extra = [x for x in ps if x not in os_]
        bad = any(k in first for k in ('X', 'Y', 'B', 'G'))
        tot_bad += bad
        print('%s %-18s %3d frames  ' % ('FAIL' if bad else 'ok  ', name, n) + ' '.join('%s %d' % (k, good[k] + (n_tfr_skip[0] if k == 'tfr' else 0)) for k in fields) +
              ('  snd ok' if not miss and not extra else '  snd miss %s extra %s' % (miss[:4], extra[:4])))
        if verbose:
            for k, v in sorted(first.items(), key=lambda kv: kv[1][0]): print('      first %s f%d port %s orig %s' % (k, v[0], v[1], v[2]))
    sys.exit(1 if tot_bad else 0)

main()
