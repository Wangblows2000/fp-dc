#!/usr/bin/env python3
"""Host tests for the Final Dreadnought gimmicks (session 24; src/game/fd_gim.c, types 2752-2911; build/re/spec_fd_gimmicks.md).
RE = the decompiled events of handle_frame_66..73 (fd1a..fd4b) and their start events; RV = the original traces in
build/orig_traces/fd_gim (zap1 pd2 al2). Lilac (0), Carol (2), Milla (4) where the behaviour can differ:
 - the placements per act (importer FD_GIM block, the reused BG / TB types);
 - transporter pipes (15188-15201): every on-map start leads to a pad (or the level's edge), M 95 at 20 px a frame, sounds 345 / 346;
 - zap beams (15203-15212, RV zap1): the 81-frame cycle, C 30 at D 60 (sound 358), scale 24, the hurt under a firing beam;
 - spiral spark makers (15221-15228): two sparks every 150 frames (sound 274), they spiral down and hurt;
 - downhill platforms (15229-15238): stood on they roll down the rail carrying their rider, stop, and roll back once left;
 - airlocks (fd1b, RV al2): A / B / C open 16 a frame in turn once the player is within 159 px, sounds 279 / 381, she drops;
 - pressure doors (fd1b, RV pd2): A = y, the doors shut behind (B -> 0 by 16, door 3 drops into place), sound 132;
 - push platforms (fd2a): the 351 x 320 loop; electric crushers: y = J + 64 cos E, the bolt hurts (not with the metal shield);
 - corkscrew doors (fd2a): held switch opens the door 2 a frame, released it closes 1 a frame; shield givers: Milla only, a monitor;
 - fd2b: the rotating barrier turns 90 per button push; the big lift rises while a switch is held, falls back; the alarm state;
 - speed boosters (fd3a): ground speed 15 (sound 379); orbit beams: 240 px around the base, the ray burns at H >= 55;
 - the laser tunnel (fd4a): the 400-frame cycle with the column firing at B in [300, 365).
usage: test_fd_gimmicks.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, math, collections, json
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
FRAMES = os.path.join(HERE, '..', 'build', 'frames')
CHARS = (0, 2, 4)
NAME = {0: 'lilac', 2: 'carol', 4: 'milla'}
DY = 14                                                    # port y = MV.Y + 14 (ORIG_REF_DY)

def run(script, frames, env, extra=None):
    """sids: (frame, id); obs: (frame, fields with 'type'); rows: player rows by frame; dbg: FP_DBG_FDG rows"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_ENERGY', 'FP_DBG_FDG')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=900)
    os.unlink(name)
    fr, sids, obs, rows, dbg = 0, [], [], {}, {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fr = int(t[1]); rows[fr] = dict(zip(t[2::2], t[3::2]))
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))
        elif t[0] == 'ob':
            d = dict(zip(t[4::2], t[5::2])); d['type'] = int(t[2]); obs.append((int(t[1]), d))
        elif t[0] == 'fdg': dbg[int(t[1])] = t
    return sids, obs, rows, dbg

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def F(d, k): return float(d[k])
def env(lv, c, track='', dbg=False):
    e = {'FP_LEVEL': lv, 'FP_CHAR': str(c)}
    if track: e['FP_TRACK'] = track
    if dbg: e['FP_DBG_FDG'] = '1'
    return e
def at(obs, f, typ):
    return [d for g, d in obs if g == f and d['type'] == typ]
def near(obs, f, typ, x, y, r=3):
    return [d for d in at(obs, f, typ) if abs(F(d, 'x') - x) <= r and abs(F(d, 'y') - y) <= r]
def jsonobjs(n, cls):
    d = json.load(open(os.path.join(FRAMES, 'frame_%d.json' % n)))
    return [(o['x'], o['y']) for o in d['objects'] if o['cls'] == cls and o['x'] is not None and o['x'] >= 0 and o['y'] >= 0]
INV = '3 SETV others E 99999;'
def hp(rows, f): return int(rows[f]['hp'])

