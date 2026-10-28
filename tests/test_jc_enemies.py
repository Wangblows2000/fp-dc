#!/usr/bin/env python3
"""Host tests for the Jade Creek enemies (session 21; src/game/jc_enemy.c, build/re/spec_jc_enemies.md) against the original's
traces (RV, build/orig_traces/jc_enemies: session1.sh / session2.sh, same input scripts as below; frame numbers are the trace's):
plant turretus (rise 2 px a frame, shot sound 28 every 190 frames, recoil), the three characters hitting a plant (Lilac / Carol
defeat it, the wreck crashes 35 frames later, Milla's attack does not reach), meteor roller (lands 50 frames apart, sound 38,
blast pieces at speed 6), buoy (jets every 197 frames, sound 60, jets live 71 frames), shark torpedo (torpedoes up / down at
4 px, 152 frames apart, the limiter turn), torpedo launchers (JC4: sound 99 per shot 7 frames apart, a 179-frame cycle, the
bomb bursts 62 frames later with sound 38), bull frag (ten bubbles, the first pops 34 frames after sound 275, 9-frame gaps), bubbl orbiter (JC2 ring), elite (JC5: none at the start).
usage: test_jc_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    """rows: per-frame player rows; sids: (frame, id); obs: FP_TRACK rows (frame, type, x, y, fields dict)"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    fr, sids, obs = 0, [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fr = int(t[1])
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))        # a sound belongs to the next 'f' row (trace frame numbering)
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]), int(t[2]), float(d['x']), float(d['y']), d))
    return sids, obs

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def gaps(v): return [b - a for a, b in zip(v, v[1:])]
def near(a, b, tol): return a is not None and b is not None and abs(a - b) <= tol
INV = '3 SETV others E 99999\n'

# ---- plant turretus (JC1 4786, 731; RV plant: rise f183 -> top f222, shots 237 / 427 / 617, pellet pop 17 at +18, recoil +3)
s, obs = run(INV + '160 TP 4650 700\n', 700, {'FP_LEVEL': 'jc1', 'FP_TRACK': '1314:4786'})
sh = sid(s, 28)
check('plant_shoots', len(sh) >= 3, str(sh))
check('plant_cycle_190', len(sh) >= 3 and all(abs(g - 190) <= 2 for g in gaps(sh)), str(gaps(sh)))
check('plant_first_shot', sh and abs(sh[0] - 237) <= 12, str(sh[:1]))                  # (the 10-frame check runs on the global counter's phase)
pop = sid(s, 17)
check('plant_pellet_pops_18_later', sh and pop and abs(pop[0] - sh[0] - 18) <= 2, '%s %s' % (sh[:1], pop[:1]))
ys = {f: y for f, t, x, y, d in obs}
rise = [f for f in sorted(ys) if f > 0 and ys[f] < ys.get(f - 1, ys[f]) ]
check('plant_rises_2px', rise and all(ys[f - 1] - ys[f] == 2 for f in rise), str(rise[:3]))
top = [f for f in sorted(ys) if ys[f] == 731]
check('plant_rise_40_frames', rise and top and top[0] - rise[0] == 39, '%s %s' % (rise[:1], top[:1]))
xs = sorted(set(x for f, t, x, y, d in obs))
check('plant_recoil_3', 4789 in xs and max(xs) <= 4790, str(xs))

# ---- the three characters attack the lower plant (4426, 1059) (RV hit_c0 / c2 / c4: Lilac defeats it at 233 (sound 23), Carol at 247,
# the wreck crashes 35 frames later (sound 29); Milla's attack never reaches it)
HIT = INV + '160 TP 4370 1050\n' + ''.join('%d %d X\n' % (f, f + 2) for f in range(220, 561, 20))
for ch, rv in ((0, 233), (2, 247), (4, None)):
    s, _ = run(HIT, 600, {'FP_LEVEL': 'jc1', 'FP_CHAR': str(ch)})
    d, c = sid(s, 23), sid(s, 29)
    if rv is None:
        check('hit_c4_no_hit', not d and not sid(s, 24), '%s %s' % (d, sid(s, 24)))
    else:
        check('hit_c%d_defeat' % ch, d and abs(d[0] - rv) <= 3, str(d))
        check('hit_c%d_crash_35' % ch, d and c and abs(c[0] - d[0] - 35) <= 2, '%s %s' % (d, c))

