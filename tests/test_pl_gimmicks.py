#!/usr/bin/env python3
"""Host tests for the Pangu Lagoon gimmicks (session 23; src/game/pl_gim.c, build/re/spec_pl_gimmicks.md), Lilac only:
the importer rows (types 2144-2166, BreakablePLRock = FN breakable wall member 1, the PL spinball remap), the vanishing blocks (RE
13032-13039 cycle, RV vb5 sound cadence), the time gates and the super boost (col_9_50, RE spec_boost 2), the cyclone tops (RV ct1),
the bows (RV bow2) and the arrow breaking the PL rocks, the breakout ball (RV bo4 path), the giant swords (13130-13134), the spinball
and chains, the lagoon platforms (path registry), the hanging switch + stone doors (RV sd3) handing over to pl_flow.c's door scene
(PL3 -> PL4, PL6 -> the arena), and the PL1 critical path through the cyclone top and the time gate.
usage: test_pl_gimmicks.py [data_dir]   (default: converted2; the session's PL data: cv)"""
import subprocess, sys, os, tempfile, struct, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace('\\n', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD',
                                                     'FP_DBG_PLDOOR', 'FP_DBG_PLG', 'FP_INVULN')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_CHAR': '0'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=900)
    os.unlink(name)
    rows, ev, obs, flow = [], [], [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            d = {}
            for i in range(0, len(t) - 1, 2):
                try: d[t[i]] = float(t[i + 1])
                except ValueError: pass
            rows.append(d)
        elif t[0] == 'sid':
            ev.append((len(rows), int(t[1])))
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2])}
            for i in range(4, len(t) - 1, 2):
                try: d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
                except ValueError: pass
            obs.append(d)
        elif t[0] == 'plflow' or t[0].startswith('load['):
            flow.append((len(rows), line))
    return rows, ev, obs, flow

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sids(ev, n): return [i for i, s in ev if s == n]
def MY(r): return r['y'] - 14.0                        # the original's MV.Y (the port's y = MV.Y + ORIG_REF_DY)
def track(obs, t, f=None): return [o for o in obs if o['type'] == t and (f is None or o['f'] == f)]

def lvobjs(lv):
    """(type, anim, x, y) of a level file's objects (FPL2 header: magic, W, H, nrec, nobj, ...)"""
    d = None
    p = os.path.join(DATA, lv)
    if not os.path.isdir(p): return []
    for fn in os.listdir(p):
        b = open(os.path.join(p, fn), 'rb').read()
        if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

# ---- data: the importer's PL_GIM rows (types 2144-2166, member | layer << 8 in the anim field; pl_inventory.md counts)
OBJ = {lv: lvobjs(lv) for lv in ('pl1', 'pl2', 'pl3', 'pl4', 'pl5', 'pl6')}
def n(lv, t, member=None):                              # placed on the map (the off-map templates are removed by the spawn)
    return len([o for o in OBJ[lv] if o[0] == t and o[2] >= 0 and o[3] >= 0 and (member is None or (o[1] & 0xff) == member)])
def members(lv, t): return sorted(o[1] & 0xff for o in OBJ[lv] if o[0] == t and o[2] >= 0 and o[3] >= 0)
check('data_vanishing_blocks', n('pl4', 2144) == 82 and 389 <= n('pl5', 2144) <= 392, str((n('pl4', 2144), n('pl5', 2144))))
check('data_time_gates', [n(lv, 2145) for lv in OBJ] == [4, 4, 2, 3, 7, 9] and n('pl1', 2146) == 40, str([n(lv, 2145) for lv in OBJ]))
check('data_bows', [n(lv, 2148) for lv in OBJ] == [0, 4, 1, 1, 1, 0] and n('pl2', 2147) == 40, str([n(lv, 2148) for lv in OBJ]))
check('data_cyclone_tops', n('pl1', 2151) == 7 and n('pl2', 2151) == 5, str((n('pl1', 2151), n('pl2', 2151))))
check('data_breakout', n('pl4', 2153) == 5 and n('pl4', 2154) == 5 and n('pl4', 2152, 0) >= 1 and n('pl4', 2152, 1) >= 1,
      str((n('pl4', 2153), n('pl4', 2154), n('pl4', 2152, 0), n('pl4', 2152, 1))))