# ---- placements per act (importer FD_GIM + the reused Battle Glacier / Thermal Base types; RE on_frame_66..73_init)
COUNTS = {
    'fd1a': {2424: 32, 2425: 39, 2752: 16, 2753: 16, 2754: 39, 2755: 8, 2756: 8, 2757: 8, 2758: 11, 2759: 45, 2760: 3, 2761: 3, 2762: 15, 2763: 3, 2794: 2},   # + the jumbotron / screen (session 26)
    'fd1b': {2424: 24, 2425: 27, 2752: 9, 2753: 9, 2754: 13, 2755: 4, 2756: 4, 2757: 4, 2758: 8, 2759: 26, 2760: 3, 2761: 3, 2762: 6, 2764: 10, 2765: 20, 2766: 20, 2767: 10, 2768: 5, 2769: 10},
    'fd2a': {1890: 2, 1891: 3, 1892: 9, 1893: 3, 1894: 16, 2424: 15, 2752: 9, 2753: 9, 2754: 10, 2758: 1, 2759: 12, 2770: 48, 2771: 48, 2772: 2, 2773: 25, 2774: 11, 2775: 10, 2776: 10, 2777: 10, 2778: 5, 2779: 5, 2780: 2},
    'fd2b': {1890: 1, 1891: 1, 1892: 3, 1893: 1, 1894: 8, 2420: 30, 2424: 2, 2755: 5, 2756: 5, 2757: 5, 2772: 24, 2773: 17, 2774: 11, 2775: 1, 2776: 1, 2777: 1, 2778: 1, 2779: 1, 2780: 3, 2781: 4, 2782: 4, 2783: 3, 2784: 6},
    'fd3a': {1890: 1, 1891: 1, 1892: 3, 1893: 1, 2424: 13, 2752: 10, 2753: 10, 2754: 18, 2785: 28, 2786: 30, 2787: 36, 2788: 36, 2789: 36, 2793: 14},   # + the back pillars
    'fd3b': {1890: 1, 1891: 1, 1892: 3, 1893: 1, 1894: 10, 2424: 7, 2752: 3, 2753: 3, 2754: 12, 2785: 12, 2786: 18, 2787: 12, 2788: 12, 2789: 12, 2793: 14},
    'fd4a': {2415: 14, 2424: 7, 2752: 11, 2753: 11, 2754: 4, 2762: 10, 2790: 103, 2791: 1, 2792: 26},
    'fd4b': {2415: 2, 2790: 2}}
TYPES = list(range(2752, 2800)) + [1890, 1891, 1892, 1893, 1894, 1914, 2415, 2420, 2424, 2425]
ALL = ','.join('%d:-1' % t for t in TYPES)
for lv, want in COUNTS.items():
    s, obs, rows, dbg = run('', 3, env(lv, 0, ALL))
    got = collections.Counter(d['type'] for f, d in obs if f == 2)
    check('%s_placements' % lv, dict(got) == want, str(dict(sorted(got.items()))))

# ---- transporter pipes (RE 15188-15201): every on-map start of every act ends on a pad (Y = pad - 16) or at the level's edge
ACTS = {'fd1a': 66, 'fd1b': 67, 'fd2a': 68, 'fd3a': 70, 'fd3b': 71, 'fd4a': 72}
for lv, n in ACTS.items():
    pads = jsonobjs(n, 'objecttransporter')
    bad = []
    for (x, y) in jsonobjs(n, 'objecttransporterstart'):
        s, obs, rows, dbg = run(INV + '5 TP %d %d' % (x, y - 10), 300, env(lv, 0, dbg=True))
        ride = [t for f, t in sorted(dbg.items()) if t[3] == '95']
        if not ride: bad.append(('no ride', x, y)); continue
        f1 = int(ride[-1][1]); end = dbg.get(f1 + 1)
        X, Y = float(end[5]), float(end[7])
        steps = [(float(a[5]), float(a[7])) for a in ride]
        moves = all(abs(steps[i + 1][0] - steps[i][0]) + abs(steps[i + 1][1] - steps[i][1]) <= 52 for i in range(len(steps) - 1))
        onpad = any(abs(px - X) < 1 and abs(py - 16 - Y) < 1 for px, py in pads)
        edge = float(ride[-1][7]) <= 20 or float(ride[-1][5]) <= 20
        st = rows.get(f1 + 3, {}).get('st')
        if not (moves and (onpad or edge) and st == '0'): bad.append((x, y, X, Y, moves, st))
    check('%s_pipes_all_end' % lv, not bad, str(bad[:4]))
