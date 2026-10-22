#!/usr/bin/env python3
"""List the functions of the ORIGINAL executable that call a given function.
usage: callers.py '<demangled name>'   (needs build/re/nm.txt and build/re/chowdren.dis)"""
import bisect, re, sys
syms = []
for l in open('build/re/nm.txt'):
    p = l.rstrip('\n').split(' ', 2)
    if len(p) == 3 and p[0] and p[1] in 'Tt': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [a for a, _ in syms]
target = next(a for a, n in syms if n == sys.argv[1])
out = {}
for l in open('build/re/chowdren.dis'):
    m = re.match(r'\s*([0-9a-f]+):\s+call\s+([0-9a-f]+)\s', l)
    if m and int(m.group(2), 16) == target:
        a = int(m.group(1), 16); i = bisect.bisect_right(addrs, a) - 1
        out.setdefault(syms[i][1], []).append(hex(a))
for n, c in sorted(out.items()): print('%-50s %d call(s)' % (n, len(c)))
