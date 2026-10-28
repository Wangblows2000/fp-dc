#!/usr/bin/env python3
"""Host tests for the Trap Hideout gimmicks (session 21b; src/game/th_gim.c, build/re/spec_th_gimmicks.md): the TH1 scan lasers
(beam cycle, sweep heads, beam lengths and positions, moving heads, damage every 7 frames with no knock-back, Milla's shield
blocking a beam, Carol's Wild Kick pierced; RV las1 / lasd_c / lasd_m / lasm_m), the shurikens (square paths, static in TH3,
10 damage; RV th2a / th3a), the pillar platforms (shared path-platform rule, phase per index; RV th2a / th3a), the collapsing
platform (= Jade Creek's; RV th2a), the barrel ride (M 98; RV th2b), the backlights, the flames (scenery), the importer rows,
and the critical path past the traps (TH1 shaft and floor lasers, TH2 pillar tower) for Carol and Milla.
usage: test_th_gimmicks.py [data_dir]   (default: converted2; the session's TH data: cv)"""
import subprocess, sys, os, tempfile, struct, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
PLAY = os.path.join(HERE, 'play')
CHARS = ((2, 'carol'), (4, 'milla'))

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_THG_G3')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    rows, ev, obs = [], [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'sid':
            ev.append((len(rows), int(t[1])))
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2])}
            for i in range(4, len(t) - 1, 2):
                try: d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
                except ValueError: pass
            obs.append(d)
    return rows, ev, obs

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sids(ev, n): return [i for i, s in ev if s == n]
def MY(r): return r['y'] - 14.0                        # the original's MV.Y (the port's y = MV.Y + ORIG_REF_DY)
def at(obs, f, t, x=None, y=None):
    return [o for o in obs if o['f'] == f and o['type'] == t and (x is None or int(o['x']) == x) and (y is None or int(o['y']) == y)]

def lvobjs(lv):
    """(type, anim, x, y) of a level file's objects (FPL2 header: magic, W, H, nrec, nobj, ...)"""
    d = None
    for fn in os.listdir(os.path.join(DATA, lv)):
        b = open(os.path.join(DATA, lv, fn), 'rb').read()
        if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

# ---- data: the importer's TH_GIM rows (types 1620-1630, member | layer << 8; LevelTHCollapsingPlatform -> 1378 member 2)
want = {'th1': {1620: 23, 1621: 23, 1536: 7},
        'th2': {1622: 29, 1623: 29, 1537: 58, 1625: 4, 1626: 4, 1627: 16, 1628: 3, 1536: 25, 1378: 6},
        'th3': {1622: 4, 1623: 4, 1537: 62, 1625: 3, 1626: 3, 1628: 1, 1536: 19, 1378: 9}}
for lv, w in want.items():
    objs = lvobjs(lv); cnt = {}
    for t, a, x, y in objs:
        if x >= 0 and y >= 0: cnt[t] = cnt.get(t, 0) + 1
    check('data_%s_types' % lv, all(cnt.get(t, 0) == n for t, n in w.items()), str({t: cnt.get(t, 0) for t in w}))
objs = lvobjs('th1')
check('data_th1_beam_members', sorted({a & 0xff for t, a, x, y in objs if t == 1620}) == [0, 1, 2, 3, 4])
check('data_th1_head_members', sorted({a & 0xff for t, a, x, y in objs if t == 1621}) == [0, 2, 3, 4, 5])
check('data_th2_collapse_member2', {a & 0xff for t, a, x, y in lvobjs('th2') if t == 1378} == {2})

# ---- scan lasers (TH1). The original's g3 is not reset by the act (RV: 107 .. 524604 at the first frame of TH traces), so the
# tests align the port's phase with RV las1 (FP_THG_G3=223: trace frame f = port frame f + 2, head B 225.027 at trace 0)
L1 = {'FP_LEVEL': 'th1', 'FP_CHAR': '2', 'FP_THG_G3': '223'}
rows, ev, obs = run('5 SETV others E 99999\n', 420, dict(L1, FP_TRACK='1620:-1,1621:255'))
HEADB = {0: 225.027, 50: 279.356, 100: 311.723, 150: 246.154}
hb = {f: at(obs, f + 2, 1621) for f in HEADB}
check('laser_sweep_head_angle', all(h and abs(h[0]['a1'] - v) < 2e-3 for (f, v), h in zip(HEADB.items(), hb.values())),
      str({f: round(h[0]['a1'], 3) if h else None for f, h in hb.items()}))