for c in CHARS:   # fd1a's first pipe: (1152, 416) up, right, down, right, down onto the pad at (1664, 640) for the three characters
    s, obs, rows, dbg = run(INV + '5 TP 1152 406', 120, env('fd1a', c, dbg=True))
    ride = [t for f, t in sorted(dbg.items()) if t[3] == '95']
    f1 = int(ride[-1][1]) if ride else 0; end = dbg.get(f1 + 1)
    ok = ride and float(end[5]) == 1664 and float(end[7]) == 640 - 16
    ys = [float(t[7]) for t in ride[:4]]
    check('pipe_fd1a_%s' % NAME[c], bool(ok) and ys[1] - ys[2] == 20 and abs(F(rows[f1 + 20], 'x') - 1664) < 2 and rows[f1 + 20]['gnd'] == '1',
          'end %s ys %s' % (end[5:8:2] if end else None, ys))
    check('pipe_fd1a_sounds_%s' % NAME[c], len(sid(s, 345)) >= 5 and [f for f in sid(s, 346) if f1 <= f <= f1 + 3], '345 %s 346 %s' % (sid(s, 345), sid(s, 346)))

# ---- zap beams (fd1a; RE 15203-15212, RV zap1: scale 24 from the start, C 30 at D 60, D wraps after 80)
s, obs, rows, dbg = run(INV + '3 TP 1440 500', 200, env('fd1a', 0, '2755:-1,2756:-1'))
cz = [(f, d) for f, d in obs if d['type'] == 2755 and F(d, 'x') == 1536]
ds = [F(d, 'a3') for f, d in cz]
c30 = [f for f, d in cz if F(d, 'a2') == 29]
beam = [(f, d) for f, d in obs if d['type'] == 2756 and F(d, 'x') == 1536]
check('zap_cycle', max(ds) <= 80 and min(ds) >= 0 and len(c30) >= 2 and c30[1] - c30[0] == 80, 'D %s..%s C29 at %s' % (min(ds), max(ds), c30[:3]))
check('zap_scale24_dir', all(F(d, 'a0') == 24 for f, d in beam) and any(d['dir'] == '1' for f, d in beam) and any(d['dir'] == '0' for f, d in beam))
check('zap_sound358', len(sid(s, 358)) >= 2)
for c in CHARS:   # standing in the sweep: hurt while it fires (hp drops), not without it
    s, obs, rows, dbg = run('3 TP 1450 650', 260, env('fd1a', c))   # under the left end of the sweep (it fires at A = B: 240 / 300)
    lost = [f for f in range(5, 259) if hp(rows, f + 1) < hp(rows, f)]
    check('zap_hurts_%s' % NAME[c], len(lost) >= 1, 'hp %d -> %d' % (hp(rows, 5), hp(rows, 259)))

# ---- spiral spark makers (fd1a; RE 15221-15228): every 150 frames two sparks (A 0 / 180) spiral down from the maker
s, obs, rows, dbg = run(INV + '3 TP 5560 700', 330, env('fd1a', 0, '2758:-1,2881:-1'))
mk = [(f, F(d, 'a0')) for f, d in obs if d['type'] == 2758 and F(d, 'x') == 5560]
resets = [f for f, a in mk if a == -90]
sp = collections.Counter(f for f, d in obs if d['type'] == 2881 and abs(F(d, 'x') - 5560) <= 41)
born = [f for f in sorted(sp) if sp[f] >= 2 and sp.get(f - 1, 0) == 0]
check('sparks_cycle', len(resets) >= 2 and resets[1] - resets[0] == 151, str(resets[:3]))
check('sparks_spawn_pairs', len(born) >= 1, str(born[:3]))
check('sparks_sound274', len(sid(s, 274)) >= 1)
s, obs, rows, dbg = run('3 TP 5560 760', 300, env('fd1a', 2))
check('sparks_hurt', hp(rows, 299) < hp(rows, 5), '%d -> %d' % (hp(rows, 5), hp(rows, 299)))

# ---- downhill platforms (fd1a (4112, 640); RE 15229-15238): ridden down the rail, the rider carried; back home once left
for c in CHARS:
    s, obs, rows, dbg = run(INV + '3 TP 4112 600', 330, env('fd1a', c, '2760:-1'))
    pl = {f: d for f, d in obs if d['type'] == 2760 and F(d, 'x') < 5000}
    on = all(abs(F(rows[f], 'x') - F(pl[f], 'x')) < 2 for f in (60, 90, 120))
    check('downhill_ride_%s' % NAME[c], F(pl[100], 'x') > 4600 and F(pl[100], 'y') > 900 and on and len(sid(s, 359)) >= 1,
          'plat %s %s player %s' % (pl[100]['x'], pl[100]['y'], rows[100]['x']))
