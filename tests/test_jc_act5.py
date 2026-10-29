#!/usr/bin/env python3
"""Host tests for Jade Creek act 5, the finale (session 21; src/game/jc_boss5.c + the JC5 variant of fn_boss.c's Serpentine;
build/re/spec_jc_boss5.md). Per character (Lilac, Carol, Milla) against the original's traces (RV numbers,
build/orig_traces/jc_boss5: l_kill / l_kill2 Lilac, c_serp / c_gun Carol, l_tp / c_tp / m_tp the act from its start):
 - the storm: the first bolt (sound 292) at frame 108 of every trace that teleports to x 5500 at 60 (the rain, 11611, fills up
   to 50 drops one a frame once (MV.X - 200 + GT) / 120 allows it; LevelDarkSky C counts while it has > 40: session 26), then
   every 115-415 frames (11618-11620);
 - Serpentine 2: voice serp_gotyounow and m_boss at his trigger, his fall (0x22af5) and the gunship 105 frames later
   (sound 427, voice serp_kungfu; RV l_kill2 1414 -> 1519, c_gun 1518 -> 1623);
 - the gunship's first state (troopers): the cable 165 / 166 at +90 / +136, the Elites. drops (233) at +176 / +275 (l_kill2, c_gun, m_gun2);
 - its health: 300 for Lilac, capped at 220 for Carol (RV c_gun 1624) and 50 for Milla (RV m_gun2 1522; 11340 / 11341);
 - its end (RV l_kill2, defeat at 2997): F 70 -> the slow motion (GUIPause B 6: F 1000 -> 1060 in 360 frames), the flash 76 at
   +568, the closing (player hidden) and the results at +751; Milla's life-petal box for Lilac / Carol.
usage: test_jc_act5.py [data_dir]   (default: cv, the Jade Creek build)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': 'jc5', 'FP_CHAR': str(char), 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_DBG_J5': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg = 0, [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows += 1
        elif t[0] in ('sid', 'music', 'voice'): ev.append((rows, line))      # (the frame the event happened in)
        elif t[0] == 'j5' and len(t) > 2: dbg[rows] = {t[i]: float(t[i + 1]) for i in range(1, len(t) - 1, 2)}
    return ev, dbg

def sids(ev, *ids): return [f for f, l in ev if l.startswith('sid ') and int(l.split()[1]) in ids]
def voices(ev, name): return [f for f, l in ev if l.startswith('voice ' + name)]
def music(ev, name): return [f for f, l in ev if l.startswith('music ' + name)]
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)
def near(a, b, tol): return a is not None and b is not None and abs(a - b) <= tol

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))
INV = ''.join('%d SETV others E 99999\n' % f for f in range(3, 6000, 300))
def fight(ch, end_attacks, extra=''):
    """TP into the arena, walk to the trigger, the Serpentine's hitbox (822) at 1 from 280; Lilac / Carol: jump attacks while
    walking left / right; Milla: the Phantom Cube (attack held 65 frames) thrown left / right"""
    L = ['60 TP 13300 560', '70 %d R' % (137 if ch == 4 else 140), '280 BOSSHPT 822 1']   # (Milla: her cube throws need her 3 px further left since JC5's start stands on the subtanker)
    if ch == 4:
        for k, f in enumerate(range(300, end_attacks, 130)): L += ['%d %d %s' % (f, f + 20, 'LR'[k & 1]), '%d %d X' % (f + 25, f + 90), '%d %d X' % (f + 95, f + 97)]
    else:
        for f in range(300, end_attacks, 36): L += ['%d %d J' % (f, f + 8), '%d %d X' % (f + 12, f + 14), '%d %d X' % (f + 22, f + 24)]
        for k, f in enumerate(range(300, end_attacks, 100)): L.append('%d %d %s' % (f, f + 100, 'LR'[k & 1]))
    return INV + '\n'.join(L) + '\n' + extra

# ---- the storm (11611-11620): the first bolt at 108 (RV: every trace, all of them teleported to x 5500 at 60), then 115-415
# frames apart (session 26: the bolt waits for the rain, which a player standing at the start never gets: test_gimmicks_m.py)
for ch, cn in CHARS:
    ev, d = run(ch, 1500, INV + '60 TP 5500 840\n')
    t = sids(ev, 292)
    gaps = [b - a for a, b in zip(t, t[1:])]
    check('storm_%s' % cn, t and near(t[0], 108, 2) and len(t) >= 3 and all(110 <= g <= 420 for g in gaps), str(t))

for ch, cn in CHARS:
    # ---- Serpentine 2 and the gunship's first state (no hits on the gunship: attacks stop at 1000)
    ev, d = run(ch, 1900 if ch != 4 else 2400, fight(ch, 1000 if ch != 4 else 1350))   # (Milla: the rain's draws on the shared
                                                                                         # random stream (session 26) move his fall to ~1310)
    trig = first(d, lambda r: r['s'] >= 2)
    mb = [f for f, l in ev if l.startswith('music ') and f >= trig] if trig else []   # m_boss = the level row's slot 4 (levels.c, the flow's)
    check('serp_%s_trigger' % cn, trig and voices(ev, 'serp_gotyounow')[:1] == [trig] and (mb[:1] == [trig] or not mb) and
          not any(l.startswith('music stop') for f, l in ev if trig <= f < trig + 900), str((trig, voices(ev, 'serp_gotyounow'), mb[:2])))
    fall = first(d, lambda r: r['s'] >= 3)
    start = sids(ev, 427)[:1]
    check('serp_%s_fall_to_gunship' % cn, fall and start and near(start[0] - fall, 105, 3) and voices(ev, 'serp_kungfu')[:1] == start,
          str((fall, start, voices(ev, 'serp_kungfu'))))
    s0 = start[0] if start else 0
    rel = lambda ids: [f - s0 for f in sids(ev, *ids) if f > s0][:2]
    check('gunship_%s_troopers' % cn, near((rel((165,)) or [None])[0], 90, 2) and near((rel((166,)) or [None])[0], 136, 2)
          and len(rel((233,))) == 2 and near(rel((233,))[0], 176, 2) and near(rel((233,))[1], 275, 2), str((rel((165,)), rel((166,)), rel((233,)))))
    hp = d.get(s0 + 2, {}).get('A')
    check('gunship_%s_health' % cn, hp == {0: 300, 2: 220, 4: 50}[ch], str(hp))
    els = max((r['el'] for f, r in d.items() if f > s0), default=0)
    check('gunship_%s_elites' % cn, els >= 1, str(els))

# ---- the end (Lilac; RV l_kill2): the gunship's health 1 once it is up, then hits
ev, d = run(0, 3200, fight(0, 2400, '1100 BOSSHPT 1485 1\n'))
# (a 'j5' line shows the state at the start of a gameplay frame: what frame f set shows at f + 1, at f + 6 in the slow motion)
dfn = first(d, lambda r: r['F'] >= 1); dfn = dfn and dfn - 1                    # the defeat frame (F 1)
s1000 = first(d, lambda r: r['F'] >= 1000); s1000 = s1000 and s1000 - 6         # F 70 -> 1000 (and GUIPause B 6) in this frame
s1060 = first(d, lambda r: r['F'] >= 1060); s1060 = s1060 and s1060 - 1         # F 1060: B 0
check('end_defeat_sounds', dfn and sids(ev, 23, 38, 134).count(dfn) == 3 and d[dfn + 2]['hd'] == -1, str([f for f in sids(ev, 23, 38, 134) if f >= (dfn or 0) - 3][:4]))
check('end_finisher_at_70', dfn and near(s1000 - dfn, 69, 0), str((dfn, s1000)))
check('end_slow_motion', s1000 and s1060 and near(s1060 - s1000, 360, 0) and d[s1000 + 6]['sm'] == 6 and d.get(s1060 + 1, {}).get('sm') == 0,
      str((s1000, s1060)))
booms = [f for f in sids(ev, 98, 99, 311) if s1000 and s1000 <= f < s1060]
check('end_slow_explosions', len(booms) >= 55 and all(b - a == 6 for a, b in zip(booms, booms[1:])), str(booms[:4]))
check('end_flash', dfn and near((sids(ev, 76) or [0])[0] - dfn, 568, 2), str((dfn, sids(ev, 76))))
cl = first(d, lambda r: r['cl'] == 1)
res = first(d, lambda r: r['res'] == 1)
check('end_closing_results', dfn and cl and res and near(res - 1 - dfn, 751, 2) and near(cl - 1 - dfn, 751 - 163, 2), str((dfn, cl, res)))
box = first(d, lambda r: r['box'] == 1)
check('milla_box_lilac', box is not None and box < dfn, str(box))
ev, d = run(4, 1900, fight(4, 1400, '1100 BOSSHPT 1485 1\n'))
check('milla_box_not_milla', all(r['box'] == 0 for r in d.values()))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
