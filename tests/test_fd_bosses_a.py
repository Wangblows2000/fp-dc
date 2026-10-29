#!/usr/bin/env python3
"""Host tests for the Final Dreadnought round 1-2 bosses (session 24; src/game/fd_boss_r12.c, fd_boss.c; build/re/spec_fd_bosses_a.md):
fd1b's push-beam chase and the morpher in the act's end tunnel, fd2a's RoboSquid (TB3's class), fd2b's mech Serpentine. Lilac, Carol and
Milla; real attacks (host script letters X attack, B special, J jump) wherever a fight is tested to its end.
usage: test_fd_bosses_a.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))
DBG = ('fdb', 'fdm', 'fds', 'tbq')

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACE_SID': '1', 'FP_DBG_FDB': '1', 'FP_DBG_TBQ': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f' and len(t) > 2:
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('sid', 'music', 'voice'):
            ev.append((len(rows), line))
        elif t[0] in DBG and len(t) > 2:
            dbg.setdefault(t[0], {})[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, dbg

def sids(ev, sid): return [f for f, l in ev if l.split()[:2] == ['sid', str(sid)]]
def num(d, k, dflt=0.0):
    try: return float(d.get(k, dflt))
    except (TypeError, ValueError): return dflt
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)
fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
INV = ''.join('%d SETV others E 99999\n' % f for f in range(3, 6000, 250))
def keys(a, b, k, every=None, length=None):
    if every is None: return '%d %d %s\n' % (a, b, k)
    return ''.join('%d %d %s\n' % (f, f + length, k) for f in range(a, b, every))

# ======================================================================== fd1b: the push-beam chase (spec 1)
# From the start of the hangar corridor (x 12700, the act's own floor), holding right: the alarm (310) once X > 12976, the beam 18 frames
# later (169, alarm A 95 > 90), the beam pushes from behind and the run reaches the act's end tunnel (X > 16380 -> X 200) without a TP.
for ch, nm in CHARS:
    sc = '20 TP 12700 2090\n' + keys(22, 1500, 'R') + keys(40, 1500, 'J', 45, 20) + INV   # (jumps: the spark-tower pits, gimmicks)
    rows, ev, d = run('fd1b', ch, 1500, sc)
    b = d.get('fdb', {})
    s310, s169 = sids(ev, 310), sids(ev, 169)
    check('fd1b %s: alarm (310) then the beam (169) 18 frames later (RE 15387 / 15389)' % nm,
          len(s310) == 1 and len(s169) == 1 and s169[0] - s310[0] == 18, '%s %s' % (s310, s169))
    fa = first(b, lambda r: r.get('pb') == '1')
    check('fd1b %s: the alarm starts past the left marker + 120 (12976)' % nm, fa is not None and 12976 < num(b[fa], 'px') < 13100, str(fa and b[fa]['px']))
    ft = first(b, lambda r: r.get('pb') == '3')
    check('fd1b %s: holding right reaches the tunnel (X > 16380 -> X 200, RE 15401)' % nm, ft is not None and abs(num(b[ft], 'px') - 200) < 12,
          '%s %s' % (ft, ft and b[ft]['px']))
    beh = [f for f in b if b[f].get('pb') == '2' and num(b[f], 'px') < num(b[f], 'cx') + 10 - 1]
    check('fd1b %s: the player is never left behind the beam (X >= beam x + 10, RE 15391)' % nm, not beh, str(beh[:3]))
# standing still in the corridor: the beam pushes the player right at B >= 5 (RE 15391) and its sensor hurts (10, RE 15392)
rows, ev, d = run('fd1b', 0, 260, '20 TP 13000 2090\n')
b = d.get('fdb', {})
check('fd1b lilac: an idle player is pushed along by the beam (x grows, cannon 4 px a frame)',
      b and num(b[max(b)], 'px') > 13300 and num(b[max(b)], 'cx') - num(b[150], 'cx') == 4 * (max(b) - 150), '%s %s' % (b and b[max(b)].get('px'), b and b[max(b)].get('cx')))

if fails: print('%d FAILED' % fails)
else: print('ALL PASS')
sys.exit(1 if fails else 0)