for c in CHARS:   # the route: ride it down, jump to the ledge at the rail's foot (Carol: off the wall), on to x > 5200
    s, obs, rows, dbg = run('3 TP 4112 600;130 400 R;130 145 J;250 262 J', 400, env('fd1a', c))
    check('downhill_route_%s' % NAME[c], F(rows[399], 'x') > 4850 and F(rows[399], 'y') < 900, '%s %s' % (rows[399]['x'], rows[399]['y']))
s, obs, rows, dbg = run(INV + '3 TP 4112 600;200 230 R;201 203 J', 600, env('fd1a', 0, '2760:-1'))
pl = {f: d for f, d in obs if d['type'] == 2760 and F(d, 'x') < 5000}
check('downhill_returns', F(pl[599], 'x') == 4112 and F(pl[599], 'y') == 640 and F(pl[599], 'a4') == 0, '%s %s' % (pl[599]['x'], pl[599]['y']))

# ---- airlocks (fd1b (1664, 1440); RE 15359-15364, RV al2: D 1 within 159 px, A / B / C + 16 in turn, sounds 279 then 381)
s, obs, rows, dbg = run(INV + '3 TP 1600 1380', 120, env('fd1b', 0, '2764:-1,2765:-1'))
ab = {f: d for f, d in obs if d['type'] == 2764 and F(d, 'x') == 1664}
f1 = min(f for f, d in ab.items() if F(d, 'a3') == 1)
A = [F(ab[f1 + k], 'a0') for k in range(0, 14)]
check('airlock_opening', A[0] == 16 and A[11] == 192 and F(ab[f1 + 24], 'a1') == 192 and F(ab[f1 + 36], 'a2') == 192,
      'A %s B %s C %s' % (A[:4], ab[f1 + 24]['a1'], ab[f1 + 36]['a2']))
check('airlock_sounds', sid(s, 279) and sid(s, 381) and sid(s, 381)[0] > sid(s, 279)[0], '279 %s 381 %s' % (sid(s, 279), sid(s, 381)))
check('airlock_drop', F(rows[110], 'y') > 1440 + 100, rows[110]['y'])

# ---- pressure doors (fd1b; RE 15373-15382 / 15566, RV pd2: A = y, B 192 -> 0 by 16 once she passes, door 3 drops 192 into place)
for c in CHARS:   # the pair at (6270, 736): open (door 1 y 544, door 2 928, door 3 544) until she runs past, then shut at 736
    s, obs, rows, dbg = run(INV + '3 TP 6200 760;5 90 R', 100, env('fd1b', c, '2767:-1,2768:-1'))
    pd = collections.defaultdict(list)
    for f, d in obs:
        if d['type'] == 2767 and F(d, 'x') == 6270: pd[f].append(d)
    k1 = [k for k, d in enumerate(pd[2]) if F(d, 'a1') < 0][0]; k2 = 1 - k1         # door 1: ctor B -192 (from above), door 2: +192
    d1 = {f: v[k1] for f, v in pd.items()}; d2 = {f: v[k2] for f, v in pd.items()}
    d3 = {f: d for f, d in obs if d['type'] == 2768 and F(d, 'x') == 6270}
    check('pdoor_open_%s' % NAME[c], F(d1[2], 'y') == 544 and F(d2[2], 'y') == 928 and F(d3[2], 'y') == 544 and F(d1[2], 'a2') == 0,
          '%s %s %s' % (d1[2]['y'], d2[2]['y'], d3[2]['y']))
    shut = [f for f in sorted(d1) if F(d1[f], 'a2') == 2]
    check('pdoor_shut_%s' % NAME[c], shut and all(F(d[99], 'y') == 736 for d in (d1, d2, d3)) and [f for f in sid(s, 132) if f > 5],
          'shut at %s y %s %s %s' % (shut[:1], d1[99]['y'], d2[99]['y'], d3[99]['y']))

