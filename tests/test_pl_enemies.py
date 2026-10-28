#!/usr/bin/env python3
"""Host tests for the Pangu Lagoon enemies (session 23; src/game/pl_enemy.c, build/re/spec_pl_enemies.md), Sash Lilac only (the
stage's one character). RE timings from the decompiled events (handle_frame_51..56, see the spec); no original traces were recorded
for this module (the checks pin the RE rules: speeds, state lengths, sound ids, hurts) and that Lilac can beat or pass each enemy.
  stinger: hover / fly cycle (4 px a frame for 31 frames), the 45-degree dive (sound 10, contact damage 5, 5 px a frame), climb back
  nest: angered by a hit, a stinger every 8 frames (sound 323), the burst death (sound 38, gone)
  sunflora: spray (sound 44), sleep gas every 3 frames, the gas hurt (5, pink tint), regrows 300 frames after its defeat
  terreon: jump (sound 147), landing (sound 51, two shockwaves), armoured legs (deflect sound 30), defeat (sound 38, debris)
  obelisk: 174-frame cycle (sound 327), the laser 8 px a frame, its hurt 5; wall stinger -> stinger; A4 swarm wrap; the back swarm
usage: test_pl_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    """sids: (frame, id); obs: (frame, type, x, y, fields); rows: player rows by frame"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SID': '1', 'FP_CHAR': '0'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    fr, sids, obs, rows = 0, [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]), int(t[2]), float(d['x']), float(d['y']), d))
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def gaps(v): return [b - a for a, b in zip(v, v[1:])]
def of(obs, t, ox=None): return [o for o in obs if o[1] == t and (ox is None or o[4].get('_ox') == ox)]
def series(obs, t, key):
    return [(f, float(d[key])) for f, tt, x, y, d in obs if tt == t]
INV = '3 SETV others E 99999\n'
def attacks(f0, f1, step=12, jump=False):
    s = ''
    for f in range(f0, f1, step):
        s += ('%d %d J\n%d %d X\n' % (f, f + 10, f + 6, f + 9)) if jump else ('%d %d X\n' % (f, f + 3))
    return s

