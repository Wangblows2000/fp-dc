#!/usr/bin/env python3
"""Compare Aqua Tunnel host traces with the original's (session 12; traces build/orig_traces/at, scripts tests/orig_at).

usage: cmp_at.py [names...]   (default: every tests/orig_at/*.txt that has an original trace)
       env FR=last frame (default: trace length - 5), V=1 verbose (first mismatching rows per field)

The act comes from the name prefix (a1_ .. a4_ -> at1 .. at4). An optional sidecar tests/orig_at/<name>.cfg holds
  CHAR <n>                       character (default 4, Milla)
  TRACK <prefix> <type> <spawnx> compare the original extra object <prefix>_x/_y (XVT_EXTRA) with the port object of
                                 engine type <type> spawned at x = <spawnx> (host FP_TRACK)
  OSHIFT <n>                     row shift for tracked objects (default 2, as the player)
The host runs from the trace's frame-0 player state (as tools/orig/cmp_milla.py) and port row f is compared with
original row f+2 (input lag, RUNTIME_MAP). Player fields X/Y (4 px), B/G (0.05), L, M; tracked objects x/y (2 px).
Sounds: original frame F = port frame F-1. Exit 1 if any trace has a player position/speed or object error.
"""
import csv, os, subprocess, sys

ROOT = '/home/claude/fpdc'
SHIFT = 2

def load(path): return list(csv.DictReader(open(path)))
def num(r, k, d=0.0):
    try: return float(r[k])
    except (KeyError, ValueError, TypeError): return d

def cfg(name):
    c = {'CHAR': '4', 'TRACK': [], 'OSHIFT': SHIFT, 'PRERUN': '122'}   # RV a1_current: the card runs object events before row 0 (122 = 120 + the 2-frame row shift)
    p = os.path.join(ROOT, 'tests/orig_at/%s.cfg' % name)
    if os.path.exists(p):
        for l in open(p):
            t = l.split()
            if not t or t[0].startswith('#'): continue
            if t[0] == 'CHAR': c['CHAR'] = t[1]
            elif t[0] == 'TRACK': c['TRACK'].append((t[1], int(t[2]), int(t[3]), int(t[4]) if len(t) > 4 else None))
            elif t[0] == 'OSHIFT': c['OSHIFT'] = int(t[1])
            elif t[0] == 'PRERUN': c['PRERUN'] = t[1]      # frames of object events before row 0 (the stage card)
            elif t[0] == 'GT': c['GT'] = t[1]              # GV[3] at row 0 (parity of the 2-frame cadences)
    return c

def run_host(name, c, o0, fr):
    lvl = 'at' + name[1]
    out = os.path.join(ROOT, 'build/fpdc_traces/at_%s.csv' % name)
    env = dict(os.environ, FP_CHAR=c['CHAR'], FP_ORIG_MOVES='1', FP_LEVEL=lvl, FP_SPAWN_X=o0['mv_X'], FP_SPAWN_Y=o0['mv_Y'],
               FP_SPAWN_L=str(int(float(o0['mv_L']))), FP_SPAWN_N=o0['mv_N'], FP_ENERGY=o0.get('en_A') or '100',
               FP_HP=o0.get('c_petals') or '14', FP_CSV=out, FP_TRACE_SID='1')
    if 'PRERUN' in c: env['FP_AT_PRERUN'] = c['PRERUN']
    if 'GT' in c: env['FP_AT_GT'] = c['GT']
    elif 'gv3' in o0: env['FP_AT_GT'] = str(int(float(o0['gv3'])))
    if c['TRACK']: env['FP_TRACK'] = ','.join('%d:%d' % (t, x) for _, t, x, _s in c['TRACK'])
    # teleports / alterable writes act on the original's frame f directly, keys are seen 2 frames later (input lag): the host
    # gets the TP / SETV lines 2 frames earlier so that port row f matches original row f + 2 after a teleport as well
    scr = os.path.join(ROOT, 'build/fpdc_traces/at_%s.txt' % name)
    with open(scr, 'w') as sf:
        for l in open(os.path.join(ROOT, 'tests/orig_at/%s.txt' % name)):
            t = l.split()
            if len(t) >= 2 and t[1] in ('TP', 'SETV', 'SETB'): t[0] = str(max(0, int(t[0]) - int(os.environ.get('TPSHIFT', '0'))))
            sf.write(' '.join(t) + '\n')
    r = subprocess.run([os.path.join(ROOT, 'host/fpdc_host'), 'converted2', str(fr + 5), scr],
                       cwd=ROOT, env=env, capture_output=True, text=True)
    snd, pending, obs = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'sid': pending.append(int(t[1]))
        elif t[0] == 'f': f = int(t[1]); snd += [(f, s) for s in pending]; pending = []
        elif t[0] == 'ob': obs[(int(t[1]), int(t[2]), int(t[3]))] = {t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2)}
    return load(out), snd, obs

