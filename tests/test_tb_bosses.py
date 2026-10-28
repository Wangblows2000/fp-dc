#!/usr/bin/env python3
"""Host tests for the Thermal Base bosses (session 21c; src/game/tb_boss.c and the encounter files tb_boss_*.c;
build/re/spec_tb_bosses.md). Each encounter from entry to defeat to progression, for Lilac, Carol and Milla, against the original's
traces (RV numbers below, build/orig_traces/tb_bosses: s1_l the RoboShrimp, w1_l the wall pusher, p1_l the Syntax Spider, q1_l the
RoboSquid, x1_l the TB6 arena). The port's scripts teleport at 59 for the trace's TP at 60: port frame = RV frame - 2 (the shrimp's
path is frame-exact under that offset).
usage: test_tb_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, csv
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
TR = os.path.join(HERE, '..', 'build', 'orig_traces', 'tb_bosses')
DBG = ('tbs', 'tbw', 'tbp', 'tbq', 'tbx', 'tbl', 'tbg')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACK': '1', 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_TRACE_HUD': '1'})
    e.update({'FP_DBG_' + d.upper(): '1' for d in DBG}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('sid', 'music', 'voice', 'hud'):
            ev.append((len(rows), line))                     # (the frame the event happened in)
        elif t[0] in DBG and len(t) > 2:
            dbg.setdefault(t[0], {})[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, dbg

def sids(ev, *ids):
    out = []
    for f, l in ev:
        if l.startswith('sid ') and int(l.split()[1]) in ids and (f, int(l.split()[1])) not in out: out.append((f, int(l.split()[1])))
    return [f for f, _ in out]
def music(ev, name): return [f for f, l in ev if l.startswith('music') and name in l]
def near(a, b, tol=1): return len(a) == len(b) and all(abs(x - y) <= tol for x, y in zip(a, b))
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)
def rv_rows(name):
    p = os.path.join(TR, name)
    return {int(r['frame']): r for r in csv.DictReader(open(p))} if os.path.exists(p) else {}
def rv_snd(name, sid):
    p = os.path.join(TR, name)
    return [int(l.split()[0]) for l in open(p) if len(l.split()) > 2 and l.split()[1] == 'id' and int(l.split()[2]) == sid] if os.path.exists(p) else []

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 300, 600, 900, 1200, 1500, 1800, 2100))

def num(d, k, dflt=0.0):
    try: return float(d.get(k, dflt))
    except (TypeError, ValueError): return dflt

# ======================================================================== TB1: the RoboShrimp (spec Part A)
# RV s1_l: Lilac TP 10120 1890 at 60; segments A 0 at 700-705 (sound 23 + 45 each), the head A 0 at 950 (23), 15 explosions every
# 7 frames 952-1043, sound 38 at 1050 (the tail gone), the railings' crystals (40) at 1091. Port: TP at 59, segments at 698, head at 948.
SH = '3 SETV others E 99999\n5 TP 1500 2100\n59 TP 10120 1890\n698 BOSSHPT 1969 0\n948 BOSSHPT 1968 0\n' + INV
rv = rv_rows('s1_l.csv')
for ch, nm in CHARS:
    rows, ev, d = run('tb1', ch, 1150, SH)
    s = d.get('tbs', {})
    f1 = first(s, lambda r: r['st'] == '1')
    check('tb1 %s: the fight starts at the shaft (state 1)' % nm, f1 is not None and 59 <= f1 <= 120, str(f1))
    if ch == 0 and rv:
        bad = [f for f in range(62, 690) if f - 2 in s and f in rv and (num(s[f - 2], 'x') != float(rv[f]['shrimp_x']) or num(s[f - 2], 'y') != float(rv[f]['shrimp_y']))]
        check('tb1 lilac: the head path is frame-exact vs RV s1_l (62-690, port = RV - 2)', not bad, 'first %s' % (bad[:1],))
    s45 = [f for f, l in ev if l.split()[:2] == ['sid', '45']]
    check('tb1 %s: six segments down, sound 45 each' % nm, len(s45) == 6, str(s45[:8]))
    ex = sids(ev, 311, 319, 320)
    exs = [f for f in ex if 948 <= f <= 1060]
    check('tb1 %s: 15 explosions every 7 frames after the head (RV 952-1043)' % nm, len(exs) == 15 and all(b - a == 7 for a, b in zip(exs, exs[1:])), str(exs[:3]))
    s38, s40 = sids(ev, 38), sids(ev, 40)
    check('tb1 %s: sound 38 then the crystals (40) ~41 frames later (RV 1050 / 1091)' % nm,
          len(s38) >= 1 and len(s40) == 1 and 1040 <= s38[0] <= 1060 and 38 <= s40[0] - s38[0] <= 44, '%s %s' % (s38, s40))
    check('tb1 %s: the arena is open (state 4)' % nm, s and s[max(s)]['st'] == '4', s[max(s)]['st'] if s else '-')

# ======================================================================== TB2: the wall pusher (spec Part B)
# Real hits: the player walks right and attacks every 16 frames; each deflected hit (C > 3) pushes the wall right. The barrier reaches
# the switch (49), the robot explodes (29 + 38), the switch's destroyers blow the blocks (38), the railings turn to crystals (40).
W3 = open(os.path.join(HERE, 'data', 'tb2_push.txt')).read() if os.path.exists(os.path.join(HERE, 'data', 'tb2_push.txt')) else None
if W3 is None:
    W3 = '3 SETV others E 99999\n5 TP 6300 3560\n50 2400 R\n' + ''.join('%d %d X\n' % (f, f + 4) for f in range(150, 2400, 16)) + INV
def milla_push():   # Milla deflects with the shield burst (B), released between pushes (R 18 / B 10)
    s, f = '3 SETV others E 99999\n5 TP 6300 3560\n50 150 R\n', 150
    while f < 2600: s += '%d %d R\n%d %d B\n' % (f, f + 17, f + 18, f + 27); f += 30
    return s + INV
for ch, nm in CHARS:
    rows, ev, d = run('tb2', ch, 2600, milla_push() if ch == 4 else W3)
    w = d.get('tbw', {})
    f1 = first(w, lambda r: r['C'] == '1')
    check('tb2 %s: the trapdoor closes behind the player (C 1)' % nm, f1 is not None and f1 < 120, str(f1))
    pushed = first(w, lambda r: num(r, 'D') >= 9.5)
    check('tb2 %s: a deflected hit pushes the wall back (D 10)' % nm, pushed is not None, str(pushed))
    f2 = first(w, lambda r: r['C'] in ('2', '3'))
    s49, s29 = sids(ev, 49), sids(ev, 29)
    check('tb2 %s: the barrier presses the switch (49) and the robot explodes 3 frames later (29; RV 344 / 347)' % nm,
          f2 is not None and len(s49) == 1 and len(s29) == 1 and s29[0] - s49[0] == 3, '%s %s %s' % (f2, s49, s29))
    check('tb2 %s: the switch counts as pressed and the blocks are lit (38 x2)' % nm,
          w and w[max(w)].get('presses') == '1' and len([f for f, l in ev if l.split()[:2] == ['sid', '38'] and s49 and f >= s49[0]]) >= 2, w[max(w)].get('presses') if w else '-')
    f3 = first(w, lambda r: r['C'] == '3')
    check('tb2 %s: the room opens (C 3, crystals 40)' % nm, f3 is not None and len(sids(ev, 40)) == 1, str(f3))

# ======================================================================== TB3: the RoboSquid (spec Part D)
# RV q3_l: Lilac at 3900 2080; A 0 at 300 (sounds 23 + 134, the card), alive again at 361 (A 30, fleeing, U 4 at each turn: shots
# every 15 frames), A 0 at 800: 18 explosions every 7 frames, sound 38 at 920 (destroyed). The card falls once in view.
SQ = '60 TP 3900 2080\n300 BOSSHPT 1975 0\n318 TP 4192 2080\n800 BOSSHPT 1975 0\n' + INV
for ch, nm in CHARS:
    rows, ev, d = run('tb3', ch, 1000, SQ)
    q = d.get('tbq', {})
    check('tb3 %s: the squid swims (bob around K 1959)' % nm, q and 1950 <= min(num(r, 'y') for r in q.values()) and max(num(r, 'y') for r in q.values()) <= 1965)
    s134, s23 = sids(ev, 134), sids(ev, 23)
    check('tb3 %s: first defeat drops the key card (134; RV 300)' % nm, len(s134) == 1 and 300 <= s134[0] <= 302, str(s134))
    o2 = first(q, lambda r: r['O'] == '2')
    check('tb3 %s: alive again after 60 frames, fleeing (O 2; RV 361)' % nm, o2 is not None and 358 <= o2 <= 364, str(o2))
    turns = [f for f in sorted(q) if f > 1 and f - 1 in q and q[f]['N'] != q[f - 1]['N']]
    xs = [num(q[f], 'x') for f in turns]
    check('tb3 %s: it turns 240 px either side of its start (RV 4436 / 3948)' % nm, turns and all(abs(x - 4192) >= 230 for x in xs), str(list(zip(turns, xs))[:4]))
    shots = [f for f in sids(ev, 28) if f > 361]
    check('tb3 %s: fleeing, it shoots every 15 frames (sound 28)' % nm, len(shots) >= 3 and any(b - a == 15 for a, b in zip(shots, shots[1:])), str(shots[:6]))
    ex = [f for f in sids(ev, 311, 319, 320) if 800 <= f <= 940]
    s38 = sids(ev, 38)
    check('tb3 %s: second defeat: 18 explosions every 7 frames then 38 (RV 801-913, 920)' % nm,
          len(ex) == 18 and len(s38) == 1 and s38[0] > ex[-1] and q[max(q)]['R'] == '1', '%d %s %s R %s' % (len(ex), ex[-2:], s38, q[max(q)]['R']))
    g = d.get('tbg', {})
    keys = max((int(r.get('keys', 0)) for r in g.values()), default=-1)
    check('tb3 %s: the dropped card is picked up (keys 1)' % nm, keys >= 1, str(keys))

# ======================================================================== TB5: the end elevator (spec Part F)
for ch, nm in CHARS:
    rows, ev, d = run('tb5', ch, 900, '5 TP 14950 2300\n40 120 R\n' + INV)
    e = d.get('tbl', {})
    c1, c2 = first(e, lambda r: r['C'] == '1'), first(e, lambda r: r['C'] == '2')
    check('tb5 %s: the ride starts past the left limit + 96 and carries the player up (3 px a frame)' % nm,
          c1 is not None and c2 is not None and all(num(e[f], 'on') >= 0 for f in range(c1 + 30, c2) if f in e), '%s %s' % (c1, c2))
    span = [num(e[f], 'ly') for f in (c1 + 10, c1 + 20)] if c1 is not None and c1 + 20 in e else []
    check('tb5 %s: the lifts rise 3 px a frame' % nm, len(span) == 2 and span[0] - span[1] == 30, str(span))
    check('tb5 %s: at the top: sound 51, the player at the trigger (y 424)' % nm, sids(ev, 51) and rows and abs(rows[-1]['y'] - 424) < 2, '%s %s' % (sids(ev, 51), rows[-1]['y'] if rows else '-'))

# ======================================================================== TB6: the Syntax squid arena (spec Part E)
# RV x2_l: the hitbox's A set to 0 at 700 / 1300 / 1900 (port 698 / 1298 / 1898): stuns (47) at each, the end (309) at the third,
# 92 at 2171, the burst (50) at 2260, the results (m_clear) at 2381. Intro: sx_still at O 21 (RV 413), m_boss2 at O 120 (RV 512).
X5 = '3 SETV others E 99999\n60 400 R\n698 BOSSHPT 1993 0\n1298 BOSSHPT 1993 0\n1898 BOSSHPT 1993 0\n' + INV
for ch, nm in CHARS:
    rows, ev, d = run('tb6', ch, 2450, X5)
    x = d.get('tbx', {})
    m = music(ev, 'm_boss2')
    v = [f for f, l in ev if l.startswith('voice') and 'sx_still' in l]
    check('tb6 %s: the stage voice then slot-4 music in the intro (sx_still, m_boss2 ~99 frames later; RV 413 / 512)' % nm,
          len(v) == 1 and len(m) >= 1 and 95 <= m[0] - v[0] <= 101, '%s %s' % (v, m))
    s47 = sids(ev, 47)
    check('tb6 %s: three stuns (47)' % nm, len(s47) == 3, str(s47))
    s309, s92, s50 = sids(ev, 309), sids(ev, 92), sids(ev, 50)
    check('tb6 %s: the end: 309 with the third, 92 ~271 frames later, the burst 50 ~360 (RV 1900 / 2171 / 2260)' % nm,
          len(s309) == 1 and s92 and s50 and 265 <= s92[0] - s309[0] <= 275 and 355 <= s50[0] - s309[0] <= 365, '%s %s %s' % (s309, s92, s50))
    res = first(x, lambda r: r['res'] == '1')
    check('tb6 %s: the results ~481 frames after the end (RV 2381)' % nm, res is not None and s309 and 470 <= res - s309[0] <= 490, '%s' % res)
    check('tb6 %s: the HUD slid up 40 px' % nm, x and num(x[max(x)], 'hud') == 40)
# a real hit: the player on the platform under the hitbox (N 1 near 790), attacking
for ch, nm in CHARS:
    atk = 'B' if ch == 4 else 'X'
    rows, ev, d = run('tb6', ch, 900, '3 SETV others E 99999\n60 400 R\n790 TP 1430 1380\n' + ''.join('%d %d %s\n' % (f, f + 9, atk) for f in (791, 806, 821, 836)))
    x = d.get('tbx', {})
    a0, a1 = num(x.get(785, {}), 'hA', -1), min((num(r, 'hA', 99) for f, r in x.items() if 790 <= f <= 899), default=-1)
    check('tb6 %s: a real attack hurts the hitbox (A 50 -> lower)' % nm, a0 == 50 and 0 < a1 < 50, '%s %s' % (a0, a1))

# ======================================================================== TB3: the Syntax Spider and the lava chase (spec Part C)
# RV p5_l (Lilac on the left ledge, HP refilled): the trigger at 60 (284), the walker's steps K 1-16 (a cycle of 1654 frames), the
# stream / cannon sounds; RV p6_l / p7_l: A 0 at 400 -> 13 explosions, 38 at 487 (the parts fly off), 52 at 527 (the line), the
# spikes (71 every 6 frames, 28 per shot); A 0 at 1100 -> 310 at 1214 (the chase), 309 at 1394, K 1 at 1445, K 2 below y 4288.
SPK = {1: 189, 2: 288, 3: 426, 4: 552, 5: 629, 6: 764, 7: 903, 8: 932, 9: 990, 10: 1117, 11: 1267, 12: 1402, 13: 1479, 14: 1606, 15: 1756, 16: 1785}
P5 = '3 SETV others E 99999\n59 TP 2860 3990\n' + ''.join('%d HP 14\n' % f for f in range(80, 1900, 20))
rows, ev, d = run('tb3', 0, 1900, P5)
sp = d.get('tbp', {})
ks = {}
for f in sorted(sp):
    k = int(num(sp[f], 'K'))
    if sp[f]['ph'] == '3' and k not in ks: ks[k] = f
bad = [(k, ks.get(k), v) for k, v in SPK.items() if ks.get(k) is None or abs(ks[k] - v) > 6]
check('tb3 lilac: the walker steps K 1-16 within 6 frames of RV p5', not bad, str(bad[:4]))
s305, s303 = sids(ev, 305), sids(ev, 303)
check('tb3 lilac: the stream (305) and cannon (303) sounds alternate (RV 297 / 572 / 773 / 932 ...)',
      len(s305) >= 4 and len(s303) >= 4 and abs(s305[0] - 297) <= 3 and abs(s303[0] - 572) <= 3, '%s %s' % (s305[:4], s303[:4]))
check('tb3 lilac: the stream pieces end in the cannon / the falls (sound 55)', len([f for f, l in ev if l.split()[:2] == ['sid', '55']]) >= 60)
check('tb3 lilac: the stage voice when the walker lands (sx_lost) and the slot-4 music at the trigger',
      [l for f, l in ev if l.startswith('voice') and 'sx_lost' in l] and music(ev, 'm_midboss'), str(music(ev, 'm_midboss')))
def p9(ch):   # both defeats by BOSSHPT, then the run right (jumping)
    return ('3 SETV others E 99999\n59 TP 2860 3990\n398 BOSSHPT 1980 0\n1098 BOSSHPT 1980 0\n' + ''.join('%d HP 14\n' % f for f in range(80, 1440, 30)) +
            '1440 2600 R\n' + ''.join('%d %d J\n' % (f, f + 14) for f in range(1460, 2600, 40)))
for ch, nm in CHARS:
    rows, ev, d = run('tb3', ch, 2400, p9(ch))
    sp = d.get('tbp', {})
    hang = first(sp, lambda r: r['ph'] == '5')
    s38, s52 = sids(ev, 38), sids(ev, 52)
    check('tb3 %s: walker beaten: explosions, 38 and the hanging phase (RV 487), 52 (RV 527)' % nm,
          hang is not None and s38 and abs(s38[0] - 487) <= 3 and s52 and abs(s52[0] - 527) <= 3, '%s %s %s' % (hang, s38[:1], s52[:1]))
    s71 = sids(ev, 71)
    check('tb3 %s: the hanging mask drops spikes (71 every 6 frames; RV 587-629)' % nm, len(s71) >= 8 and all(b - a == 6 for a, b in zip(s71[:8], s71[1:8])), str(s71[:4]))
    s310, s309 = sids(ev, 310), sids(ev, 309)
    check('tb3 %s: the end: 310 (RV 1214), the wall comes (309; RV 1394)' % nm,
          s310 and abs(s310[0] - 1214) <= 6 and s309 and abs(s309[0] - 1394) <= 6, '%s %s' % (s310, s309))
    k1 = first(sp, lambda r: r['ph'] == '6' and r['K'] == '1')
    k2 = first(sp, lambda r: r['ph'] == '6' and r['K'] == '2')
    check('tb3 %s: the run starts (K 1; RV 1445) and ends in the room below (K 2: y > 4288)' % nm,
          k1 is not None and abs(k1 - 1445) <= 6 and k2 is not None and num(sp[k2], 'py') > 4288, '%s %s' % (k1, k2))
    check('tb3 %s: the player outruns the wall (alive, HP left)' % nm, rows and rows[-1].get('hp', 0) > 0 and rows[-1]['x'] > 7000, '%s %s' % (rows[-1].get('hp') if rows else '-', rows[-1]['x'] if rows else '-'))
# real hits on the walker's mask (standing at the right end in K 2)
for ch, nm in CHARS:
    atk = 'B' if ch == 4 else 'X'
    sc = '3 SETV others E 99999\n59 TP 2860 3990\n300 TP 3670 3980\n' + ''.join('%d %d %s\n' % (f, f + 9, atk) for f in range(302, 400, 14)) + ''.join('%d HP 14\n' % f for f in range(80, 400, 20))
    rows, ev, d = run('tb3', ch, 400, sc)
    sp = d.get('tbp', {})
    check('tb3 %s: real attacks hurt the mask (A 200 -> lower)' % nm, sp and num(sp[max(sp)], 'A') < 200, sp[max(sp)]['A'] if sp else '-')

print('%d failures' % fails)
sys.exit(1 if fails else 0)
