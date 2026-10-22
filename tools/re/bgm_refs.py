#!/usr/bin/env python3
"""Map each BGM string object (str_data_bgm_*) to the functions that reference it.
usage: bgm_refs.py <Chowdren ELF> <objdump -d output> > out.txt"""
import bisect, re, subprocess, sys, collections
elf, dis = sys.argv[1], sys.argv[2]
nm = subprocess.run(['nm', '-C', elf], capture_output=True, text=True).stdout.splitlines()
funcs, strs = [], {}
for l in nm:
    p = l.split(' ', 2)
    if len(p) < 3 or not p[0]: continue
    a, t, n = int(p[0], 16), p[1], p[2]
    if t in 'Tt': funcs.append((a, n))
    if n.startswith('str_data_bgm_'): strs[a] = n
funcs.sort(); fa = [f[0] for f in funcs]
refs = collections.defaultdict(list)
rx = re.compile(r'^\s*([0-9a-f]+):.*\$0x([0-9a-f]+),')
for line in open(dis):
    if '$0x28' not in line: continue
    m = rx.match(line)
    if not m: continue
    v = int(m.group(2), 16)
    if v in strs:
        pc = int(m.group(1), 16)
        refs[strs[v]].append((pc, funcs[bisect.bisect_right(fa, pc) - 1][1]))
for s in sorted(refs, key=lambda k: int(k.rsplit('_', 1)[1])):
    fn = collections.Counter(f for _, f in refs[s] if not f.startswith('_GLOBAL__') and 'init_strings' not in f)
    print(s.replace('str_data_bgm_', '').rsplit('_', 2)[0], dict(fn))
