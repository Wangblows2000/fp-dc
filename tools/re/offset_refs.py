#!/usr/bin/env python3
"""Functions (optionally limited to a reachability list) whose disassembly references a member offset.
usage: offset_refs.py 0x213c8 [build/re/f21_all.txt] [--lines]"""
import bisect, re, sys
syms = []
for l in open('build/re/nm.txt'):
    p = l.rstrip('\n').split(' ', 2)
    if len(p) == 3 and p[0] and p[1] in 'Tt': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [a for a, _ in syms]
off = sys.argv[1]
lim = set(l.split(' ', 1)[1].strip() for l in open(sys.argv[2])) if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else None
hits = {}
for l in open('build/re/chowdren.dis'):
    if off + '(' in l:
        m = re.match(r'\s*([0-9a-f]+):\s+(.*)', l)
        if not m: continue
        n = syms[bisect.bisect_right(addrs, int(m.group(1), 16)) - 1][1]
        if lim is None or n in lim: hits.setdefault(n, []).append(m.group(2).strip())
for n, v in sorted(hits.items()):
    print(n, len(v), v[:2] if '--lines' in sys.argv else '')
