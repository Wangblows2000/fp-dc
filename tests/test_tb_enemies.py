#!/usr/bin/env python3
"""Host tests for the Thermal Base enemies (session 21c; src/game/tb_enemy.c, src/game/enemy_kit.h, build/re/spec_tb_enemies.md)
against the original's traces (RV, build/orig_traces/tb_enemies: session1.sh + the same input scripts; frame numbers are the
trace's: an object state at its .xl row = the host's 'ob' frame + 2, a sound at its .snd frame = the host's sound frame + 1).
The TB acts are not yet playable from their starts on the host (the flow / gimmick modules: tb1's start lift), so each run parks
Lilac / Carol / Milla with per-frame TPs (a TP zeroes the speeds), as the trace's TP at 60 placed her.
  seeker (tb1 903, 1976): flight 2 px a frame on y = E + 40 sin F (RV positions), the stop within 99 px, the homing bullet (sound
          59, 3.75 px a frame, RV path), the 60-frame cooldown; the bullet hurts 10; Lilac / Carol hit it, Milla's thrown cube;
  push-up (tb1 5548, 2175): lasers at 106 / 113 / 121 (sound 437), the ground waves (sound 264, 5 px, a diamond every 3 frames),
          the flips every 102 frames, the head on the action point; beaten: the head flies and lands (sound 50), debris;
  lava squid (tb2 4442, 3032): shots 29 frames apart (sound 73), the turn (flag 2 from the start), the 0.2-decelerated dash;
  tank bot (tb4 2999, 2305): walk 2 px, pellets 16 / 34 frames into the attack (sound 28), G 90, the limiter turn;
  saucer (tb4 944, 2704): speed 0.1 a frame to 3, the limiter turn at x 1122, the bob, the legs, fireballs every 14 frames;
  defeats for Lilac / Carol / Milla (the shared Q20 hits), the crashes (keystone + sound 29), the debris of squid / saucer,
  no contact damage from the bodies.
usage: test_tb_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
HOFF = 2                                                     # trace row = host 'ob' frame + HOFF

def run(script, frames, env):
    """sids: (frame, id) in the trace's .snd numbering; obs: (frame, type, x, y, fields); rows: host player rows by frame"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    fr, sids, obs, rows = 0, [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
        elif t[0] == 'sid': sids.append((fr + 2, int(t[1])))        # played in host frame fr + 1 = the .snd frame fr + 2
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]) + HOFF, int(t[2]), float(d['x']), float(d['y']), d))
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def gaps(v): return [b - a for a, b in zip(v, v[1:])]
def near(a, b, tol): return a is not None and b is not None and abs(a - b) <= tol
def changes(obs, key):
    out, prev = [], None
    for f, t, x, y, d in obs:
        if d[key] != prev: out.append((f, float(d[key]))); prev = d[key]
    return out
def first(ch, v, after=0): return next((f for f, val in ch if val == v and f > after), None)
def of(obs, t): return [o for o in obs if o[1] == t]
def park(x, y, f0, f1, inv=True):
    return ('3 SETV others E %d\n' % (99999 if inv else 0)) + ''.join('%d TP %d %d\n' % (f, x, y) for f in range(f0, f1))
def hp(rows, f): return int(rows[f]['hp']) if f in rows else None
CH = (0, 2, 4)
T_SEEK, T_PU, T_HEAD, T_SQ, T_TANK, T_SAU, T_LEG1, T_BUL, T_LAS, T_LAS2, T_LAS3, T_PUDEB, T_SQLAS, T_DEB, T_TDEB, T_FBALL = \
    1824, 1825, 1826, 1827, 1828, 1829, 1830, 1856, 1857, 1858, 1859, 1860, 1861, 1862, 1863, 257

