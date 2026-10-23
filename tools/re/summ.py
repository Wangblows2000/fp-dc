#!/usr/bin/env python3
"""One-screen summaries of annotated events (RE reading aid; output stays in build/re).
usage: summ.py build/re/Frames__event_func_N.a.c ..."""
import re, subprocess, sys
skip = re.compile(r'^\s*[{}]?\s*$|21a5d|21a42|iVar|lVar\d+ = \*\(long|plVar|unaff|in_R|^\s+return;|while \(|^\s*do\s*\{|lVar\d+ = lVar|\(lVar\d+ \+ 8\)|^\s*else\s*\{?$|== 0\) goto|code_r0x')
args = sys.argv[1:]; extra = []; lim = 900
if '--members' in args:
    i = args.index('--members'); extra = ['--members', args[i + 1]]; del args[i:i + 2]
if '--lim' in args:
    i = args.index('--lim'); lim = int(args[i + 1]); del args[i:i + 2]
for f in args:
    out = subprocess.run(['python3', 'tools/re/compact.py'] + extra + [f], capture_output=True, text=True).stdout
    lines = []
    for l in out.splitlines():
        if skip.search(l): continue
        if re.search(r'UniformGrid|puVar\d+\[|CONCAT44|>> 8;|\+ 0xff;|\(int\)\(uVar\d+ - 1\)|uVar\d+ = uVar\d+;|uVar\d+ = \*\(uint|uVar\d+ = 0;|uVar\d+ = 1;|puVar\d+ = \(uint|if \(\(int\)uVar|if \(-1 < \(int\)uVar|if \(0 < \(int\)uVar|\*puVar\d+ = \*puVar\d+ & 0xfffffcff|_ZdlPv|func_0x007fc[68]70|_Znwm|throw_bad|0x3fffffffffffffff|uStack|lStack|Stack_', l): continue
        l = re.sub(r'_UNK_[0-9a-f]+\{=', '{=', l)
        l = l.replace('*(double *)', '').replace('(undefined8 *)', '').replace('*(undefined8 *)', '')
        l = re.sub(r'_Z13check_overlapR10ObjectListR13QualifierList', 'OVLQ', l)
        l = re.sub(r'_Z13check_overlapR10ObjectListS0_', 'OVL', l)
        l = re.sub(r'\s+', ' ', l).strip()
        lines.append(l)
    print(' | '.join(lines)[:lim])
