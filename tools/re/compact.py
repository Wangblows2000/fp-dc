#!/usr/bin/env python3
"""Compact annotated Ghidra output (*.a.c) for reading: names Frames members, drops declarations and
selection-list boilerplate, collapses the group-active checks. usage: compact.py file.a.c [...]"""
import re, sys
NAMES = {0x3730: 'MV', 0x470: 'CTRL', 0x43b0: 'PAV_A', 0x43f0: 'PLAYER', 0x50f0: 'PAV_B', 0x4330: 'GROUPS_B',
         0x4370: 'ANGLECHG', 0x4430: 'S_LEFT', 0x4470: 'S_LEFTBIG', 0x44b0: 'S_RIGHT', 0x44f0: 'S_RIGHTBIG',
         0x4530: 'S_TOP', 0x4570: 'S_TOPBIG', 0x45b0: 'S_BOTTOM', 0x45f0: 'S_BOTTOMBIG', 0x4630: 'S_SLOPES',
         0x4670: 'S_PLATFORM', 0x46b0: 'S_LANGLE', 0x46f0: 'S_RANGLE', 0x4730: 'S_LEDGE', 0x4770: 'S_REDGE',
         0x47b0: 'SKIN', 0x1b70: 'OTHERS', 0x5670: 'CAM', 0x213a0: 'GLOBALS', 0x218d8: 'Q_OBSTACLE',
         0x218c0: 'Q_218c0', 0x219f8: 'Q_219f8', 0x22040: 'Q_HANG', 0x42f0: 'GROUPS_A'}
for fn in sys.argv[1:]:
    s = open(fn).read()
    s = s[s.find('{'):] if '{' in s else s
    for off, nm in NAMES.items():
        s = s.replace('*(long *)(*(long *)(param_1 + 0x%x) + 0x20)' % off, nm + '.alt')
        s = s.replace('*(long *)(param_1 + 0x%x)' % off, nm)
        s = s.replace('(param_1 + 0x%x)' % off, '&' + nm)
    s = re.sub(r"\(\*\(char \*\)\(param_1 \+ 0x21a[0-9a-f]{2}\) != '\\0'\)( && )?", '', s)
    out = []
    for l in s.split('\n'):
        st = l.strip()
        if re.match(r'^(undefined\d?|int|long|double|char|bool|byte|uint|ulong|ushort|float|code|short)\s+\*?\w+(\s*\[\d+\])?;$', st): continue
        if re.search(r'iVar\d+ = iVar\d+ \+ 1;|lVar\d+ = lVar\d+ \+ 0x10;|\(lVar\d+ \+ 0x18\) = iVar\d+;', st): continue
        if st in ('', 'do {', '} while (iVar%s != iVar%s);' % ('', '')): continue
        out.append(l)
    print('==== ' + fn.split('__')[-1][:-4])
    print('\n'.join(out))
