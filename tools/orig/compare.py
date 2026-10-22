#!/usr/bin/env python3
"""
Compare an original-game trace (tools/orig/trace_orig.py) with an fpdc host trace (FP_CSV).

usage: compare.py <orig.csv> <fpdc.csv> [--shift 2] [--from F] [--to F] [--quiet]

Alignment: the original applies a key injected at frame f from the state read at frame f+2
(RUNTIME_MAP "input lag"); fpdc's row f already includes frame f's input. So fpdc row f is
compared with original row f+shift (default 2).
Fields: X (orig mv_X), Y (orig mv_Y = feet-14), B (ground speed / air x speed), G (air y speed), L (ground flag).
Tolerances (prompt v3 "practical"): positions ~4 px, speeds 0.05, key outcomes:
apex 3 px, top speed 3%, time-to-top 3 frames, distances 5%, timings +-2 frames.
Exit code 0 when every key outcome passes.
"""
import csv, sys

def load(path):
    return list(csv.DictReader(open(path)))

def series(rows, orig):
    out = []
    for r in rows:
        if orig:
            out.append({'f': int(r['frame']), 'held': r['held'], 'X': float(r['mv_X']), 'Y': float(r['mv_Y']),
                        'B': float(r['mv_B']), 'G': float(r['mv_G']), 'L': int(float(r['mv_L'])),
                        'M': int(float(r['mv_M'])), 'anim': r['skin_anim']})
        else:
            out.append({'f': int(r['frame']), 'held': r['held'], 'X': float(r['X']), 'Y': float(r['Y']),
                        'B': float(r['B']), 'G': float(r['G']), 'L': int(r['L']), 'M': int(r['state']),
                        'anim': r['anim']})
    return out

def outcomes(s, f0):
    seg = [r for r in s if r['f'] >= f0]
    o = {}
    sp = [abs(r['B']) for r in seg if r['L'] == 1]
    if sp:
        top = max(sp); o['top_speed'] = top
        hit = next((i for i, r in enumerate(seg) if r['L'] == 1 and abs(r['B']) >= 0.98 * top), None)
        o['frames_to_top'] = hit
    ys = [r['Y'] for r in seg]
    o['apex_rise'] = seg[0]['Y'] - min(ys) if ys else 0
    air = [i for i, r in enumerate(seg) if r['L'] == 0]
    o['airtime'] = len(air)
    o['distance_x'] = seg[-1]['X'] - seg[0]['X']
    # frames until the speed returns to 0 after first being non-zero (stop timing)
    nz = next((i for i, r in enumerate(seg) if abs(r['B']) > 1e-6), None)
    if nz is not None:
        z = next((i for i, r in enumerate(seg[nz:]) if abs(r['B']) < 1e-6 and r['L'] == 1), None)
        o['moving_frames'] = z
    return o

def main():
    a = sys.argv
    shift = int(a[a.index('--shift') + 1]) if '--shift' in a else 2
    f_from = int(a[a.index('--from') + 1]) if '--from' in a else 0
    f_to = int(a[a.index('--to') + 1]) if '--to' in a else 10 ** 9
    quiet = '--quiet' in a
    # --skip a-b,c-d: frames excluded from the per-frame error report (teleports land 'shift' frames
    # apart in the two traces, so the frames around a mid-sequence TP are not comparable)
    skip = set()
    if '--skip' in a:
        for rg in a[a.index('--skip') + 1].split(','):
            if rg:
                lo, hi = (int(v) for v in rg.split('-')); skip.update(range(lo, hi + 1))
    O = {r['f']: r for r in series(load(a[1]), True)}
    P = series(load(a[2]), False)
    pairs = [(p, O[p['f'] + shift]) for p in P if f_from <= p['f'] <= f_to and p['f'] + shift in O]
    tol = {'X': 4.0, 'Y': 4.0, 'B': 0.05, 'G': 0.05}
    first, maxerr = {}, {k: (0.0, None) for k in tol}
    for p, o in pairs:
        if p['f'] in skip: continue
        for k in tol:
            e = abs(p[k] - o[k])
            if e > maxerr[k][0]: maxerr[k] = (e, p['f'])
            if e > tol[k] and k not in first: first[k] = (p['f'], p[k], o[k])
    # outcome metrics describe one continuous movement: evaluate each segment between mid-sequence
    # teleports separately (every segment must pass)
    segs, cur = [], []
    for p, o in pairs:
        if p['f'] in skip:
            if cur: segs.append(cur); cur = []
            continue
        cur.append((p, o))
    if cur: segs.append(cur)
    checks = []
    def chk(name, ok, detail): checks.append((name, ok, detail))
    for si, sg in enumerate(segs):
        if len(sg) < 5: continue
        tag = '' if len(segs) == 1 else '[%d] ' % si
        f0 = sg[0][0]['f']
        po = outcomes([p for p, _ in sg], f0)
        oo = outcomes([dict(o, f=o['f'] - shift) for _, o in sg], f0)
        if 'top_speed' in oo and 'top_speed' in po:
            chk(tag + 'top_speed', abs(po['top_speed'] - oo['top_speed']) <= 0.03 * max(oo['top_speed'], 0.1),
                '%.3f vs %.3f' % (po['top_speed'], oo['top_speed']))
        if oo.get('frames_to_top') is not None and po.get('frames_to_top') is not None:
            chk(tag + 'frames_to_top', abs(po['frames_to_top'] - oo['frames_to_top']) <= 3, '%d vs %d' % (po['frames_to_top'], oo['frames_to_top']))
        chk(tag + 'apex_rise', abs(po['apex_rise'] - oo['apex_rise']) <= 3, '%.1f vs %.1f' % (po['apex_rise'], oo['apex_rise']))
        chk(tag + 'airtime', abs(po['airtime'] - oo['airtime']) <= 2, '%d vs %d' % (po['airtime'], oo['airtime']))
        dx = max(abs(oo['distance_x']), 1.0)
        chk(tag + 'distance_x', abs(po['distance_x'] - oo['distance_x']) <= max(0.05 * dx, 4), '%.1f vs %.1f' % (po['distance_x'], oo['distance_x']))
        if oo.get('moving_frames') is not None and po.get('moving_frames') is not None:
            chk(tag + 'moving_frames', abs(po['moving_frames'] - oo['moving_frames']) <= 2, '%d vs %d' % (po['moving_frames'], oo['moving_frames']))
    ok = all(c[1] for c in checks)
    print('%s  %s  (fpdc vs original, shift %d, frames %d..%d)' % ('PASS' if ok else 'FAIL', a[2].split('/')[-1], shift, f_from, min(f_to, P[-1]['f'])))
    for n, good, d in checks:
        print('   %-14s %s  %s' % (n, 'ok ' if good else 'BAD', d))
    if not quiet:
        print('   max error: ' + ', '.join('%s %.3f@%s' % (k, v[0], v[1]) for k, v in maxerr.items()))
        print('   first out of tolerance: ' + (', '.join('%s @f%d (%.3f vs %.3f)' % (k, *v) for k, v in first.items()) or 'none'))
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
