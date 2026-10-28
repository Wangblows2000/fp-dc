#!/usr/bin/env python3
"""Host tests for the Battle Glacier gimmicks (session 24; src/game/bg_gim.c, types 2400-2425; build/re/spec_bg_gimmicks.md).
RE = the decompiled events (14118-14186 BG1-BG3, 14362-14396 / 14604-14606 BG4, 14633-14656 BG5); RV = the original traces in
build/orig_traces/bg_gim (tb1 sp1 sn1 spi1 bar2 fe4 mg4 mg4m ag4 sw5 mi4: the same inputs as below). Lilac (0), Carol (2), Milla (4):
 - the placements per act (importer BG_GIM block) and the runtime tele-blocks (one per mask);
 - tele-blocks (bg1): a mask near the player pulls its block in 20 px a frame (spin + 5), the arrival plays sound 77, the block sits
   on the mask (C 0) and the player walks the staircase down; spiral slides (bg1): M 96 ride around the pole and out at the end;
 - snake drills: x = D + 128 sin F, y = E - 128 cos F (RV sn1), sound 149 and rocks; snow spires: rise 1.5 a frame to 201, sound
   110 not more often than every 17 frames (RV spi1); BG bars (bg2): +-6 a frame, a 41-frame pause, the hang follows the bar;
 - crank ferris (bg4): a 9-frame ratchet every 50 frames; antigrav (bg4): the lift (G -4, sound 344) and the 20-px-a-frame puffs;
 - magnet launchers (bg4): grab (M 95, sound 7), carried 6 px a frame up to the stopper, released by jump (sound 5), the magnet goes
   back to its spot after 90 frames; spin mines: y = D + 64 sin E (RV mi4), contact damage;
 - tele switch (bg5): the touch swaps the switch blocks (sound 285) and opens the wall at x 528 for all three characters;
 - transporters are solid and animate, sirens run (g3 / 4) mod 4; bg1's start route is passable for all three characters.
usage: test_bg_gimmicks.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, math, collections
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (0, 2, 4)
NAME = {0: 'lilac', 2: 'carol', 4: 'milla'}
DY = 14                                                    # port y = MV.Y + 14 (ORIG_REF_DY)

def run(script, frames, env):
    """sids: (frame, id); obs: (frame, fields with 'type'); rows: player rows by frame"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_ENERGY')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=900)
    os.unlink(name)
    fr, sids, obs, rows, seen = 0, [], [], {}, collections.Counter()
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fr = int(t[1]); rows[fr] = dict(zip(t[2::2], t[3::2]))
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))
        elif t[0] == 'ob':
            d = dict(zip(t[4::2], t[5::2])); d['type'] = int(t[2]); k = (t[1], t[2], t[3]); seen[k] += 1
            d['key'] = t[3] if t[3] != '-1' else '#%d' % (seen[k] - 1); obs.append((int(t[1]), d))   # '-1' tracks: the slot order
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def F(d, k): return float(d[k])
def env(lv, c, track=''):
    e = {'FP_LEVEL': lv, 'FP_CHAR': str(c)}
    if track: e['FP_TRACK'] = track
    return e
def series(obs, typ, key=None):
    """{key: [(frame, fields)]} of one type"""
    out = collections.defaultdict(list)
    for f, d in obs:
        if d['type'] == typ and (key is None or d['key'] == key): out[d['key']].append((f, d))
    return out
def sinr(a): return math.sin(math.radians(a))
def cosr(a): return math.cos(math.radians(a))
INV = '3 SETV others E 99999;'

# ---- placements per act (importer BG_GIM; RE on_frame_60..65_init) + the runtime tele-blocks (14175: one per mask)
COUNTS = {
    'bg1': {2400: 60, 2401: 60, 2402: 10, 2403: 48, 2404: 8, 2405: 8, 2406: 34, 2407: 11},
    'bg2': {2400: 85, 2401: 85, 2402: 12, 2403: 24, 2404: 5, 2405: 5, 2406: 20, 2407: 8, 2409: 8, 2410: 8},
    'bg3': {2400: 7, 2401: 7, 2402: 6, 2406: 29, 2407: 14, 2408: 1},
    'bg4': {2406: 20, 2411: 50, 2412: 16, 2413: 64, 2414: 1, 2415: 11, 2417: 6, 2418: 12, 2419: 6, 2420: 8},
    'bg5': {2411: 55, 2412: 4, 2413: 16, 2414: 1, 2415: 17, 2417: 1, 2418: 2, 2419: 1, 2420: 4, 2421: 220, 2422: 15, 2423: 30,
            2424: 10, 2425: 15},
    'bg6': {2411: 2, 2425: 3}}