# ---- stinger (pl1 1815, 1288; RE 12784-12787): hovers > 50 frames (I from R(30)), then flies 4 px a frame for 30 frames
s, obs, _ = run(INV + '60 TP 1850 1700\n', 600, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2080:1815'})   # (1815, 1288: 400 px above her, no dive)
st = [o for o in obs if o[1] == 2080]
D = [(f, float(d['a0'])) for f, t, x, y, d in st]
fly = [f for f, v in D if v == 1]
xs = {f: x for f, t, x, y, d in st}
runs, cur = [], []
for f in fly:
    if cur and f != cur[-1] + 1: runs.append(cur); cur = []
    cur.append(f)
if cur: runs.append(cur)
full = [r for r in runs if len(r) > 5]
steps = [abs(xs[f + 1] - xs[f]) for r in full for f in r[:-1] if f + 1 in xs]
check('stinger_flies_4px', steps and all(v == 4 for v in steps), str(sorted(set(steps))))
Dm = dict(D)
ends = [len(r) for r in full if Dm.get(r[-1] + 1) == 0]                  # (runs a dive did not cut short)
check('stinger_fly_31_frames', ends and all(len_ in (30, 31) for len_ in ends), str([len(r) for r in full]))

# ---- stinger dive (pl1 879: Lilac on its 45-degree diagonal): wind-up 31 frames, dive sound 10, damage 5, +-5 px a frame, climb back
s, obs, rows = run(INV + '60 TP 700 1700\n100 500 R\n', 500, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2080:879'})
st = [o for o in obs if o[1] == 2080]
D = {f: float(d['a0']) for f, t, x, y, d in st}
f2 = next((f for f in sorted(D) if D[f] == 2), None); f3 = next((f for f in sorted(D) if D[f] == 3), None)
check('stinger_windup_31', f2 and f3 and 30 <= f3 - f2 <= 32, '%s %s' % (f2, f3))
check('stinger_dive_sound_10', any(f3 and f3 <= f <= f3 + 10 for f in sid(s, 10)), '%s %s' % (f3, sid(s, 10)[:3]))
dive = [(f, x, y, int(d['bd'])) for f, t, x, y, d in st if d['a0'] == '3.000' and d['bd'] == '5']
dd = [(dive[k + 1][1] - dive[k][1], dive[k + 1][2] - dive[k][2]) for k in range(len(dive) - 1) if dive[k + 1][0] == dive[k][0] + 1]
check('stinger_dive_5px_diagonal', dd and all(abs(a) == 5 and b == 5 for a, b in dd), str(dd[:4]))
back = [f for f in sorted(D) if D[f] == 0 and f3 and f > f3]
check('stinger_climbs_back_home', back and st and any(y == 1592 for f, t, x, y, d in st if f >= back[0]), str(back[:1]))

# ---- stinger hurts a vulnerable Lilac (contact damage 5 while diving)
s, obs, rows = run('60 TP 779 1700\n', 400, {'FP_LEVEL': 'pl1'})
hp = [int(rows[f]['hp']) for f in sorted(rows) if 'hp' in rows[f]]
check('stinger_hurts_lilac', hp and min(hp) < hp[0], '%s -> %s' % (hp[0], min(hp)))

# ---- nest (pl1 2688, 1476): Lilac's jump attacks anger it (stingers every 8 frames, sound 323) and break it (sound 38)
s, obs, _ = run(INV + '60 TP 2670 1580\n' + attacks(100, 600, 40, True), 700, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2081:2688'})
n323 = sid(s, 323)
check('nest_releases_stingers_8f', len(n323) >= 4 and all(g % 8 == 0 for g in gaps(n323)), str(gaps(n323)[:6]))
check('nest_15_stingers_max', len(n323) <= 15, str(len(n323)))
nest = [o for o in obs if o[1] == 2081]
check('nest_defeated_by_lilac', 38 in [i for f, i in s] and nest and nest[-1][0] < 690, 'last %s' % (nest[-1][0] if nest else None))
check('nest_burst_explode_sounds', len([1 for f, i in s if i in (311, 319, 320)]) >= 9, str(len([1 for f, i in s if i in (311, 319, 320)])))

# ---- sunflora (pl1 552, 1656): spray sound 44, gas every 3 frames; the gas hurts 5 with the pink tint
s, obs, rows = run(INV + '60 TP 420 1700\n', 420, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2082:552,2083:-1'})
sf = [o for o in obs if o[1] == 2082]
s44 = sid(s, 44)
check('sunflora_sprays_sound_44', len(s44) >= 2, str(s44))
gas_born = sorted(set(f for f, t, x, y, d in obs if t == 2083 and d['a0'] == '1.000'))
check('sunflora_gas_every_3', len(gas_born) >= 10 and all(g == 3 for g in gaps(gas_born[:20])), str(gaps(gas_born)[:21]))
check('sunflora_gas_20_per_spray', 19 <= len([f for f in gas_born if f < (s44[1] if len(s44) > 1 else 9999)]) <= 21, str(len(gas_born)))
ages = [float(d['a0']) for f, t, x, y, d in obs if t == 2083]
check('gas_lives_80', ages and max(ages) <= 81, str(max(ages) if ages else None))
s, obs, rows = run('60 TP 470 1700\n', 330, {'FP_LEVEL': 'pl1'})
hp = [int(rows[f]['hp']) for f in sorted(rows) if 'hp' in rows[f]]
check('gas_hurts_lilac', hp and min(hp) < hp[0], '%s -> %s' % (hp[0] if hp else None, min(hp) if hp else None))

# ---- sunflora defeated by Lilac, regrows 300 frames later (A 12 again)
s, obs, rows = run(INV + '60 TP 530 1700\n' + attacks(100, 200, 10, False), 600, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2082:552'})
sf = [o for o in obs if o[1] == 2082]
dead = [f for f, t, x, y, d in sf if d['a0'] == '6.000']
check('sunflora_defeated_by_lilac', dead and 50 in sid(s, 50) or (dead and any(i == 50 for f, i in s)), str(dead[:1]))
back = [f for f, t, x, y, d in sf if dead and f > dead[0] and d['a0'] == '0.000' and d['A'] == '12.00']
check('sunflora_regrows_300', dead and back and 299 <= back[0] - dead[0] <= 303, '%s %s' % (dead[:1], back[:1]))

# ---- terreon (pl2 5027, 902): jump sound 147, landing sound 51 + two shockwaves; armoured legs; Lilac defeats it
s, obs, rows = run(INV + '60 TP 4990 890\n' + attacks(70, 700, 12), 800, {'FP_LEVEL': 'pl2', 'FP_TRACK': '2084:5027,367:-1'})
check('terreon_jumps_sound_147', len(sid(s, 147)) >= 1, str(sid(s, 147)))
check('terreon_lands_sound_51', len(sid(s, 51)) >= 1, str(sid(s, 51)))
l51 = sid(s, 51)
shocks = sorted(set(f for f, t, x, y, d in obs if t == 367))
check('terreon_landing_two_shockwaves', l51 and len([1 for f, t, x, y, d in obs if t == 367 and f == l51[0]]) == 2, str(l51[:1]))
check('terreon_legs_deflect_30', 30 in [i for f, i in s], '')
life = {}
for f, t, x, y, d in obs:
    if t == 367: life.setdefault(d.get('a1'), []).append(f)
ages = [float(d['a1']) for f, t, x, y, d in obs if t == 367]
check('shockwave_gone_after_50', ages and max(ages) <= 51, str(max(ages) if ages else None))
ter = [o for o in obs if o[1] == 2084]
check('terreon_defeated_by_lilac', 38 in [i for f, i in s] and ter and ter[-1][0] < 790, 'last %s' % (ter[-1][0] if ter else None))

# ---- obelisk (pl4 828, 228; its limiter turns it right): sound 327 every 174 frames, the laser 8 px a frame, hurts 5
s, obs, rows = run('60 TP 900 228\n', 600, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2090:828,2113:-1'})
s327 = sid(s, 327)
check('obelisk_cycle_174', len(s327) >= 2 and all(172 <= g <= 176 for g in gaps(s327)), str(gaps(s327)))
las = [(f, x) for f, t, x, y, d in obs if t == 2113]
mv = [b[1] - a[1] for a, b in zip(las, las[1:]) if b[0] == a[0] + 1 and b[1] != a[1]]
check('obelisk_laser_8px', mv and all(m == 8 for m in mv), str(sorted(set(mv))))
hp = [int(rows[f]['hp']) for f in sorted(rows) if 'hp' in rows[f]]
check('obelisk_laser_hurts', hp and min(hp) < hp[0], '%s -> %s' % (hp[0] if hp else None, min(hp) if hp else None))
s, obs, rows = run(INV + '60 TP 866 228\n62 64 L\n' + attacks(70, 400, 12), 500, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2090:828'})
ob = [o for o in obs if o[1] == 2090]
check('obelisk_defeated_by_lilac', 29 in [i for f, i in s] and ob and ob[-1][4]['a0'] == '6.000', ob[-1][4]['a0'] if ob else '')

# ---- wall stinger (pl4 1504, 1000): burrows 2 px a frame, out of the rock it becomes a stinger and leaves a spawn marker
s, obs, rows = run(INV + '60 TP 1400 1000\n', 300, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2091:1504,2114:-1'})
w = [(f, x, y) for f, t, x, y, d in obs if t == 2091]
sp = [(f, x, y) for f, t, x, y, d in obs if t == 2114]
stp = [((b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2) ** 0.5 for a, b in zip(w, w[1:]) if b[0] == a[0] + 1 and b[1:] != a[1:]]
check('wall_stinger_2px', stp and all(0.9 <= v <= 3.2 for v in stp), '%.2f..%.2f' % (min(stp), max(stp)) if stp else '')
check('wall_stinger_becomes_stinger', sp and sp[0][1:] == (1504.0, 1000.0) and w and w[-1][0] < sp[0][0], str(sp[:1]))

# ---- A4 swarm: stingers follow the view horizontally (13086 / 13087), within view - 80 .. view + 506
s, obs, rows = run(INV + '60 TP 1400 1000\n', 120, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2080:-1'})
xs = [x for f, t, x, y, d in obs if t == 2080 and f == 110 and float(d['a0']) < 5]
cam = float(rows[110]['x']) if 110 in rows else 1400
check('a4_stingers_wrap_to_view', xs and all(abs(x - cam) < 700 for x in xs), str(xs[:6]))
# ---- the back swarm (102 on json layer 1) stays in its band and wraps with the view
s, obs, rows = run(INV + '60 TP 3900 1500\n', 160, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2092:-1'})
b = [(x, y) for f, t, x, y, d in obs if t == 2092 and f == 150]
check('back_swarm_102', len(b) == 102, str(len(b)))
check('back_swarm_follows_view', b and all(3300 < x < 4700 and 1300 < y < 1720 for x, y in b), str(b[:3]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