# ---- meteor roller (JC1 2823, 892 and 3071, 884; RV meteor: lands 347 / 397 (sound 38), blast pieces +6 / (+4, +4) a frame, the first
# roller stays where it landed while she runs on)
s, obs = run(INV + '160 TP 2400 850\n200 560 R\n', 700, {'FP_LEVEL': 'jc1', 'FP_TRACK': '1310:2823,1340:-1'})
ld = sid(s, 38)
check('meteor_lands_347_397', len(ld) >= 2 and abs(ld[0] - 347) <= 3 and ld[1] - ld[0] == 50, str(ld))
fall = [(f, x, y) for f, t, x, y, d in obs if t == 1310 and d.get('a0') == '2.000']
check('meteor_falls_5_5', len(fall) > 5 and all(b[1] - a[1] == 5 and b[2] - a[2] == 5 for a, b in zip(fall, fall[1:]) if b[0] == a[0] + 1), str(fall[:2]))
m699 = [x for f, t, x, y, d in obs if t == 1310 and f == 699]
check('meteor_stays', m699 and abs(m699[0] - 2823) <= 4, str(m699))
pieces = {}
for f, t, x, y, d in obs:
    if t == 1340: pieces.setdefault(round(float(d['vx']) * 100), []).append((f, x, y))
p0, p1 = pieces.get(600, []), pieces.get(424, [])
check('meteor_blast_6', len(p0) > 2 and p0[1][1] - p0[0][1] == 6, str(p0[:2]))
check('meteor_blast_45deg', len(p1) > 2 and p1[1][1] - p1[0][1] == 4 and p1[1][2] - p1[0][2] == 4, str(p1[:2]))

# ---- buoys (JC1 4616 / 4920 / 5064; RV buoy: sound 60 at 220 / 417 / 614, jets 71 frames, at the buoy +-10, -10)
s, obs = run(INV + '160 TP 4750 1400\n', 700, {'FP_LEVEL': 'jc1', 'FP_TRACK': '1341:-1,1312:4616'})
j = sid(s, 60)
check('buoy_fires', len(j) >= 3, str(j))
check('buoy_cycle_197', len(j) >= 3 and all(abs(g - 197) <= 4 for g in gaps(j)), str(gaps(j)))
life = {}
for f, t, x, y, d in obs:
    if t == 1341: life.setdefault(d.get('born', x), []).append(f)
jf = sorted(set(f for f, t, x, y, d in obs if t == 1341))
runs, st = [], None
for f in jf:
    if st is None or f != st[1] + 1: st = [f, f]; runs.append(st)
    else: st[1] = f
check('buoy_jets_71', runs and abs(runs[0][1] - runs[0][0] + 1 - 71) <= 1, str([(a, b - a + 1) for a, b in runs[:3]]))
by = {f: y for f, t, x, y, d in obs if t == 1312}
jet0 = [(x, y, f) for f, t, x, y, d in obs if t == 1341 and runs and f == runs[0][0]]
check('buoy_jet_offsets', any(abs(x - 4626) <= 0 and y == by.get(f, 0) - 10 for x, y, f in jet0) and any(x == 4606 for x, y, f in jet0), str(jet0))

# ---- shark torpedo (JC1 9460, 1565; RV shark: torpedo up from (9398, 1561) at 192, down from (9466, 1569) at 344; the limiter at 9282)
s, obs = run(INV + '160 TP 9300 1500\n', 660, {'FP_LEVEL': 'jc1', 'FP_TRACK': '1344:-1,1321:9460'})
tp = {}
for f, t, x, y, d in obs:
    if t == 1344: tp.setdefault(x, []).append((f, y))
