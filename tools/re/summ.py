#!/usr/bin/env python3
"""One-screen summaries of annotated events (RE reading aid; output stays in build/re).
usage: summ.py build/re/Frames__event_func_N.a.c ..."""
import re, subprocess, sys
skip = re.compile(r'^\s*[{}]?\s*$|21a5d|21a42|iVar|lVar\d+ = \*\(long|plVar|unaff|in_R|^\s+return;|while \(|^\s*do\s*\{|lVar\d+ = lVar|\(lVar\d+ \+ 8\)|^\s*else\s*\{?$|== 0\) goto|code_r0x')
for f in sys.argv[1:]:
    out = subprocess.run(['python3', 'tools/re/compact.py', f], capture_output=True, text=True).stdout
    lines = []
    for l in out.splitlines():
        if skip.search(l): continue
        l = re.sub(r'_UNK_[0-9a-f]+\{=', '{=', l)
        l = l.replace('*(double *)', '').replace('(undefined8 *)', '').replace('*(undefined8 *)', '')
        l = re.sub(r'_Z13check_overlapR10ObjectListR13QualifierList', 'OVLQ', l)
        l = re.sub(r'_Z13check_overlapR10ObjectListS0_', 'OVL', l)
        l = re.sub(r'\s+', ' ', l).strip()
        lines.append(l)
    print(' | '.join(lines)[:900])
