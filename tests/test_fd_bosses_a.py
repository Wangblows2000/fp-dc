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

# ======================================================================== fd1b: the morpher in the end tunnel (spec 2)
# From the tunnel start (X 16300 -> 200): every 30 frames next to the head (TPO, the fights' real-attack convention) and two attacks
# (X; Milla B, her shield burst). Forms in the order "234" (B A C), head A 80 + 3 x 50 (D 4), sound 47 at each D step, the defeat
# timeline (38, 76) and results_begin_boss (the morpher state 101 / the results).
HEAD = 2920 + 14
def morpher_script(atk):
    s = '20 TP 16300 2090\n22 60 R\n'
    for f in range(150, 3200, 30): s += '%d TPO %d -30 0\n%d %d %s\n%d %d %s\n' % (f, HEAD, f + 1, f + 4, atk, f + 12, f + 15, atk)
    return s + INV
for ch, nm in CHARS:
    rows, ev, d = run('fd1b', ch, 3200, morpher_script('B' if ch == 4 else 'X'))
    m = d.get('fdm', {})
    js = [m[f]['J'] for f in sorted(m)]
    order = [j for i, j in enumerate(js) if j not in ('0', '1') and (i == 0 or js[i - 1] != j)]
    check('fd1b %s: the morpher starts with the forms 2 3 (B A of "234"), the later strings bring D G E (5 7 6: "5234" "752" "62754")' % nm,
          order[:2] == ['2', '3'] and {'5', '6', '7'} <= set(order), str(order))
    fd = first(m, lambda r: r.get('def') == '1')
    hp0 = num(m[min(m)], 'hp') if m else 0
    check('fd1b %s: head HP 230 (80 + 3 x 50) and real attacks defeat it' % nm, hp0 == 230 and fd is not None, '%s %s' % (hp0, fd))
    s47 = sids(ev, 47)
    check('fd1b %s: sound 47 at each of the 4 head kills (RE 15488 / RV)' % nm, len(s47) == 4, str(s47))
    if fd:
        s38, s76 = [f for f in sids(ev, 38) if f > fd], [f for f in sids(ev, 76) if f > fd]
        check('fd1b %s: the defeat: 38 (parts slide) then 76 (white fade) 120 frames apart (RV m2_l P 180 / 300)' % nm,
              len(s38) >= 1 and len(s76) >= 1 and 110 <= s76[0] - s38[0] <= 130, '%s %s' % (s38[:2], s76[:2]))
        fr = first(m, lambda r: r.get('def') == '1' and num(r, 'P') > 500)
        res = [i for i, r in enumerate(rows) if r.get('res', 0) > 0]
        check('fd1b %s: results_begin_boss after the defeat (the results start)' % nm, fr is not None and res and res[0] > fd, '%s %s' % (fr, res[:1]))
        hit = [f for f in sorted(m) if f < fd and num(m[f], 'hp') < hp0]
        check('fd1b %s: the head loses HP before the defeat (attacks land)' % nm, len(hit) > 0)

# ======================================================================== fd2a: the RoboSquid (spec 3; TB3's class, tb_boss_squid.c)
# RV q2_l (Lilac at 5504 946): the squid leaves its spot 5504 for 5821 and bobs there (y 778-787). Fight: next to it every 24 frames
# (TPO) with two attacks (Milla B): first defeat -> the key card (134), alive again fleeing (O 2), second defeat -> 18 explosions, 38.
def squid_script(atk):
    s = '200 TP 5504 946\n'
    for f in range(230, 2000, 24): s += '%d TPO 1975 -30 0\n%d %d %s\n%d %d %s\n' % (f, f + 1, f + 4, atk, f + 12, f + 15, atk)
    return s + INV
rows, ev, d = run('fd2a', 0, 1000, '200 TP 5504 946\n' + INV)
q = d.get('tbq', {})
late = [q[f] for f in q if f > 500]
check('fd2a: the squid moves from 5504 to 5821 and bobs there (RV q2_l)', late and all(num(r, 'x') == 5821 and 776 <= num(r, 'y') <= 789 for r in late),
      str(late and (late[0].get('x'), late[0].get('y'))))