# ---- push platforms (fd2a; RE 15622-15629): Q delays (32 k), then up 3 x 117, right 4 x 80, down 3 x 117, left 4 x 80
s, obs, rows, dbg = run('', 700, env('fd2a', 0, '2770:-1'))
byf = collections.defaultdict(list)
for f, d in obs:
    if d['type'] == 2770: byf[f].append(d)
rng = set()
for k in range(len(byf[1])):        # every platform (tracked by its list index) runs the whole 320 x 351 loop
    xs = [F(byf[f][k], 'x') for f in range(1, 700)]; ys = [F(byf[f][k], 'y') for f in range(1, 700)]
    rng.add((max(xs) - min(xs), max(ys) - min(ys)))
check('push_loop', len(byf[1]) == 48 and rng == {(320.0, 351.0)}, str(rng))
dl = collections.Counter(F(d, 'a0') for d in byf[0])   # frame 0 is after the first update: Q 32 k - 1 (Q 0 stays 0), 4 of each
check('push_delays', sorted(dl) == [0.0] + [32.0 * k - 1 for k in range(1, 12)] and set(dl.values()) == {4}, str(sorted(dl.items())))

# ---- electric crushers (fd2a; RE 15630 / 15631, start E = 45 k): N = J + 64 cos E, E + 2; the bolt hurts
s, obs, rows, dbg = run('', 60, env('fd2a', 0, '2775:-1'))
cr = [(f, d) for f, d in obs if d['type'] == 2775 and F(d, 'x') == 3936]
ok = all(abs(F(d, 'y') - math.floor(1600 + 64 * math.cos(math.radians(F(d, 'vx'))))) <= 1 for f, d in cr)
check('crusher_path', len(cr) > 50 and ok)
for c in CHARS:
    s, obs, rows, dbg = run('3 TP 3936 1700', 200, env('fd2a', c))   # on the floor (feet 1728) under the crusher: the bolt reaches her at its low point
    check('crusher_bolt_hurts_%s' % NAME[c], min(hp(rows, f) for f in range(10, 199)) < hp(rows, 5) and len(sid(s, 371)) >= 1)

# ---- corkscrew doors (fd2a; RE 15609-15621): standing on the switch (2624, 1648) opens its door (2671, 1711) 2 a frame
for c in CHARS:
    s, obs, rows, dbg = run(INV + '3 TP 2624 1620;80 100 R', 200, env('fd2a', c, '2778:-1,2779:-1'))
    dr = {f: d for f, d in obs if d['type'] == 2778 and F(d, 'a1') == 2671}
    opened = min(F(dr[f], 'a2') for f in dr)
    st = [f for f in sorted(dr) if F(dr[f], 'a2') < 0][:1]
    check('cdoor_%s' % NAME[c], opened == -128 and F(dr[199], 'a2') > opened and len(sid(s, 49)) >= 1,
          'min C %s C@199 %s' % (opened, dr[199]['a2']))

# ---- shield givers (fd2a (3904, 2391); RE 15592-15608): Milla only gets her monitor (a wood shield from the first)
for c in CHARS:
    s, obs, rows, dbg = run(INV + '3 TP 3850 2360', 160, env('fd2a', c, '2780:-1,151:-1,18:-1'))
    g = {f: d for f, d in obs if d['type'] == 2780 and F(d, 'x') == 3904}
    mons = [d for f, d in obs if f == 159 and d['type'] in (151, 18) and abs(F(d, 'x') - 3888) < 4]
    if c == 4: check('shieldgiver_milla', F(g[159], 'a0') >= 3 and len(mons) == 1 and len(sid(s, 241)) == 1, 'A %s mons %d' % (g[159]['a0'], len(mons)))
    else: check('shieldgiver_%s_none' % NAME[c], F(g[159], 'a0') == 0 and not mons)

# ---- fd2b: the rotating barrier (RE 15729-15736, start 15989), the big lift (15758-15777), the alarm (15802-15811)
for c in CHARS:   # pushing the button (a wall push) turns the barrier B + 90 (sound 49)
    s, obs, rows, dbg = run(INV + '3 TP 940 4920;10 40 R', 120, env('fd2b', c, '2781:-1,2782:-1'))
    bs = sorted(set(F(d, 'a1') for f, d in obs if d['type'] == 2781 and F(d, 'x') == 704))
    check('barrier_turns_%s' % NAME[c], len(bs) >= 2 and any(b % 90 == 0 and b >= 90 for b in bs) and len(sid(s, 49)) >= 1, str(bs))
