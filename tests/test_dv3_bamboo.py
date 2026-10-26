#!/usr/bin/env python3
"""Host tests for Dragon Valley 3's bamboo spike traps (session 19; src/game/gimmicks.c bamboo_update, RE events 5227-5234 /
5319, RV original traces of set_frame 22, RE_NOTES "Session 19: DV3 spike ball, RM Milla background").
Pivot GimmickBambooSpike (OT_BAM1 110), joint ...2 (111, hurts), pole ...3 (112), head ...4 (113), three sets at x 8512 /
8848 / 9040. Original trace rows are mapped to host frames by +120 (the trace starts 120 frames after set_frame; host TP
lands 2-4 frames earlier).
usage: test_dv3_bamboo.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
B1, B2, B3, B4, MON_METAL = 110, 111, 112, 113, 154
SETS = (8512, 8848, 9040)

def run(script, frames, ch=0, track=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': 'dv3', 'FP_CHAR': str(ch)})
    if track: e['FP_TRACK'] = track
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs = [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            d.update({t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2) if t[i] != 'fl'})
            obs[(d['f'], d['type'], d['x0'])] = d
    return rows, obs

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def hurts(rows):   # frames where the petals drop
    return [int(rows[i]['f']) for i in range(1, len(rows)) if rows[i]['hp'] < rows[i - 1]['hp']]
def tip(ang):      # RV action-point offset of the pole image at Active angle ang (s19 trace, 29 angles)
    a = math.radians(ang); c, s = math.cos(a), math.sin(a)
    return (int(8 * c - 5 * s) - int(8 * c + 52 * s), int(-8 * s - 5 * c) - int(-8 * s + 52 * c))

# RV samples of the joint offset from the pivot (original trace dv3_bam_idle, previous-frame pivot angle -> joint)
RV = {0: (0, -57), 5: (-5, -56), 10: (-9, -55), 25: (-24, -50), 45: (-40, -40), 70: (-52, -19), 290: (53, -20),
      315: (40, -40), 340: (19, -52), 355: (5, -56)}
check('bamboo_action_point_formula_matches_rv', all(tip(a) == v for a, v in RV.items()), str({a: tip(a) for a in RV}))

# ---- the parts move together (5227): joint / pole on the pivot's action point (last frame's angle), pole angle =
# -(pivot angle), head on the pole's action point; all four parts of every set
track = ','.join('%d:%d' % (t, x) for t in (B1, B2, B3, B4) for x in SETS)
rows, obs = run('# no input', 420, track=track)
bad = []; xs = {x: [] for x in SETS}
for x in SETS:
    for f in range(2, 420):
        p0, p1 = obs.get((f - 1, B1, x)), obs.get((f, B1, x))
        j, pl, hd = obs.get((f, B2, x)), obs.get((f, B3, x)), obs.get((f, B4, x))
        if not (p0 and p1 and j and pl and hd): bad.append((x, f, 'missing')); break
        dx, dy = tip(p0['ang'])
        if (j['x'], j['y']) != (p1['x'] + dx, p1['y'] + dy) or (pl['x'], pl['y']) != (j['x'], j['y']): bad.append((x, f, 'joint/pole', j['x'], j['y']))
        if abs((pl['ang'] - (360 - p0['ang'])) % 360) > 1e-3: bad.append((x, f, 'pole angle', pl['ang'], p0['ang']))
        hx, hy = tip(pl['ang'])
        if (hd['x'], hd['y']) != (pl['x'] + hx, pl['y'] + hy): bad.append((x, f, 'head', hd['x'], hd['y']))
        xs[x].append(j['x'] - p1['x'])
check('bamboo_parts_follow_the_pivot', not bad, str(bad[:4]))
check('bamboo_joint_swings_both_sides', all(min(v) <= -52 and max(v) >= 52 for v in xs.values()),
      str({x: (min(v), max(v)) for x, v in xs.items() if v}))
check('bamboo_head_stays_over_the_pivot', all(abs(obs[(f, B4, x)]['x'] - x) <= 1 for x in SETS for f in range(2, 420) if (f, B4, x) in obs))

# ---- timing (RV start trace: the pivots at 8848 / 9040 / 8512 start swinging at rows 29 / 39 / 49 = spread B 20 / 10 / 0,
# 5319); swing 5 degrees a frame to 70 (13 frames), hold 51, back, then the other side: 128 frames a side
def first(x, cond, after=0):
    return next((f for f in range(after, 420) if (f, B1, x) in obs and cond(obs[(f, B1, x)])), None)
st1 = {x: first(x, lambda o: o['st'] == 1) for x in SETS}
check('bamboo_start_phases_rv', all(st1[x] is not None and abs(st1[x] - (r + 1)) <= 2 for x, r in ((8848, 29), (9040, 39), (8512, 49))), str(st1))
ok = True; info = {}
for x in SETS:
    a = st1[x]; b = first(x, lambda o: o['st'] == 2, a); c = first(x, lambda o: o['st'] == 1, b + 1 if b else 0)
    d0, d1 = obs[(a, B1, x)]['dir'], obs[(c, B1, x)]['dir'] if c else None
    info[x] = (a, b, c, d0, d1); ok &= b == a + 13 and c == a + 128 and d1 == -d0
check('bamboo_cycle_128_frames_a_side', ok, str(info))

# ---- damage (5228): standing in the joint's sweep (RV idle trace: one hurt at row 195, then knocked out of reach)
rows, _ = run('180 TP 8455 2060', 820)
h = hurts(rows)
check('bamboo_joint_hurts_standing_player_rv', len(h) == 1 and abs(h[0] - (195 + 120)) <= 4, str(h))
# walking under / back out (RV walk trace: hurts at rows 143, 224, 318, 387, 450), the turned character sensor on the slope
rows, _ = run('180 TP 8380 2060;220 280 R;320 350 L;420 450 R', 620)
h = hurts(rows)
want = [r + 120 for r in (143, 224, 318, 387, 450)]
check('bamboo_joint_hurts_on_approach_and_retreat_rv', len(h) == len(want) and all(abs(a - b) <= 5 for a, b in zip(h, want)), 'host %s orig %s' % (h, want))
check('bamboo_repeated_contact_waits_for_invulnerability', all(b - a >= 60 for a, b in zip(h, h[1:])), str(h))
# no hurt while invulnerable
rows, _ = run('180 TP 8455 2060;181 SETV others E 400', 560)
check('bamboo_no_hurt_while_invulnerable', not hurts(rows), str(hurts(rows)))
# metal shield (OTHERS.B 3): immune; another shield absorbs the hit (2412 / 2413), no petal lost
rows, _ = run('180 TP 8380 2060;230 MON %d;232 234 X;330 TP 8455 2060' % MON_METAL, 720)
ctl, _ = run('180 TP 8380 2060;330 TP 8455 2060', 720)   # control: the next sweep (~569) hurts without the shield
check('bamboo_metal_shield_immune', rows[329]['sh'] == 3 and not hurts(rows[330:]) and rows[-1]['sh'] == 3 and hurts(ctl[330:]),
      'sh %s hurts %s control %s' % (rows[329]['sh'], hurts(rows[330:]), hurts(ctl[330:])))
# Carol and Milla take the same contact damage
for ch, nm in ((2, 'carol'), (4, 'milla')):
    rows, _ = run('180 TP 8455 2060', 420, ch=ch)
    h = hurts(rows)
    check('bamboo_hurts_%s' % nm, len(h) == 1 and abs(h[0] - 315) <= 8, str(h))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