for ch, nm in CHARS:
    rows, ev, d = run('fd2a', ch, 2000, squid_script('B' if ch == 4 else 'X'))
    q = d.get('tbq', {})
    check('fd2a %s: real attacks: first defeat drops the card (134), it flees (O 2)' % nm, len(sids(ev, 134)) == 1 and first(q, lambda r: r['O'] == '2') is not None,
          '%s' % sids(ev, 134))
    check('fd2a %s: second defeat destroys it (38, R 1)' % nm, len(sids(ev, 38)) == 1 and q and q[max(q)]['R'] == '1', '%s' % sids(ev, 38))

# ======================================================================== fd2b: the mech Serpentine (spec 4)
# RV s2_l (Lilac at 7600 274 next to the NPC at 7772 260): the trigger, 373 (the transformation), 374 x 17, 67 + m_boss6 (slot 4), 354
# (the ledge breaks: the fall down the shaft to the arena, y 4992); the fight from RV ~1150. Real attacks next to the head every 30 frames
# (TPO; Milla B): 5 lives of 40 (the last + 20, m_boss6b), 47 at each, the defeat (60 / 61), results_begin_boss (162, the results).
HEADS = 2941
def serp_script(atk):
    s = '60 TP 7600 274\n'
    for f in range(1150, 4400, 30): s += '%d TPO %d -30 0\n%d %d %s\n%d %d %s\n' % (f, HEADS, f + 1, f + 4, atk, f + 12, f + 15, atk)
    return s + INV
for ch, nm in CHARS:
    rows, ev, d = run('fd2b', ch, 4400, serp_script('B' if ch == 4 else 'X'), {'FP_TRACE_SND': '1'})
    s = d.get('fds', {})
    mus = [(f, l.split()[1]) for f, l in ev if l.startswith('music ') and len(l.split()) > 1]
    s373, s374, s354 = sids(ev, 373), sids(ev, 374), sids(ev, 354)
    check('fd2b %s: the transformation: 373 once, 374 x 17 every 20 frames, then m_boss6 (RE 15688) and 354 (the ledge breaks)' % nm,
          len(s373) == 1 and len(s374) == 17 and all(b - a == 20 for a, b in zip(s374, s374[1:])) and len(s354) == 1
          and any(m.endswith('m_boss6.adp') and f < s354[0] for f, m in mus), '%s %s %s %s' % (s373, s374[:2], s354, mus[:3]))
    fl = first(s, lambda r: r.get('st') == '0' and num(r, 'py') > 4900)
    check('fd2b %s: the player falls down the shaft to the arena and the fight starts (RV ~1150)' % nm, fl is not None and 1000 < fl < 1250, str(fl))
    seen = {r.get('st') for r in s.values()}
    check('fd2b %s: the dash (1-3), the shots (4 / 5) and the jump (6-8) all happen' % nm, {'1', '2', '3', '4', '5', '6', '7', '8'} <= seen, str(sorted(seen)))
    s47 = sids(ev, 47)
    check('fd2b %s: real attacks take the 5 lives (sound 47 x 5) and the last plays m_boss6b (RE 15689)' % nm,
          len(s47) == 5 and any(m.endswith('m_boss6b.adp') for f, m in mus), '%s %s' % (s47, [m for f, m in mus]))
    fd = first(s, lambda r: r.get('st') == '60')
    fr = first(s, lambda r: r.get('st') == '162')
    res = [i for i, r in enumerate(rows) if r.get('res', 0) > 0]
    check('fd2b %s: the defeat (D 60, 61) then results_begin_boss (the results start)' % nm, fd is not None and fr is not None and fr > fd and res and res[0] >= fr - 1,
          '%s %s %s' % (fd, fr, res[:1]))

if fails: print('%d FAILED' % fails)
else: print('ALL PASS')
sys.exit(1 if fails else 0)