SWEEP = {30: (248, 666, 127), 60: (262, 666, 127), 90: (269, 662, 170), 120: (262, 666, 127), 150: (247, 666, 127),
         180: (240, 661, 174), 210: (248, 666, 127), 240: (262, 666, 127), 300: (262, 666, 127), 360: (240, 661, 174), 400: (252, 668, 166)}
pos_ok, len_ok, info = 0, 0, []
for f, (x, y, A) in SWEEP.items():
    b = at(obs, f + 2, 1620, x, y)
    pos_ok += bool(b); len_ok += bool(b) and abs(b[0]['a0'] - A) <= 2
    if not b or abs(b[0]['a0'] - A) > 2: info.append((f, x, y, A, [(int(o['x']), int(o['y']), int(o['a0'])) for o in at(obs, f + 2, 1620) if abs(o['x'] - x) < 20 and abs(o['y'] - y) < 20]))
check('laser_sweep_position_RV', pos_ok == len(SWEEP), str(info[:3]))            # Active::update_action_point (rotated hot spot / action point truncated)
check('laser_sweep_length_RV_2px', len_ok == len(SWEEP), str(info[:3]))
STATIC = [(250, 312, 137), (135, 2177, 104)]   # down / left beams on stopped heads (RV las1 f200; the moving heads' phase depends on the act start)
got = {(x, y): at(obs, 202, 1620, x, y) for x, y, A in STATIC}
check('laser_static_lengths_RV', all(got[(x, y)] and int(got[(x, y)][0]['a0']) == A for x, y, A in STATIC),
      str({k: int(v[0]['a0']) if v else None for k, v in got.items()}))
b = [o for o in obs if o['type'] == 1620 and int(o['x']) == 250 and int(o['y']) == 312]
sw = [o['f'] - 2 for i, o in enumerate(b[1:]) if o['dir'] != b[i]['dir'] and o['f'] > 10]
check('laser_cycle_RV', sw[:4] == [89, 180, 269, 360], 'static beam switches at trace frames %s' % sw[:4])
s = [o for o in obs if o['type'] == 1620 and int(o['x']) == 240 and int(o['y']) == 661]
check('laser_sweep_burns_in_other_half', any(o['f'] == 100 and o['dir'] == 1 for o in at(obs, 100, 1620)) and
      all(o['dir'] == 0 for o in at(obs, 50, 1620) if o['a3'] and int(o['y']) in range(650, 680)))
check('laser_beam_hidden_drawn_by_module', all(o['hd'] == 1 for o in at(obs, 100, 1620)))
# moving heads (BallMovement 2 px a frame, turn round on the background); Head4 at (249, 288) stopped on its limiter
h2 = [o for o in obs if o['type'] == 1621]
check('laser_head_sweep_static', all(int(o['x']) == 255 and int(o['y']) == 647 for o in h2))
rows2, ev2, obs2 = run('5 SETV others E 99999\n', 120, dict(L1, FP_TRACK='1621:-1,40:-1'))
hd = {}
for o in obs2:
    if o['type'] == 1621: hd.setdefault(o['f'], []).append((int(o['x']), int(o['y']), int(o['dir'])))
moved = [(a, b_) for a, b_ in zip(hd[60], hd[61]) if a != b_]
check('laser_heads_move_2px', moved and all(abs(a[0] - b_[0]) + abs(a[1] - b_[1]) in (2, 0) for a, b_ in moved), str(moved[:4]))
check('laser_head_stops_on_limiter', any(x == 249 and y == 288 for x, y, d in hd[100]) and
      not [o for o in obs2 if o['type'] == 40 and o['f'] == 100 and int(o['x']) == 269 and int(o['y']) == 306], str([h for h in hd[100] if h[1] == 288]))