# ================================================================ seeker (tb1; RV seeker1: TP 650 1960 at 60)
for c in CH:
    sc = park(5330, 2150, 2, 60) + '60 TP 650 1960\n'
    s, obs, _ = run(sc, 320, {'FP_LEVEL': 'tb1', 'FP_CHAR': str(c), 'FP_TRACK': '%d:903,%d:-1' % (T_SEEK, T_BUL)})
    sk = {f: (x, y) for f, t, x, y, d in of(obs, T_SEEK)}
    rv = {64: (897, 1979), 100: (825, 2015), 136: (753, 1972), 139: (749, 1969), 217: (747, 1967), 250: (681, 1936)}
    bad = {f: (sk.get(f), v) for f, v in rv.items() if sk.get(f) != v}
    check('c%d_seeker_path_rv' % c, not bad, str(bad))
    D = changes(of(obs, T_SEEK), 'a0')
    check('c%d_seeker_attack_138_close_207_fly_217' % c, first(D, 3) == 138 and first(D, 4) == 207 and first(D, 0, 150) == 216 and first(D, 1, 150) == 217, str(D[:7]))
    sh = sid(s, 59)
    check('c%d_seeker_shot_sound_165_304' % c, 165 in sh and any(near(f, 304, 1) for f in sh), str(sh))
    bul = {f: x for f, t, x, y, d in of(obs, T_BUL) if x < 1000}
    rvb = {166: 749, 190: 659, 215: 565, 235: 490}
    bad = {f: (bul.get(f), v) for f, v in rvb.items() if bul.get(f) != v}
    check('c%d_seeker_bullet_path_rv' % c, not bad, str(bad))

# the bullet (it flies at y 1969, over a standing player's head: RV) hurts one in its way: 10, two half petals (col_5_44)
for c in (0, 2):
    sc = park(5330, 2150, 2, 60, inv=False) + park(650, 1962, 60, 200, inv=False) + '61 63 L\n'
    s, obs, rows = run(sc, 200, {'FP_LEVEL': 'tb1', 'FP_CHAR': str(c)})
    check('c%d_seeker_bullet_hurts_10' % c, sid(s, 17) and hp(rows, 60) - hp(rows, 199) == 2, '%s hp %s -> %s' % (sid(s, 17), hp(rows, 60), hp(rows, 199)))
# no contact damage from a body (RE: no class sets B): parked on the lava squid before its first shot
for c in CH:
    s, obs, rows = run(park(4442, 3020, 2, 46, inv=False), 46, {'FP_LEVEL': 'tb2', 'FP_CHAR': str(c), 'FP_TRACK': '%d:4442' % T_SQ})
    E = [float(d['a1']) for f, t, x, y, d in obs]
    check('c%d_squid_body_no_contact_damage' % c, E and max(E) >= 40 and hp(rows, 45) == hp(rows, 5), 'E %s hp %s -> %s' % (max(E or [0]), hp(rows, 5), hp(rows, 45)))
# defeats: Lilac's / Carol's attacks next to the stopped seeker (sound 24, defeat 23, the crash with a keystone: 29); Milla's
# thrown cube (held 62-140, thrown at 160) reaches it from her spot
for c in CH:
    if c == 4: sc = park(5330, 2150, 2, 60) + park(650, 1962, 60, 200) + '61 63 R\n62 140 X\n160 162 X\n'
    else: sc = park(5330, 2150, 2, 60) + '60 TP 650 1960\n' + park(730, 1975, 150, 175) + '151 153 X\n160 162 X\n168 170 X\n'
    s, obs, _ = run(sc, 300, {'FP_LEVEL': 'tb1', 'FP_CHAR': str(c), 'FP_TRACK': '%d:903' % T_SEEK})
    df = [f for f, t, x, y, d in obs if int(d['fl'], 16) & 0x40]
    gone = max(f for f, t, x, y, d in obs) if obs else None
    check('c%d_seeker_defeated_crashes_keystone' % c, sid(s, 23) and sid(s, 29) and df and gone and gone < 298 and sid(s, 29)[0] >= df[0],
          'hit %s def %s crash %s first %s gone %s' % (sid(s, 24), sid(s, 23), sid(s, 29), df[:1], gone))