def main():
    os.makedirs(os.path.join(ROOT, 'build/fpdc_traces'), exist_ok=True)
    names = sys.argv[1:] or sorted(n[:-4] for n in os.listdir(os.path.join(ROOT, 'tests/orig_at')) if n.endswith('.txt'))
    verbose = os.environ.get('V'); tot_bad = 0
    for name in names:
        op = os.path.join(ROOT, 'build/orig_traces/at/%s.csv' % name)
        if not os.path.exists(op): print('SKIP', name); continue
        orig = load(op); O = {int(r['frame']): r for r in orig}
        if len(orig) < 10: print('SKIP', name, '(trace incomplete)'); continue
        fr = int(os.environ.get('FR', len(orig) - 8))
        c = cfg(name)
        tr = []                                   # "pre*": the instance of that family whose frame-0 x is the spawn x
        for pre, t, x0, sh in c['TRACK']:
            if pre.endswith('*'):
                cand = [k[:-2] for k in orig[0] if k.startswith(pre[:-1]) and k.endswith('_x') and (abs(num(orig[0], k) - x0) < 1 or abs(num(orig[0], k[:-2] + '_F') - x0) < 1)]
                pre = cand[0] if cand else pre[:-1] + '?'
            tr.append((pre, t, x0, sh))
        c['TRACK'] = tr
        P, snd, obs = run_host(name, c, orig[0], fr)
        tps = [int(l.split()[0]) for l in open(os.path.join(ROOT, 'tests/orig_at/%s.txt' % name)) if ' TP ' in l]
        skip = set()
        for t in tps: skip.update(range(t - 3, t + 4))
        f0 = max([50] + [t + 4 for t in tps])          # compare after the last teleport
        fields = {'X': 4.0, 'Y': 4.0, 'B': 0.05, 'G': 0.05, 'L': 0, 'M': 0}
        for pre, _, _, _s in c['TRACK']: fields[pre] = 2.0
        good = {k: 0 for k in fields}; first = {}; n = 0
        for p in P:
            f = int(p['frame'])
            if f < f0 or f > fr or f in skip or f + SHIFT not in O: continue
            o = O[f + SHIFT]; n += 1
            vals = {'X': (num(p, 'X'), num(o, 'mv_X')), 'Y': (num(p, 'Y'), num(o, 'mv_Y')), 'B': (num(p, 'B'), num(o, 'mv_B')),
                    'G': (num(p, 'G'), num(o, 'mv_G')), 'L': (int(p['L']), int(num(o, 'mv_L'))), 'M': (int(p['M']), int(num(o, 'mv_M')))}
            for pre, t, x0, sh in c['TRACK']:
                q = obs.get((f, t, x0))
                if q is None: continue
                ds = []
                for shf in ([c['OSHIFT']] if sh is None else [sh, SHIFT]):   # world objects: shift 0, after a player-driven hit: 2
                    oo = O.get(f + shf)
                    if oo is not None and (pre + '_x') in oo: ds.append(max(abs(q['x'] - num(oo, pre + '_x')), abs(q['y'] - num(oo, pre + '_y'))))
                if ds: vals[pre] = (min(ds), 0)
            for k, (a_, b_) in vals.items():
                if abs(a_ - b_) <= fields[k] + 1e-9: good[k] += 1
                elif k not in first: first[k] = (f, a_, b_)
            for k in fields:
                if k not in vals: good[k] += 1
        sp = op[:-4] + '.snd'
        os_ = sorted((int(l.split()[0]) - 1, int(l.split()[2])) for l in open(sp) if ' id ' in l) if os.path.exists(sp) else []
        os_ = [(f, s) for f, s in os_ if f0 <= f <= fr]
        ps = [(f, s) for f, s in snd if f0 <= f <= fr]
        miss = [x for x in os_ if x not in ps]; extra = [x for x in ps if x not in os_]
        bad = any(k in first for k in fields if k not in ('L', 'M'))
        tot_bad += bad
        print('%s %-22s %3d frames  ' % ('FAIL' if bad else 'ok  ', name, n) + ' '.join('%s %d' % (k, good[k]) for k in fields) +
              ('  snd ok' if not miss and not extra else '  snd miss %s extra %s' % (miss[:4], extra[:4])))
        if verbose:
            for k, v in sorted(first.items(), key=lambda kv: kv[1][0]): print('      first %s f%d port %s orig %s' % (k, v[0], v[1], v[2]))
    sys.exit(1 if tot_bad else 0)

main()