bounce = [o for o in obs2 if o['type'] == 1621 and int(o['y']) == 2335 and o['f'] in (10, 30)]
check('laser_head_bounces', any(o['dir'] == 0 for o in bounce if o['f'] == 30) and any(o['dir'] == 16 for o in bounce if o['f'] == 10),
      str([(o['f'], int(o['x']), o['dir']) for o in bounce][:6]))

# damage: standing in a burning beam (RV lasd_c / lasd_m: R every 7 frames, 5 damage (one half petal), B = G = 0, M 8 for 22 frames)
for ch, nm in CHARS:
    rows, ev, _ = run('60 TP 250 430\n', 200, dict(L1, FP_CHAR=str(ch)))
    hurt = [int(r['f']) for i, r in enumerate(rows[1:]) if r['hp'] < rows[i]['hp']]
    gaps = sorted({b_ - a for a, b_ in zip(hurt, hurt[1:]) if b_ - a < 20})
    check('laser_hurt_every_7_%s' % nm, len(hurt) >= 4 and gaps == [7], 'hurt at %s' % hurt[:8])
    check('laser_hurt_one_halfpetal_%s' % nm, all(rows[0]['hp'] - r['hp'] <= 14 for r in rows) and rows[hurt[0]]['hp'] == rows[hurt[0] - 1]['hp'] - 1 if hurt else False)
    r1 = [r for r in rows if hurt and hurt[0] <= r['f'] < hurt[0] + 22]
    check('laser_no_knockback_%s' % nm, r1 and all(r['x'] == 250 and abs(MY(r) - 434) < 0.01 for r in r1), str([(r['f'], r['x'], MY(r)) for r in r1][:3]))
    m8 = [r['f'] for r in rows if r['mm'] == 8]
    check('laser_m8_22_frames_%s' % nm, hurt and all(f in m8 for f in range(hurt[0], hurt[0] + 22)) and hurt[0] + 22 not in m8,
          'M8 %s..' % m8[:3])
rows, ev, obs = run('40 TP 250 430\n45 300 BU\n', 300, dict(L1, FP_CHAR='4', FP_TRACK='1620:-1'))
b = [o for o in obs if o['type'] == 1620 and int(o['x']) == 250 and int(o['y']) == 312 and o['f'] >= 150]
check('laser_milla_shield_blocks_RV', b and all(int(o['a0']) == 78 for o in b), str(sorted({int(o['a0']) for o in b})))
check('laser_milla_shield_no_damage', all(r['hp'] == rows[150]['hp'] for r in rows[150:]) and rows[150]['mm'] == 31, 'hp %s mm %s' % (rows[150]['hp'], rows[150]['mm']))
# Carol's Wild Kick (M 44: invulnerable to enemies) is still burnt (RE 13491: M 44 with OTHERS.D 2)
rows, ev, _ = run('5 SETV others E 99999\n150 TP 250 430\n160 260 B\n', 260, dict(L1, FP_CHAR='2', FP_THG_G3='0'))
k = [r for r in rows if r['mm'] == 44]
check('laser_wild_kick_burnt', k and rows[-1]['hp'] < rows[150]['hp'], 'kick frames %d hp %s -> %s' % (len(k), rows[150]['hp'], rows[-1]['hp']))

# ---- shurikens: TH2 square paths (RV th2a: port frame f + 120 = trace f), TH3 static (14097), 10 damage, 16 degrees a frame
L2 = {'FP_LEVEL': 'th2', 'FP_CHAR': '2'}
rows, ev, obs = run('5 SETV others E 99999\n', 380, dict(L2, FP_TRACK='1626:-1,1625:-1,1622:-1,1623:-1'))
SHB = {0: [(2176, 1536), (1920, 1640), (6472, 1224), (6424, 960)], 1: [(2176, 1532), (1920, 1636), (6472, 1220), (6424, 956)],
       50: [(2316, 1472), (2052, 1568), (6596, 1144), (6564, 896)], 100: [(2464, 1528), (2124, 1664), (6588, 1336), (6712, 952)]}
for f, w in SHB.items():
    g = [(int(o['x']), int(o['y'])) for o in at(obs, f + 120, 1626)]
    check('shuriken_bases_RV_f%d' % f, g == w, str(g))
