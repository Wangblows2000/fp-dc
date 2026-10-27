#!/usr/bin/env python3
"""Host test: level-scoped state does not leak across stage transitions (session 20).

Bug (user report): after Fortune Night the player sprite was not drawn in Sky Battalion, and from then on in every other
level and for every character until the console was restarted. Cause: fn_assist.c's state (AS) was only cleared when FN6's
Panther spawned, so the closing scene's cast_on (MV.M 50: the cast stands in, the player is not drawn) stayed 1 in every
later level. Fix: fn_assist_reset() on every level load (fn_panther_spawn).

Route per character (FP_TRACE_PDRAW prints game_player_visible() each frame): FN6 boss beaten (BOSSHP 0, invulnerable) ->
closing cast (player hidden, as in the original) -> LOAD sb1 -> LOAD dv1 -> LOAD rm1 -> LOAD fn6 again.
usage: test_transitions.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

ROUTE = [(2100, 'sb1'), (2500, 'dv1'), (2800, 'rm1'), (3100, 'fn6')]
FRAMES = 3400

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1


def run(ch):
    script = '5 SETV others E 99999\n' + ''.join('%d BOSSHP 0\n' % (300 + k * 70) for k in range(25))
    script += ''.join('%d LOAD %s\n' % (f, d) for f, d in ROUTE)
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_PDRAW': '1', 'FP_LEVEL': 'fn6', 'FP_CHAR': ch})
    r = subprocess.run([BIN, DATA, str(FRAMES), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows = []
    for line in r.stdout.splitlines():
        t = line.split()
        if t and t[0] == 'pdraw': rows.append((int(t[1]), int(t[2]), t[4]))
    return rows


for ch, nm in (('0', 'lilac'), ('2', 'carol'), ('4', 'milla')):
    rows = run(ch)
    fn6a = [v for f, v, lv in rows if f < ROUTE[0][0]]
    check('%s_fn6_closing_cast_hides_player' % nm, 0 in fn6a, 'hidden frames %d' % fn6a.count(0))
    for i, (f0, lv) in enumerate(ROUTE):
        f1 = ROUTE[i + 1][0] if i + 1 < len(ROUTE) else FRAMES
        seg = [(f, v, l) for f, v, l in rows if f0 + 1 <= f < f1]
        hidden = [f for f, v, l in seg if not v]
        check('%s_player_drawn_in_%s_after_fn6' % (nm, lv), seg and all(l == lv for f, v, l in seg) and not hidden,
              '%d frames, hidden %d (first %s)' % (len(seg), len(hidden), hidden[:1]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