check('data_plrock_is_fn_wall_member1', (n('pl2', 577, 1), n('pl3', 577, 1), n('pl2', 577, 0)) == (4, 6, 0), str((n('pl2', 577, 1), n('pl3', 577, 1))))
check('data_lagoon_platforms', (n('pl1', 2156), n('pl2', 2156), n('pl2', 2157)) == (4, 2, 1), str((n('pl1', 2156), n('pl2', 2157))))
check('data_swords', (n('pl5', 2158), n('pl6', 2158), n('pl5', 2159)) == (6, 2, 6), str((n('pl5', 2158), n('pl6', 2158), n('pl5', 2159))))
check('data_spinball_remap', (n('pl4', 2160), n('pl5', 2160), n('pl4', 272), members('pl4', 2161)) == (1, 2, 0, [0, 1, 2, 3]),
      str((n('pl4', 2160), n('pl4', 272))))
check('data_doors', members('pl3', 2163) == [0, 1] and members('pl6', 2163) == [0, 1, 2, 3]
      and n('pl3', 2162) == 1 and n('pl6', 2162) == 1, '')
check('data_active_shelves', n('pl1', 2164) == 5, str(n('pl1', 2164)))

# ---- vanishing blocks (RE 13032-13039: A = (6 k + t) mod 210, solid 60 <= A < ~163, drawn while B < 14): the first PL5 block (k 0)
rows, ev, obs, _ = run('', 640, {'FP_LEVEL': 'pl5', 'FP_TRACK': '2144:192'})
b = sorted((o['f'], o) for o in track(obs, 2144) if o.get('y') == 768)
solid = [f for f, o in b if not (o['fl'] & 1)]
shown = [f for f, o in b if o['hd'] == 0]
cyc = [f for f in solid if f < 420 and f >= 210]
check('vblock_solid_cycle', 100 <= len(cyc) <= 106 and solid and abs(solid[0] % 210 - 60) <= 2 and all((f - solid[0]) % 210 < 106 for f in solid),
      'solid frames in a cycle %d, first %s' % (len(cyc), solid[:1]))
check('vblock_drawn_while_solid', shown and set(shown) <= set(range(min(solid) - 1, 641)) and abs(len([f for f in shown if 210 <= f < 420]) - len(cyc)) <= 3,
      '%d drawn / %d solid' % (len([f for f in shown if 210 <= f < 420]), len(cyc)))
s8, s9 = sids(ev, 328), sids(ev, 329)
check('vblock_sound_cadence', len(s8) > 10 and len(s9) > 10 and len({f % 6 for f in s8 + s9}) == 1, 'RV vb5: every 6 frames; %s' % sorted({f % 6 for f in s8 + s9}))
# standing on a block (the first two PL5 blocks, A0 0 / 6, over a pit): solid until A ~ 163, then she drops through (flag 0)
rows, ev, obs, _ = run('3 SETV others E 99999\n80 TP 208 740', 220, {'FP_LEVEL': 'pl5'})
check('vblock_player_stands_then_falls', rows and all(r['gnd'] == 1 and r['y'] == 768 for r in rows[90:155]) and MY(rows[200]) > 900,
      'y %s / %s' % (rows and rows[120]['y'], rows and rows[200]['y']))