g = [(int(o['x']), int(o['y'])) for o in at(obs, 250, 1626)]
check('shuriken_square_path', all(2176 <= x <= 2464 or 1920 <= x <= 2144 or 6424 <= x <= 6712 for x, y in g), str(g))
s1, s2 = at(obs, 200, 1625), at(obs, 201, 1625)
check('shuriken_on_base_spin16', s1 and [(int(o['x']), int(o['y'])) for o in s1] == [(int(o['x']), int(o['y'])) for o in at(obs, 200, 1626)] and
      abs((s2[0]['ang'] - s1[0]['ang']) % 360 - 16) < 1e-3, str([(o['x'], o['y'], o['ang']) for o in s1][:2]))
PL = {0: [(2160, 1175), (2272, 1152), (2160, 767), (2312, 688)], 2: [(2160, 1173), (2272, 1154), (2160, 765), (2312, 690)]}
for f, w in PL.items():
    g = [(int(o['x']), int(o['y'])) for o in at(obs, f + 120, 1622)][:4]
    check('pillar_path_RV_f%d' % f, g == w, str(g))
pf = at(obs, 200, 1623); pp = at(obs, 200, 1622)
check('pillar_flame_under_pillar', pf and all(int(a['y']) == int(b_['y']) + 64 and int(a['x']) == int(b_['x']) for a, b_ in zip(pf, pp)))
rows, ev, obs = run('5 SETV others E 99999\n', 160, {'FP_LEVEL': 'th3', 'FP_CHAR': '4', 'FP_TRACK': '1626:-1,1622:-1'})
g = {(int(o['x']), int(o['y'])) for o in obs if o['type'] == 1626}
check('shuriken_th3_static', g == {(384, 2232), (456, 2232), (312, 1448)}, str(g))
g = [(int(o['x']), int(o['y'])) for o in at(obs, 120, 1622)]
check('pillar_th3_phase_RV', g == [(264, 792), (2576, 551), (11408, 800), (11440, 687)], str(g))   # E = (n - 1 - index) 180, RV th3a f0
for ch, nm in CHARS:
    rows, ev, _ = run('100 TP 2320 1500\n', 260, dict(L2, FP_CHAR=str(ch)))
    hurt = [(int(r['f']), rows[i]['hp'] - r['hp']) for i, r in enumerate(rows[1:]) if r['hp'] < rows[i]['hp']]
    check('shuriken_hurt_10_%s' % nm, hurt and hurt[0][1] == 2, str(hurt[:3]))

# ---- collapsing platform (Jade Creek's, member 2; RV th2a: landing at 1386.75, sound 426, she falls 37 frames after landing)
for ch, nm in CHARS:
    rows, ev, obs = run('150 TP 1680 1370\n', 260, dict(L2, FP_CHAR=str(ch), FP_TRACK='1379:-1'))
    land = next((int(r['f']) for r in rows if r['f'] > 150 and r['gnd'] == 1), None)
    fall = next((int(r['f']) for r in rows if land and r['f'] > land and r['gnd'] == 0), None)
    check('collapse_land_RV_%s' % nm, land and abs(MY(rows[land]) - 1386.75) < 0.01, 'land %s y %s' % (land, MY(rows[land]) if land else None))
    check('collapse_sound_426_%s' % nm, len(sids(ev, 426)) == 1, str(sids(ev, 426)))
    check('collapse_pieces_8_%s' % nm, len({o['x'] for o in obs if o['type'] == 1379}) >= 1 and max((len([o for o in obs if o['type'] == 1379 and o['f'] == f]) for f in range(150, 260)), default=0) == 8)
    check('collapse_fall_37_%s' % nm, land and fall and 34 <= fall - land <= 40, 'fall %s frames after landing' % (fall - land if fall and land else None))