ups = [v for x, v in tp.items() if len(v) > 2 and v[1][1] - v[0][1] == -4]
downs = [v for x, v in tp.items() if len(v) > 2 and v[1][1] - v[0][1] == 4]
check('shark_torpedo_up', ups and 9396 <= [x for x, v in tp.items() if v is ups[0]][0] <= 9400 and abs(ups[0][0][0] - 191) <= 3, str(ups[:1][0][:2] if ups else tp.keys()))
check('shark_torpedo_down', downs and abs(downs[0][0][0] - ups[0][0][0] - 152) <= 2 if ups and downs else False, str([(v[0]) for v in tp.values()]))
sx = [x for f, t, x, y, d in obs if t == 1321]
check('shark_turns_at_limiter', sx and abs(min(sx) - 9282) <= 2, str(min(sx) if sx else None))

# ---- torpedo launchers (JC4 start; RV launcher: sound 99 at 18 / 25 / 32 then +179, bursts (38) 62 frames after each shot)
s, obs = run(INV, 500, {'FP_LEVEL': 'jc4'})
f99, f38 = sid(s, 99), sid(s, 38)
check('launcher_volley_7', len(f99) >= 3 and gaps(f99[:3]) == [7, 7], str(f99[:6]))
check('launcher_cycle_179', len(f99) >= 4 and f99[3] - f99[0] == 179, str(f99[:6]))
check('launcher_burst_62', f99 and f38 and f38[0] - f99[0] == 62, '%s %s' % (f99[:1], f38[:1]))

# ---- bubbl orbiter (JC2 888, 1320; RV orbiter2: 15 shots on y = E + 14 = 1334, x within 888 +- 90; it bobs 16 px)
s, obs = run(INV + '60 TP 1000 1300\n', 200, {'FP_LEVEL': 'jc2', 'FP_TRACK': '1345:-1,1323:888'})
ring = [(x, y) for f, t, x, y, d in obs if t == 1345 and f == 150 and abs(x - 888) <= 100]
check('orbiter_ring_15', len(ring) == 15 and all(y == 1334 and abs(x - 888) <= 90 for x, y in ring), str(ring[:4]))
oy = [y for f, t, x, y, d in obs if t == 1323]
check('orbiter_bobs', oy and min(oy) <= 1290 and max(oy) >= 1316, '%s %s' % (min(oy) if oy else None, max(oy) if oy else None))

# ---- bull frag (JC1 7151, 960; RV frog2 / frog2_c2 / frog2_c4: sound 275 at 209, the ten bubbles pop (314) at 243 .. 325, gaps 9 9 10 9 ..,
# the first 34 frames after 275 (the bubble moves in the frame it is blown))
for ch in (0, 2, 4):
    s, obs = run(INV + '160 TP 7000 1060\n' + ('240 660 B\n' if ch == 4 else ''), 400, {'FP_LEVEL': 'jc1', 'FP_CHAR': str(ch)})
    b275, b314 = sid(s, 275), sid(s, 314)
    check('frog_c%d_blows' % ch, len(b275) >= 1 and abs(b275[0] - 209) <= 4 and len(b314) >= 10, '%s %d' % (b275, len(b314)))
    check('frog_c%d_pop_34' % ch, b275 and b314 and b314[0] - b275[0] == 34, '%s %s' % (b275[:1], b314[:1]))
    check('frog_c%d_pop_gaps' % ch, gaps(b314[:10]) == [9, 9, 10, 9, 9, 9, 9, 9, 9], str(gaps(b314[:10])))

# ---- the JC5 elite: no instance at the act start (RV elite: its lists are empty)
s, obs = run(INV, 30, {'FP_LEVEL': 'jc5', 'FP_TRACK': '1326:-1,1327:-1'})
check('elite_absent_jc5', not obs, str(obs[:1]))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
