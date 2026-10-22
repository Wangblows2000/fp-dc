#!/usr/bin/env python3
"""Animation agreement with the original: per sequence, the share of frames where Lilac's animation number
and frame index equal the original's (skin_anim = anim.dir.frame), with the comparator's 2-frame shift.
fpdc bank index -> original animation number via tools/import_frame.py LILAC (bank = 2 * entry + left).
usage: anim_match.py [sequence ...]   (reads build/orig_traces and build/fpdc_traces)"""
import csv, glob, os, re, sys
src = open(os.path.join(os.path.dirname(__file__), '..', 'import_frame.py')).read()
LILAC = [int(n) for n in re.findall(r"\('[a-z_]+', (\d+)\)", src[src.index('LILAC = ['):src.index(']', src.index('LILAC = ['))])]
names = sys.argv[1:] or sorted(os.path.basename(p)[:-4] for p in glob.glob('build/fpdc_traces/*.csv'))
tot_a = tot_f = tot = 0
for n in names:
    try:
        o = {int(r['frame']): r for r in csv.DictReader(open('build/orig_traces/%s.csv' % n))}
        f = list(csv.DictReader(open('build/fpdc_traces/%s.csv' % n)))
    except FileNotFoundError:
        continue
    a = fr = c = 0
    for r in f:
        k = int(r['frame'])
        if k < 50 or k + 2 not in o or 'anim' not in r: continue
        sa = o[k + 2]['skin_anim'].split('.')
        if len(sa) != 3: continue
        oa, of = int(sa[0]), int(sa[2])
        bi = int(r['anim']); ma = LILAC[bi // 2] if bi // 2 < len(LILAC) else -1
        c += 1; a += (ma == oa); fr += (ma == oa and int(r['afr']) == of)
    if c:
        tot += c; tot_a += a; tot_f += fr
        if len(names) < 10 or '-v' in sys.argv: print('%-24s anim %5.1f%%  anim+frame %5.1f%%' % (n, 100. * a / c, 100. * fr / c))
print('ALL %d frames: anim %.1f%%  anim+frame %.1f%%' % (tot, 100. * tot_a / max(tot, 1), 100. * tot_f / max(tot, 1)))
