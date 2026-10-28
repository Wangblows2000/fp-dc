#!/usr/bin/env python3
"""Whole-game transition matrix (session 24): every stage's last act, ended through the shared boss results (RESBOSS -> the
results -> RESEND -> the pop-up pages confirmed), leads to the next stage of the Classic route for Lilac, Carol and Milla, with the
character kept. Expected routes are the original's (RE frame 8 Classic block, MenuSequence.I by g1; levels.c ROUTE comment):
  Lilac:        DV -> RM -> FN -> SB -> JC -> TB -> PL -> BG -> FD
  Carol, Milla: (Milla: AT ->) RM -> FN -> SB -> JC -> TH -> TB -> BG -> FD
(the shoot-'em-up lead-in to Battle Glacier, frame 84, is not ported: its end loads BG1). Final Dreadnought: the rounds chain
fd1a -> fd1b -> fd2a -> ... -> fd4b and FD4's end starts the ending (tests/test_fd_flow.py, tests/test_ending.py); here each
round's last act is checked to lead to the next round when the round ends through the results.
Also: no level is entered twice, the frame after the load the player is the same character.
usage: test_route_matrix.py [data_dir]   (default: converted2)"""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CONFIRM = ''.join('%d %d J\n' % (f, f + 2) for f in range(760, 1000, 40))
SCRIPT = '3 SETV others E 99999\n30 RESBOSS\n700 RESEND\n' + CONFIRM
fails = 0

def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

def run(level, ch, frames=1400):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(SCRIPT); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch)})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    loads, chr_after = [], None
    for l in r.stdout.splitlines():
        if l.startswith('load['): loads.append(l[5:l.index(']')])
        elif l.startswith('f ') and loads and loads[-1] not in ('boot', 'frontend', 'gal') and chr_after is None and len(loads) > 2:
            t = l.split()
            if 'chr' in t: chr_after = int(t[t.index('chr') + 1])
    return [x for x in loads if x not in ('boot', 'frontend', 'gal')], chr_after

def have(lv): return os.path.isdir(os.path.join(DATA, lv))

LILAC = {'dv4': 'rm1', 'rm5': 'fn1', 'fn6': 'sb1', 'sb4': 'jc1', 'jc5': 'tb1', 'tb6': 'pl1', 'pl6': 'bg1', 'bg6': 'fd1a'}
OTHERS = {'dv4': 'rm1', 'rm5': 'fn1', 'fn6': 'sb1', 'sb4': 'jc1', 'jc5': 'th1', 'th3': 'tb1', 'tb6': 'bg1', 'bg6': 'fd1a'}
FD = {'fd1b': 'fd2a', 'fd2b': 'fd3a', 'fd3b': 'fd4a'}
for ch, nm, table in ((0, 'lilac', LILAC), (2, 'carol', OTHERS), (4, 'milla', dict(OTHERS, at4='rm1'))):
    for lv, nx in list(table.items()) + list(FD.items()):
        if not have(lv) or not have(nx): print('SKIP %s_%s_to_%s (no data)' % (nm, lv, nx)); continue
        loads, c = run(lv, ch)
        check('%s_%s_to_%s' % (nm, lv, nx), loads[:2] == [lv, nx] and len(loads) == 2 and c == ch, '%s chr %s' % (loads, c))
print('%d failures' % fails)
sys.exit(1 if fails else 0)