# ---- the barrel ride (RV th2b: lands at Y = E - 54 (1018 .. 1019.5), M 98, Right: F + 0.15 - 0.05 friction, jump: G -7, M 1)
for ch, nm in CHARS:
    rows, ev, obs = run('60 TP 2648 990\n100 200 R\n260 300 L\n330 332 J\n', 380, dict(L2, FP_CHAR=str(ch), FP_TRACK='1628:2648'))
    ride = [r for r in rows if r['mm'] == 98]
    check('barrel_ride_m98_%s' % nm, ride and all(1018 <= MY(r) <= 1019.5 for r in ride if r['f'] < 150), str([MY(r) for r in ride][:5]))
    b = {o['f']: o for o in obs if o['type'] == 1628}
    acc = [round(b[f + 1]['a0'] - b[f]['a0'], 3) for f in range(120, 140) if f in b and f + 1 in b]
    check('barrel_accel_RV_%s' % nm, acc and all(abs(a - 0.1) < 1e-6 for a in acc), str(acc[:5]))
    check('barrel_carries_player_%s' % nm, all(abs(r['x'] - b[int(r['f'])]['a3']) < 1e-3 for r in ride if int(r['f']) in b and r['f'] < 320))
    j = next((r for r in rows if r['f'] > 320 and r['mm'] == 1), None)
    check('barrel_jump_RV_%s' % nm, j and abs(j['vy'] + 7) < 0.3 and 8 in sids(ev, 8) or (j and abs(j['vy'] + 7) < 0.3), 'vy %s' % (j['vy'] if j else None))
    check('barrel_roll_sound_264_%s' % nm, len(sids(ev, 264)) >= 3, str(sids(ev, 264)[:4]))

# ---- backlights (13532: frame (g3 / 10) mod 6), flames (scenery: no damage): th_flow.c types 1536 / 1537 since the merge
rows, ev, obs = run('5 SETV others E 99999\n', 90, dict(L2, FP_TRACK='1536:-1,1537:-1'))
fr = {o['f']: o['fr'] for o in obs if o['type'] == 1536}
check('backlight_frame_g3', all(fr[f] == ((f + 1) // 10) % 6 for f in range(20, 80)), str([fr[f] for f in range(20, 80, 10)]))
fl = [o for o in obs if o['type'] == 1537 and o['f'] == 60]
check('flame_animated_scenery', fl and len({o['fr'] for o in obs if o['type'] == 1537}) > 1)

# ---- critical path past the traps (host route-search inputs, replayed: tests/play/th1_<char>_gim.txt = the act start through the
# shaft's first laser rooms (down beams, sweeps), th1_<char>_floor.txt = the floor corridor's up beams to the Laser Iris,
# th2_<char>_pillars.txt = the pillar tower; the TP lines put the player at the section start)
for ch, nm in CHARS:
    rf = os.path.join(PLAY, 'th1_%s_gim.txt' % nm)
    if os.path.exists(rf):
        rows, ev, _ = run(open(rf).read(), 900, {'FP_LEVEL': 'th1', 'FP_CHAR': str(ch)})
        deep = max(MY(r) for r in rows)
        check('path_th1_shaft_lasers_%s' % nm, deep > 1100 and min(r['hp'] for r in rows) > 0, 'Y %.0f, min hp %s' % (deep, min(r['hp'] for r in rows)))
    else: check('path_th1_shaft_lasers_%s' % nm, False, 'no route file')
    rf = os.path.join(PLAY, 'th1_%s_floor.txt' % nm)
    if os.path.exists(rf):
        rows, ev, _ = run(open(rf).read(), 800, {'FP_LEVEL': 'th1', 'FP_CHAR': str(ch)})
        far = max((r['x'] for r in rows if MY(r) > 2150), default=0)
        check('path_th1_floor_lasers_%s' % nm, far > 2300 and min(r['hp'] for r in rows) > 0, 'x %.0f below y 2150, min hp %s' % (far, min(r['hp'] for r in rows)))
    else: check('path_th1_floor_lasers_%s' % nm, False, 'no route file')
    rf = os.path.join(PLAY, 'th2_%s_pillars.txt' % nm)
    if os.path.exists(rf):
        rows, ev, _ = run(open(rf).read(), 700, {'FP_LEVEL': 'th2', 'FP_CHAR': str(ch)})
        top = min((MY(r) for r in rows if 7050 < r['x'] < 7300), default=9999)
        check('path_th2_pillar_tower_%s' % nm, top < 990 and min(r['hp'] for r in rows) > 0, 'top Y %.0f' % top)
    else: check('path_th2_pillar_tower_%s' % nm, False, 'no route file')

print('%d failures' % fails)
sys.exit(1 if fails else 0)