ALL = ','.join('%d:-1' % t for t in range(2400, 2426))
for lv, want in COUNTS.items():
    s, obs, rows = run('', 3, env(lv, 0, ALL))
    got = collections.Counter(d['type'] for f, d in obs if f == 2)
    check('%s_placements' % lv, dict(got) == want, str(dict(sorted(got.items()))))

# ---- tele-blocks (bg1; RE 14124-14141, RV tb1: C 320 -> 0 by 20, D + 5, the block at mask + C (cos D, sin D))
s, obs, rows = run(INV + '5 TP 360 620;10 160 R', 160, env('bg1', 0, '2400:-1,2401:-1'))
ms = series(obs, 2400)
arr = None
for k, sq in ms.items():
    C = [F(d, 'a2') for f, d in sq]
    if 0 in C and 320 in C:
        i0 = C.index(0); j = max(i for i in range(i0) if C[i] == 320)
        steps = [C[i] - C[i + 1] for i in range(j, i0)]
        Ds = [F(d, 'a3') for f, d in sq[j:i0 + 1]]
        dstep = all(abs((b - a) % 360 - 5) < 1e-6 for a, b in zip(Ds, Ds[1:]))
        if all(x == 20 for x in steps) and dstep: arr = sq[i0][0]; mk = sq; break
check('teleblock_pull_in_20_spin_5', arr is not None, str(arr))
check('teleblock_arrival_sound_77', arr is not None and any(arr - 1 <= f <= arr + 1 for f in sid(s, 77)), '%s %s' % (arr, sid(s, 77)[:4]))
bl = [d for f, d in obs if d['type'] == 2401 and f == 150]
vis = [d for d in bl if d['hd'] == '0']
check('teleblock_blocks_visible_when_pulled', len(vis) >= 1 and all(F(d, 'a0') < 320 for d in vis) and all(d['hd'] == '1' for d in bl if F(d, 'a0') >= 320),
      '%d visible of %d' % (len(vis), len(bl)))
check('teleblock_staircase_walked', F(rows[159], 'x') > 600 and F(rows[159], 'y') > 640, '%s %s' % (rows[159]['x'], rows[159]['y']))

# ---- spiral slides (bg1 start 1688 824 / end 1688 1120; RE 14142-14147, RV sp1: ride 56 frames, out at the end's y)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '5 TP 1600 800;10 160 R', 200, env('bg1', c))
    ride = [f for f in sorted(rows) if rows[f]['mm'] == '96']
    check('%s_spiral_ride' % n, len(ride) >= 40 and ride[0] in sid(s, 7) and all(1688 - 25 <= F(rows[f], 'x') <= 1688 + 25 for f in ride[1:]),
          '%s..%s' % (ride[:1], ride[-1:]))
    out = ride[-1] + 1 if ride else None
    check('%s_spiral_out_at_the_end' % n, out and 1100 <= F(rows[out], 'y') - DY <= 1160 and F(rows[199], 'x') > 1800 and rows[199]['mm'] != '96',
          '%s %s %s' % (out, out and rows[out]['y'], rows[199]['x']))

# ---- snake drills (bg1 760 1420; RE 14148-14160, RV sn1)
s, obs, rows = run(INV + '5 TP 760 1420', 300, env('bg1', 0, '2403:-1,225:-1'))
sn = [d for f, d in obs if d['type'] == 2403]
bad = [d for d in sn if abs(F(d, 'x') - math.trunc(F(d, 'a3') + 128 * sinr(F(d, 'a5')))) > 1 or abs(F(d, 'y') - math.trunc(F(d, 'a4') - 128 * cosr(F(d, 'a5')))) > 1]
# (the fields are read after the frame's step: F moved by 1 or 3 (2 at the turn: 180 - 1 + 3); allow the step)
bad = [d for d in bad if min(abs(F(d, 'x') - math.trunc(F(d, 'a3') + 128 * sinr(F(d, 'a5') + k))) for k in (-3, -2, -1, 1, 3)) > 1]
check('snake_on_its_circle', sn and not bad, '%d of %d off %s' % (len(bad), len(sn), [(d['x'], d['y'], d['a3'], d['a4'], d['a5']) for d in bad[:3]]))
rocks = set(d['key'] for f, d in obs if d['type'] == 225)
check('snake_sound_149_and_rocks', sid(s, 149) and len(rocks) >= 5, '%s %d' % (sid(s, 149)[:3], len(rocks)))

# ---- snow spires (bg1 3168 1170; RE 14118-14123, RV spi1: D + 1.5 to 201, sound 110 every 17)
s, obs, rows = run(INV + '5 TP 3168 1170', 220, env('bg1', 0, '2405:-1'))
sp = series(obs, 2405)
risen = [sq for sq in sp.values() if F(sq[-1][1], 'a3') > 0]
ok = False
for sq in risen:
    Ds = [F(d, 'a3') for f, d in sq]
    st = [b - a for a, b in zip(Ds, Ds[1:]) if b != a]
    ok = ok or (Ds[-1] == 201 and all(x == 1.5 for x in st))