# ================================================================ push-up (tb1 5548; RV pushup1: TP 5330 2150 at 60)
s, obs, _ = run(park(9000, 1200, 2, 60) + '60 TP 5330 2150\n', 320,
                {'FP_LEVEL': 'tb1', 'FP_CHAR': '0', 'FP_TRACK': '%d:5548,%d:5549,%d:-1,%d:-1,%d:-1' % (T_PU, T_HEAD, T_LAS, T_LAS2, T_LAS3)})
fire = sid(s, 437); land = sid(s, 264)
check('pushup_lasers_106_113_121', fire[:3] == [106, 113, 121] and fire[3:6] == [208, 215, 223], str(fire))
check('pushup_lasers_land_111_118_126', land[:3] == [111, 118, 126], str(land))
dirs = changes(of(obs, T_PU), 'dir')
check('pushup_flips_87_189_291', [f for f, v in dirs[1:4]] == [87, 189, 291], str(dirs))
lp = {f: y for f, t, x, y, d in of(obs, T_LAS) if abs(x - 5548) < 2}
check('pushup_laser_7_5px', [lp.get(f) for f in range(107, 112)] == [2133, 2140, 2148, 2155, 2163], str([lp.get(f) for f in range(107, 112)]))
w = sorted((f, x) for f, t, x, y, d in of(obs, T_LAS2) if 113 <= f <= 116 and x > 5548)
check('pushup_wave_5px', [x for f, x in w] == [5553, 5558, 5563, 5568], str(w))
dm = sorted(set(x for f, t, x, y, d in of(obs, T_LAS3) if f == 115))
check('pushup_diamonds_every_3', dm == [5533, 5548, 5563], str(dm))
hy = sorted(set(y for f, t, x, y, d in of(obs, T_HEAD)))
check('pushup_head_on_action_point_2117_2143', 2117 in hy and 2143 in hy, str(hy[:6]))
# the laser hurts 10 (sound 17); the head beaten (BOSSHPT) flies, shown, and lands like a Ganseki (sound 50); the body's debris
s, obs, rows = run(park(5548, 2160, 2, 140, inv=False), 140, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0'})
check('pushup_laser_hurts_10', sid(s, 17) and hp(rows, 5) - hp(rows, 139) >= 2, '%s hp %s -> %s' % (sid(s, 17), hp(rows, 5), hp(rows, 139)))
for c in CH:
    s, obs, _ = run(park(5450, 2150, 2, 200) + '40 BOSSHPT %d 0\n' % T_HEAD, 200,
                    {'FP_LEVEL': 'tb1', 'FP_CHAR': str(c), 'FP_TRACK': '%d:5549,%d:-1' % (T_HEAD, T_PUDEB)})
    deb = set(f for f, t, x, y, d in obs if t == T_PUDEB)
    head = [(f, d['hd']) for f, t, x, y, d in obs if t == T_HEAD]
    check('c%d_pushup_beaten_head_flies_lands_50' % c, sid(s, 23) and sid(s, 50) and deb and head and head[-1][1] == '0',
          'def %s land %s debris %s head %s' % (sid(s, 23), sid(s, 50), len(deb), head[-1:]))

# ================================================================ lava squid (tb2 4442, 3032; RV squid2: shots 125 / 154, D1 155, turn 166-181, dash 212)
s, obs, _ = run(park(7000, 2990, 2, 60) + park(4250, 2990, 60, 330), 330, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0', 'FP_TRACK': '%d:4442,%d:-1' % (T_SQ, T_SQLAS)})
sq = of(obs, T_SQ); D = changes(sq, 'a0')
sh = sid(s, 73)
check('squid_shots_29_apart', len(sh) >= 2 and sh[1] - sh[0] == 29, str(sh))
d1 = first(D, 1); d3 = first(D, 3); d1b = first(D, 1, d3 or 0); d2 = first(D, 2)
check('squid_turn_after_first_aim', d1 and d3 and d1b and d2 and len(sh) > 1 and d1 - sh[1] == 1 and d3 - d1 == 11 and d1b - d3 == 15 and d2 - d1b == 31,
      '%s %s %s %s %s' % (sh, d1, d3, d1b, d2))
G = [float(d['a3']) for f, t, x, y, d in sq if d2 and f >= d2]
xs = {f: x for f, t, x, y, d in sq}
check('squid_dash_left_decelerating', d2 and G and G[0] == 1 and xs.get(d2 + 1) == 4434 and xs.get(d2 + 2) == 4426 and min(xs.values()) < 4300,
      '%s %s %s %s' % (G[:1], xs.get((d2 or 0) + 1), xs.get((d2 or 0) + 2), min(xs.values())))
lz = sorted((f, x) for f, t, x, y, d in of(obs, T_SQLAS))
check('squid_laser_6px_left', len(lz) > 3 and lz[1][1] - lz[0][1] == -6, str(lz[:3]))
s, obs, rows = run(park(4300, 3025, 2, 160, inv=False), 160, {'FP_LEVEL': 'tb2', 'FP_CHAR': '2'})
check('squid_laser_hurts_10', sid(s, 73) and hp(rows, 5) - hp(rows, 159) >= 2, '%s hp %s -> %s' % (sid(s, 73), hp(rows, 5), hp(rows, 159)))
for c in CH:
    s, obs, _ = run(park(4250, 2990, 2, 200) + '40 BOSSHPT %d 0\n' % T_SQ, 200, {'FP_LEVEL': 'tb2', 'FP_CHAR': str(c), 'FP_TRACK': '%d:-1' % T_DEB})
    deb = sorted(set(d['fr'] for f, t, x, y, d in obs if f < 50))
    check('c%d_squid_beaten_two_halves_crash' % c, deb == ['0', '1'] and sid(s, 23) and sid(s, 29), 'frames %s def %s crash %s' % (deb, sid(s, 23), sid(s, 29)))

# ================================================================ tank bot (tb4 2999, 2305; RV tank4: attack 72, pellets 88 / 106 (sound 28), G 90)
s, obs, _ = run(park(7000, 2280, 2, 60) + park(2780, 2280, 60, 330), 330, {'FP_LEVEL': 'tb4', 'FP_CHAR': '0', 'FP_TRACK': '%d:2999' % T_TANK})
tk = of(obs, T_TANK); D = changes(tk, 'a0')
pel = sid(s, 28); a2 = first(D, 2)
walk = [tk[i + 1][2] - tk[i][2] for i in range(len(tk) - 1) if 64 <= tk[i][0] < 70]
check('tank_walks_2px', walk == [-2] * 6, str(walk))
check('tank_attack_72_pellets_88_106', near(a2, 72, 1) and len(pel) >= 2 and near(pel[0] - a2, 16, 3) and near(pel[1] - pel[0], 18, 1), '%s %s' % (a2, pel))   # (INFERRED: 3 frames later)
g = [float(d['a3']) for f, t, x, y, d in tk]
check('tank_cooldown_90', g and max(g) >= 89, str(max(g or [0])))
d1 = first(D, 1); d0 = first(D, 0, d1 or 0)
check('tank_limiter_turn', d1 and d0 and 30 <= d0 - d1 <= 31, '%s %s' % (d1, d0))
for c in CH:   # Lilac / Carol attack it from behind; Milla throws her cube at it
    if c == 4: sc = park(2840, 2280, 2, 420) + '3 5 R\n' + ''.join('%d %d X\n%d %d X\n' % (b, b + 60, b + 70, b + 72) for b in range(10, 400, 100))
    else: sc = park(3030, 2290, 2, 420) + ''.join('%d %d X\n' % (f, f + 3) for f in range(20, 420, 12))
    s, obs, _ = run(sc, 420, {'FP_LEVEL': 'tb4', 'FP_CHAR': str(c), 'FP_TRACK': '%d:2999,%d:-1' % (T_TANK, T_TDEB)})
    tracks = [o for o in obs if o[1] == T_TDEB]
    check('c%d_tank_defeated_tracks_crash' % c, sid(s, 23) and sid(s, 311) and tracks and sid(s, 29),
          'hits %s def %s 311 %s tracks %s crash %s' % (len(sid(s, 24)), sid(s, 23), sid(s, 311), len(tracks), sid(s, 29)))

# ================================================================ saucer (tb4 944, 2704; RV saucer4: the turn at x 1122, fireballs 259 / 273 / 287 / 301)
s, obs, _ = run(park(7000, 2680, 2, 60) + park(600, 2680, 60, 430), 430,
                {'FP_LEVEL': 'tb4', 'FP_CHAR': '0', 'FP_TRACK': '%d:944,%d:944,%d:912,%d:944,%d:976,%d:-1' % (T_SAU, T_LEG1, T_LEG1 + 1, T_LEG1 + 2, T_LEG1 + 3, T_FBALL)})
sa = {f: (x, y, d) for f, t, x, y, d in of(obs, T_SAU)}
turn = next((f for f in sorted(sa) if float(sa[f][2]['a4']) == -1), None)
check('saucer_turns_at_1122', turn and abs(sa[turn][0] - 1122) <= 6, '%s %s' % (turn, sa.get(turn, (None,))[0]))
G = [float(sa[f][2]['a3']) for f in sorted(sa) if f > 60]
check('saucer_speed_0_1_to_3', max(G) == 3 and abs(G[2] - G[1] - 0.1) < 1e-6, str(G[:4]))
ys = sorted(set(y for x, y, d in sa.values()))
check('saucer_bob_10px', ys[0] >= 2694 and ys[-1] <= 2704 and ys[-1] - ys[0] >= 9, str(ys[:3] + ys[-2:]))
fb = sid(s, 46)
check('saucer_fireballs_every_14', len(fb) == 4 and gaps(fb) == [14, 14, 14], str(fb))
legs = {t: {f: (x, y) for f, tt, x, y, d in obs if tt == t} for t in range(T_LEG1, T_LEG1 + 4)}
ok, info = True, ''
for f in (141, 200, 260):
    if f not in sa: ok = False; continue
    X, Y, d = sa[f]; D_ = float(d['a0'])
    exp = [X - 32 * math.sin(math.radians(D_)), X + 32 * math.sin(math.radians(D_ - 90)), X + 32 * math.sin(math.radians(D_)), X + 32 * math.sin(math.radians(D_ + 90))]
    got = [legs[T_LEG1 + k].get(f, (None, None))[0] for k in range(4)]
    if any(g_ is None or abs(g_ - e) > 1.01 for g_, e in zip(got, exp)): ok = False; info += '%d %s %s ' % (f, got, [round(e) for e in exp])
check('saucer_legs_32_sin_cos', ok, info)
up = sorted((f, y) for f, t, x, y, d in of(obs, T_FBALL))
check('saucer_fireball_flies_up_5px', len(up) > 3 and up[1][1] - up[0][1] == -5, str(up[:3]))
for c in CH:
    s, obs, _ = run(park(600, 2680, 2, 200) + '40 BOSSHPT %d 0\n' % T_SAU, 200, {'FP_LEVEL': 'tb4', 'FP_CHAR': str(c), 'FP_TRACK': '%d:-1' % T_DEB})
    deb = sorted(set(d['fr'] for f, t, x, y, d in obs if f < 50))
    n = len(set((x, y) for f, t, x, y, d in obs if f == 44))
    check('c%d_saucer_beaten_dome_and_legs' % c, n >= 5 and deb == ['0', '1', '2'] and sid(s, 23), 'pieces %s frames %s def %s' % (n, deb, sid(s, 23)))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
