#!/usr/bin/env python3
"""Compact view of an original trace: frames a..b, columns that matter.
usage: show.py trace.csv [a] [b] [cols...]"""
import csv, sys
r = list(csv.DictReader(open(sys.argv[1])))
a = int(sys.argv[2]) if len(sys.argv) > 2 else 58
b = int(sys.argv[3]) if len(sys.argv) > 3 else 140
cols = sys.argv[4:] or ['held', 'px', 'py', 'skin_anim', 'mv_B', 'mv_G', 'mv_K', 'mv_L', 'mv_M', 'mv_O', 'mv_W', 'mv_J']
print('f   ' + ' '.join('%9s' % c.replace('mv_', '') for c in cols))
prev = None
for row in r[a:b]:
    vals = [row[c] for c in cols]
    if vals != prev:
        print('%-4s' % row['frame'] + ' '.join('%9s' % (v[:9]) for v in vals))
    prev = vals