check('spire_rises_1_5_to_201', ok, str([F(sq[-1][1], 'a3') for sq in risen]))
t110 = sid(s, 110)
check('spire_sound_110_every_17', len(t110) >= 3 and all(b - a >= 17 for a, b in zip(t110, t110[1:])), str(t110[:6]))

# ---- BG bars (bg2 mask 3401 630; RE 14180-14186 / 14245, RV bar2: +-6, the pause D to 41, reverse)
s, obs, rows = run(INV, 300, env('bg2', 0, '2410:3401'))
bm = [d for f, d in obs if d['type'] == 2410 and F(d, 'a0') == 612]
B = [F(d, 'a1') for d in bm]
mv = [abs(b - a) for a, b in zip(B, B[1:]) if b != a]
held = max((len(list(g)) for k, g in __import__('itertools').groupby(B) if abs(k) == 128), default=0)
check('bar_moves_6_pause_41', mv and all(x <= 6 for x in mv) and 40 <= held <= 42 and all(F(d, 'y') == 612 + F(d, 'a1') for d in bm),
      'held %d' % held)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '5 TP 3401 700;30 34 J', 200, env('bg2', c, '2410:3401'))
    hang = [f for f in sorted(rows) if rows[f]['mm'] == '41']
    bmy = {f: F(d, 'y') for f, d in obs if d['type'] == 2410 and F(d, 'a0') == 612}
    follow = [f for f in hang if f in bmy and abs(F(rows[f], 'y') - DY - bmy[f]) <= 1]
    check('%s_bar_hang_follows' % n, len(hang) >= 60 and len(follow) >= len(hang) - 2, '%d hang, %d follow' % (len(hang), len(follow)))