# ---- time gate (col_9_50: super boost, ENERGY 100, sound 324) and the super boost up PL1's wall (critical path)
rows, ev, obs, _ = run('3 SETV others E 99999\n5 TP 8700 1120\n10 400 R\n24 44 J\n95 100 B', 400, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2146:-1'})
g = sids(ev, 324)
check('tgate_sound_energy', g and rows[g[0]]['en'] >= 99.9, str(g[:1]) + ' en %s' % (g and rows[g[0]]['en']))
check('tgate_super_boost', sids(ev, 322) and not sids(ev, 53) and g and sids(ev, 322)[0] > g[0], '322 %s 53 %s' % (sids(ev, 322)[:1], sids(ev, 53)[:1]))
check('tgate_pieces_orbit', g and any(o['an'] != track(obs, 2146, 0)[0]['an'] for o in track(obs, 2146, g[0] + 3)), 'activated pieces anim 7')
check('pl1_wall_after_gate', rows and min(MY(r) for r in rows[100:250]) < 500 and max(r['x'] for r in rows[200:]) > 9300,
      'min y %.0f' % min(MY(r) for r in rows[100:250]) if rows else '')

# ---- cyclone top (RV ct1: TP (4040, 1330) at 60, J at 72: ride at f89 with 7 + 174, rises 1 px a frame, released at H 251 = f340)
rows, ev, obs, _ = run('3 SETV others E 99999\n60 TP 4040 1330\n72 72 J', 560, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2151:4040'})
s7 = sids(ev, 7)
check('ctop_ride_starts', s7 and 85 <= s7[0] <= 92 and sids(ev, 174) and abs(sids(ev, 174)[0] - s7[0]) <= 1, str(s7[:1]))
check('ctop_rises', rows and all(abs(rows[f]['y'] - rows[f + 1]['y'] - 1) < 1e-6 for f in range(100, 330)), '1 px a frame (RV ct1)')
rel = [f for f in range(300, 360) if rows[f]['vy'] > -1 + 1e-6 and rows[f - 1]['vy'] <= -1 + 1e-6] if rows else []
check('ctop_release_h251', rel and s7 and 248 <= rel[0] - s7[0] <= 256, 'released %s (RV f341, ride f89)' % rel[:1])
top = [o for o in track(obs, 2151) if o['f'] in (150, 450, 555)]
check('ctop_goes_home', len(top) == 3 and top[0]['y'] < 1440 and top[1]['y'] > 1440 and top[1]['a0'] == 2 and top[2]['y'] == 1440 and top[2]['a0'] == 0, str([(o['f'], o['y']) for o in top]))
# PL1 critical path: the top at (5152, 1632) is the only way up the cliff at x 5335; jump off near the top
rows, ev, obs, _ = run('3 SETV others E 99999\n5 TP 5000 1760\n10 32 R\n32 57 J\n200 220 J\n200 400 R', 400, {'FP_LEVEL': 'pl1'})
check('pl1_cliff_by_top', rows and sids(ev, 7) and rows[-1]['x'] > 5800 and MY(rows[-1]) < 1400, 'x %.0f y %.0f' % (rows[-1]['x'], MY(rows[-1])) if rows else '')

# ---- bow (RV bow2: TP (3440, 700) at 60: grab f84 (33), release f108 (325), stuck f161 (326) at (4185, 903), Lilac at x 4485)
rows, ev, obs, _ = run('3 SETV others E 99999\n60 TP 3440 700', 320, {'FP_LEVEL': 'pl2', 'FP_TRACK': '2148:3440'})
a = sorted((o['f'], o) for o in track(obs, 2148))
check('bow_grab_release_stick', sids(ev, 33) and abs(sids(ev, 33)[0] - 84) <= 3 and sids(ev, 325) and abs(sids(ev, 325)[0] - 108) <= 4
      and sids(ev, 326) and abs(sids(ev, 326)[0] - 161) <= 5, str((sids(ev, 33)[:1], sids(ev, 325)[:1], sids(ev, 326)[:1])))
check('bow_arrow_stuck_at', a and abs(a[-1][1]['x'] - 4185) <= 10 and abs(a[-1][1]['y'] - 903) <= 10, str(a and (a[-1][1]['x'], a[-1][1]['y'])))
check('bow_lilac_lands', rows and abs(rows[-1]['x'] - 4485) <= 12 and rows[-1]['gnd'] == 1, 'x %.1f' % (rows[-1]['x'] if rows else 0))
# the PL2 arrow at (11360, 880) breaks the four BreakablePLRocks in its path (fn_gima_wall_hit; sound 13, pieces)
rows, ev, obs, _ = run('3 SETV others E 99999\n60 TP 11360 850', 200, {'FP_LEVEL': 'pl2', 'FP_TRACK': '577:-1'})
check('bow_arrow_breaks_rocks', len(track(obs, 577, 50)) == 4 and len(track(obs, 577, 199)) == 0 and len(sids(ev, 13)) == 4,
      '%d -> %d, %d x 13' % (len(track(obs, 577, 50)), len(track(obs, 577, 199)), len(sids(ev, 13))))

# ---- breakout (RV bo4: TP (2145, 890) at 60; ball (2144, 880) f100 (2066, 886), f250 (2230, 918), turns at x 1998 / 2290)
rows, ev, obs, _ = run('3 SETV others E 99999\n60 TP 2145 890', 651, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2153:2144,2154:-1'})
bb = {o['f']: o for o in track(obs, 2153)}
RV = {100: (2066, 886), 250: (2230, 918), 400: (2050, 878)}
check('breakout_ball_path', all(f in bb and abs(bb[f]['x'] - x) <= 8 and abs(bb[f]['y'] - y) <= 40 for f, (x, y) in RV.items()),
      str([(f, bb.get(f, {}).get('x'), bb.get(f, {}).get('y')) for f in RV]))
xs = [bb[f]['x'] for f in sorted(bb)]
check('breakout_ball_walls', xs and abs(min(xs) - 1998) <= 4 and abs(max(xs) - 2290) <= 4, '%s..%s' % (min(xs) if xs else '', max(xs) if xs else ''))
pd = [o for o in track(obs, 2154) if o['f'] == 300]
check('breakout_paddle_follows', any(abs(o['x'] - bb[300]['x']) <= 64 and o['x'] != o.get('x0', -1) for o in pd) and 300 in bb, '')

# ---- giant sword (RE 13130-13134: trigger 150, falls after F > 20 with damage 15, lands with 51)
rows, ev, obs, _ = run('60 TP 8176 260\n62 300 L', 200, {'FP_LEVEL': 'pl5', 'FP_TRACK': '2158:8176'})
sw = {o['f']: o for o in track(obs, 2158)}
check('sword_trigger', sids(ev, 150) and abs(sids(ev, 150)[0] - 60) <= 2, str(sids(ev, 150)[:1]))
fall = [f for f in sorted(sw) if sw[f]['y'] > 240]
check('sword_falls_after_20', fall and 18 <= fall[0] - sids(ev, 150)[0] <= 24 and sw[fall[0]]['bd'] == 15, str(fall[:1]))
check('sword_lands', sids(ev, 51) and sw[199]['y'] > 300 and sw[199]['bd'] == 0 and sw[199]['y'] == sw[150]['y'], 'y %s' % sw.get(199, {}).get('y'))

# ---- spinball (PL4 (1080, 880): radius 120, D + 2 a frame; chains at 24 / 48 / 72 / 96 on the same angle)
rows, ev, obs, _ = run('', 62, {'FP_LEVEL': 'pl4', 'FP_TRACK': '2160:1080,2161:-1'})
ok = True
for f in (30, 60):
    ball = track(obs, 2160, f)
    ch = track(obs, 2161, f)
    if not ball or len(ch) != 4: ok = False; break
    d = math.hypot(ball[0]['x'] - 1080, ball[0]['y'] - 880)
    rs = sorted(round(math.hypot(c['x'] - 1080, c['y'] - 880)) for c in ch)
    ok = ok and abs(d - 120) <= 1.5 and all(abs(r - e) <= 1.5 for r, e in zip(rs, (24, 48, 72, 96)))
b30, b60 = track(obs, 2160, 30), track(obs, 2160, 60)
check('spinball_orbit', ok and b30 and b60 and abs((b60[0]['a3'] - b30[0]['a3']) % 360 - 60) < 1e-6, '')

# ---- lagoon platform (path registry F 2, G 50: a 100-px swing)
rows, ev, obs, _ = run('', 400, {'FP_LEVEL': 'pl1', 'FP_TRACK': '2156:6104'})
ys = [o['y'] for o in track(obs, 2156)]
check('lagoon_platform_swing', ys and 96 <= max(ys) - min(ys) <= 102, '%s' % (ys and max(ys) - min(ys)))

# ---- hanging switch + stone door (RV sd3: TP (6025, 64) at 60: grab f61 (33), 62 at f62, 132 at f125, the switch 64 px down) and
# the door scene (pl_flow.c): Down + Jump to let go, she lands between the doors -> walk, fade, PL4
rows, ev, obs, flow = run('3 SETV others E 99999\n60 TP 6025 64\n150 175 D\n150 151 J', 420, {'FP_LEVEL': 'pl3', 'FP_TRACK': '2163:5976,301:-1'})
d1 = {o['f']: o for o in track(obs, 2163)}
sw = {o['f']: o for o in track(obs, 301)}
check('door_switch_sounds', sids(ev, 33) and sids(ev, 62) and sids(ev, 62)[0] - sids(ev, 33)[0] in (0, 1) and sids(ev, 132)
      and abs(sids(ev, 132)[0] - sids(ev, 62)[0] - 63) <= 4, 'RV 61 / 62 / 125: %s' % ((sids(ev, 33)[:1], sids(ev, 62)[:1], sids(ev, 132)[:1]),))
check('door_opens', 140 in d1 and d1[140]['a0'] == 0 and 60 in d1 and d1[60]['a0'] > 0.99, '')
check('door_switch_descends', 140 in sw and sw[140]['y'] - sw[0]['y'] == 64, str(sw.get(140, {}).get('y')))
st = [i for i, l in flow if 'door start' in l]
check('door_scene_pl3_to_pl4', st and any('load[pl4]' in l for _, l in flow), str([l[:40] for _, l in flow]))
check('door_closes_behind', st and any(f > st[0] + 60 and d1[f]['a0'] >= 0.99 for f in d1), '')
rows, ev, obs, flow = run('3 SETV others E 99999\n60 TP 4481 224\n150 175 D\n150 151 J', 520, {'FP_LEVEL': 'pl6'})
check('door_scene_pl6_arena', any('arena' in l for _, l in flow) and rows and abs(rows[-1]['x'] - 6080) < 2, str([l[:40] for _, l in flow]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
