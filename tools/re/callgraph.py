#!/usr/bin/env python3
"""One-pass call graph of the ORIGINAL executable (build/re/callgraph.json: function -> callees), and
reachability queries. usage: callgraph.py build | callgraph.py reach '<root>' [regex]"""
import bisect, json, re, sys
if sys.argv[1] == 'build':
    syms = []
    for l in open('build/re/nm.txt'):
        p = l.rstrip('\n').split(' ', 2)
        if len(p) == 3 and p[0] and p[1] in 'TtWw': syms.append((int(p[0], 16), p[2]))
    syms.sort(); addrs = [a for a, _ in syms]; name = dict(syms)
    g = {}
    for l in open('build/re/chowdren.dis'):
        m = re.match(r'\s*([0-9a-f]+):\s+(?:call|jmp)\s+([0-9a-f]+)\s', l)
        if not m: continue
        a, t = int(m.group(1), 16), int(m.group(2), 16)
        if t not in name: continue
        i = bisect.bisect_right(addrs, a) - 1
        f = syms[i][1]
        if name[t] != f: g.setdefault(f, []).append(name[t]) if name[t] not in g.get(f, []) else None
    json.dump(g, open('build/re/callgraph.json', 'w'))
    print(len(g), 'functions')
else:
    g = json.load(open('build/re/callgraph.json'))
    root = sys.argv[2]; pat = re.compile(sys.argv[3]) if len(sys.argv) > 3 else None
    seen, todo = {root: 0}, [root]
    while todo:
        f = todo.pop()
        for c in g.get(f, []):
            if c not in seen and c.startswith('Frames::'): seen[c] = seen[f] + 1; todo.append(c)
    for f, d in sorted(seen.items(), key=lambda kv: kv[1]):
        if pat is None or pat.search(f): print(d, f)