# ---- crank ferris (bg4 hub 4032 1296; RE 14362-14368, RV fe4: a 45-degree ratchet, 9 frames of 5 every 50 frames; landed on the
# bottom car she is carried up the left side: (4031, 1394) -> (3940, 1356) -> (3903, 1266) -> (3940, 1175), MV units, as the original)
RIDE = [(4031, 1394), (3940, 1356), (3903, 1266), (3940, 1175)]
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '120 TP 4032 1100', 330, env('bg4', c, '2412:-1'))
    if c == 0:
        fe = series(obs, 2412)
        sq = fe['#1']
        pos = [(f, F(d, 'x'), F(d, 'y')) for f, d in sq]
        moving = [f for (f, x, y), (g, x2, y2) in zip(pos, pos[1:]) if (x, y) != (x2, y2)]
        win = collections.Counter(f // 50 for f in moving)
        check('ferris_ratchet_9_of_50', len(win) >= 5 and all(v == 9 for v in win.values()), str(dict(win)))
    path = []
    for f in range(170, 330):
        if rows[f]['gnd'] == '1':
            q = (round(F(rows[f], 'x')), round(F(rows[f], 'y')) - DY)
            if not path or path[-1] != q: path.append(q)
    check('%s_ferris_ride' % n, all(q in path for q in RIDE) and rows[329]['hp'] == rows[10]['hp'], str([q for q in path if q in RIDE]))

# ---- antigrav (bg4 spawner 3776 1832; RE 14369-14378, RV ag4: G -4, sound 344, the lift)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '60 TP 3776 1790', 140, env('bg4', c, '2416:-1'))
    vy = [F(rows[f], 'vy') for f in range(62, 130)]
    t344 = sid(s, 344)
    check('%s_antigrav_lift' % n, t344 and 58 <= t344[0] <= 62 and min(vy) <= -4 and F(rows[60], 'y') - F(rows[130], 'y') > 200,
          '%s %.2f %s -> %s' % (t344[:2], min(vy), rows[60]['y'], rows[130]['y']))
    if c == 0:
        ag = [d for f, d in obs if d['type'] == 2416 and F(d, 'x') in (3776, 3840)]
        check('antigrav_puffs_20_a_frame', ag and all(1832 - F(d, 'y') == 20 * F(d, 'a0') and F(d, 'a0') <= 17 for d in ag),
              str([(d['y'], d['a0']) for d in ag[:3]]))

# ---- magnet launchers (bg4 4768 1352; RE 14379-14394, RV mg4 / mg4m: grab at ~100, carried to 5068, released)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '60 TP 4768 1480;80 110 J;150 200 R;230 232 J', 420, env('bg4', c, '2417:4768'))
    m95 = [f for f in sorted(rows) if rows[f]['mm'] == '95']
    mg = {f: d for f, d in obs if d['type'] == 2417}
    check('%s_magnet_grab' % n, m95 and 95 <= m95[0] <= 110 and m95[0] in sid(s, 7), str(m95[:1]))
    xs = [F(mg[f], 'x') for f in range(150, 205) if f in mg]
    st = [b - a for a, b in zip(xs, xs[1:]) if b != a]
    check('%s_magnet_carried_to_the_stopper' % n, st and all(x == 6 for x in st) and xs[-1] == 5068 and abs(F(rows[220], 'x') - 5068) <= 1,
          '%s %s' % (xs[-1:], rows[220]['x']))
    rel = m95[-1] + 1 if m95 else None
    check('%s_magnet_release' % n, rel and 229 <= rel <= 233 and any(rel - 1 <= f <= rel + 1 for f in sid(s, 5)) and F(rows[rel + 5], 'y') > F(rows[rel], 'y'),
          str(rel))
    back = [F(mg[f], 'x') for f in sorted(mg) if f > 233]
    check('%s_magnet_returns' % n, back and back[-1] == 4768 and all(x == 5068 for x in back[:85]), str(back[80:95:5]))

# ---- spin mines (bg4 7480 ..; RE 14395 / 14396 / 14606, RV mi4: the same E / y at frame 60)
s, obs, rows = run('60 TP 7480 1800', 120, env('bg4', 0, '2420:7480,2420:7528'))
mi = [(f, d) for f, d in obs if d['type'] == 2420]
on = all(F(d, 'y') == math.trunc(F(d, 'a3') + 64 * sinr(F(d, 'a4') - 2)) for f, d in mi)
e60 = {d['key']: F(d, 'a4') for f, d in mi if f == 60}
check('spinmine_path_and_phase', on and e60.get('7480') == 332 and e60.get('7528') == 302, str(e60))   # 14606: E = 30 (7 - k), + 2 a frame
check('spinmine_contact_hurts', int(rows[100]['hp']) < int(rows[50]['hp']), '%s -> %s' % (rows[50]['hp'], rows[100]['hp']))

# ---- tele switch + switch blocks (bg5 switch 448 960, the wall of blocks 1 at x 528; RE 14647-14654, RV sw5)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '10 200 R;72 95 J', 200, env('bg5', c, '2421:528,2421:560'))
    sw = sid(s, 285)
    wall = {f: d for f, d in obs if d['type'] == 2421 and d['key'] == '528' and F(d, 'y') == 1008}
    floor = {f: d for f, d in obs if d['type'] == 2421 and d['key'] == '560'}
    t = sw[0] if sw else None
    check('%s_switch_touch_285' % n, t and t < 120, str(sw[:2]))
    check('%s_switch_swaps_blocks' % n, t and int(wall[t - 2]['fl']) & 1 == 0 and int(wall[t + 1]['fl']) & 1 == 1 and int(floor[t + 1]['fl']) & 1 == 0,
          '%s %s' % (t and wall[t - 2]['fl'], t and wall[t + 1]['fl']))
    check('%s_switch_opens_the_wall' % n, F(rows[199], 'x') > 700, rows[199]['x'])
s, obs, rows = run(INV + '10 200 R', 200, env('bg5', 0))
check('switch_wall_blocks_without_the_touch', F(rows[199], 'x') < 528 and not sid(s, 285), rows[199]['x'])

# ---- transporters (Q7, own 4-frame animation) and sirens (14656: frame (g3 / 4) mod 4) (bg5)
s, obs, rows = run(INV + '60 TP 3584 1060', 140, env('bg5', 0, '2424:3584,2425:8511'))
tr = [d for f, d in obs if d['type'] == 2424]
check('transporter_solid', rows[139]['gnd'] == '1' and F(rows[139], 'y') - DY < 1120, '%s %s' % (rows[139]['gnd'], rows[139]['y']))
check('transporter_animates', len(set(d['fr'] for d in tr)) >= 3, str(sorted(set(d['fr'] for d in tr))))
si = [int(d['fr']) for f, d in obs if d['type'] == 2425 and F(d, 'y') == 1775]
runs = [(k, len(list(g))) for k, g in __import__('itertools').groupby(si)]
check('siren_frames', len(runs) > 8 and all(n == 4 for k, n in runs[1:-1]) and all((b - a) % 4 == 1 for (a, _), (b, _) in zip(runs, runs[1:])),
      str(runs[:6]))

# ---- bg1 start route (glacier platforms, the first spires / tele-blocks): passable for all three characters
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '10 400 R;40 65 J', 400, env('bg1', c))
    check('%s_bg1_start_route' % n, F(rows[399], 'x') > 1300, rows[399]['x'])

print('%d failures' % fails)
sys.exit(1 if fails else 0)