for c in CHARS:   # standing on a switch (64 px left of the lift's centre) the lift rises with her (sound 49 on the press)
    s, obs, rows, dbg = run(INV + '3 TP 6176 5600', 200, env('fd2b', c, '2783:-1'))
    lf = {f: d for f, d in obs if d['type'] == 2783 and F(d, 'x') == 6240 and F(d, 'a1') == 5632}
    check('biglift_rises_%s' % NAME[c], F(lf[150], 'fx') < 5632 - 100 and F(rows[150], 'y') - DY < 5632 - 100 and len(sid(s, 49)) >= 1,
          'J %s player %s' % (lf[150]['fx'], rows[150]['y']))
s, obs, rows, dbg = run(INV + '3 TP 6240 5600', 200, env('fd2b', 0, '2783:-1'))   # on the centre (no switch): it stays home
lf = {f: d for f, d in obs if d['type'] == 2783 and F(d, 'x') == 6240 and F(d, 'a1') == 5632}
check('biglift_center_stays', all(F(lf[f], 'fx') == 5632 for f in range(20, 200)) and rows[199]['gnd'] == '1', lf[199]['fx'])
for c in CHARS:   # fd2b's alarm (15802 / 15807): A 1 below y 5340, A 2 (sound 372) 400 frames later; never for Milla
    s, obs, rows, dbg = run(INV + '3 TP 6240 5600', 430, env('fd2b', c, dbg=True))
    a = [int(dbg[f][16]) for f in (10, 429)]
    if c == 4: check('siren_milla_off', a == [0, 0] and not sid(s, 372), str(a))
    else: check('siren_%s' % NAME[c], a == [1, 2] and len(sid(s, 372)) == 1, '%s 372 %s' % (a, sid(s, 372)))

# ---- speed boosters (fd3a (1935, 391); RE col_26_69): ground speed +-15, sound 379
for c in CHARS:
    s, obs, rows, dbg = run('3 TP 1870 380;3 40 R', 60, env('fd3a', c))
    sp = max(abs(F(rows[f], 'gsp')) for f in range(5, 59))
    check('booster_%s' % NAME[c], sp >= 14.9 and len(sid(s, 379)) >= 1, 'max gsp %.2f' % sp)

# ---- orbit beams (fd3a; RE 16040-16046, 16201): 240 px around the base, F - 1 a frame, the ray's A = H (burns at 55+)
s, obs, rows, dbg = run(INV, 120, env('fd3a', 0, '2788:-1,2789:-1'))
ob = [(f, d) for f, d in obs if d['type'] == 2788 and F(d, 'a0') == 4832]
r = [math.hypot(F(d, 'x') - 4832, F(d, 'y') - 544) for f, d in ob]
check('orbit_radius', ob and all(abs(x - 240) <= 2 for x in r), '%s..%s' % (min(r), max(r)))
byf = collections.defaultdict(list)
for f, d in ob: byf[f].append(F(d, 'a2'))          # the first beam of the base (list order is stable), F per frame
o1 = [byf[f][0] for f in range(10, 40)]
check('orbit_turns_clock', all((b - a) % 360 == 359.0 for a, b in zip(o1, o1[1:])), str(o1[:4]))

# ---- the laser tunnel (fd4a; RE 16674-16681, start B 160): B + 1 while she is in the arena, fires at 300
s, obs, rows, dbg = run(INV + '3 TP 2400 6300', 200, env('fd4a', 0, '2791:-1'))
lt = {f: d for f, d in obs if d['type'] == 2791}
bs = [F(lt[f], 'a1') for f in sorted(lt)]
check('tunnel_cycle', bs and bs[-1] > 160 and all(b2 - b1 in (0, 1, -399) for b1, b2 in zip(bs, bs[1:])), '%s..%s' % (bs[0], bs[-1]))
s, obs, rows, dbg = run('3 TP 2600 6300', 320, env('fd4a', 0, '2791:-1'))
check('tunnel_fires_hurts', len(sid(s, 270)) >= 1 and hp(rows, 319) < hp(rows, 5), '270 %s hp %d -> %d' % (sid(s, 270), hp(rows, 5), hp(rows, 319)))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
