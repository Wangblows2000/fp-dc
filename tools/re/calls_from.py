#!/usr/bin/env python3
"""Functions called (directly) by a function of the ORIGINAL executable, in call order.
usage: calls_from.py '<demangled name>'"""
import bisect, re, sys
syms = []
for l in open('build/re/nm.txt'):
    p = l.rstrip('\n').split(' ', 2)
    if len(p) == 3 and p[0] and p[1] in 'Tt': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [a for a, _ in syms]; name = dict(syms)
start = next(a for a, n in syms if n == sys.argv[1]); end = addrs[bisect.bisect_right(addrs, start)]
seen = []
for l in open('build/re/chowdren.dis'):
    m = re.match(r'\s*([0-9a-f]+):\s+call\s+([0-9a-f]+)\s', l)
    if m and start <= int(m.group(1), 16) < end:
        t = int(m.group(2), 16)
        if t in name and name[t] not in seen: seen.append(name[t])
print('\n'.join(seen))
