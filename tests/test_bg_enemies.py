#!/usr/bin/env python3
"""Host tests for the Battle Glacier enemies (session 24; src/game/bg_enemy.c, the reused jc_enemy.c Elite / tb_enemy.c saucers and
squids / tb_gim.c explosive blocks; build/re/spec_bg_enemies.md). RE = the decompiled events (14276-14306 drill sat, 14656-14662 /
14724 elite spawner, 10949-11038 + 14408 Elite); RV = original traces build/orig_traces/bg_enemies (session1.sh, the same inputs).
Lilac (0), Carol (2) and Milla (4):
 - drill sat (bg3): buried and unhittable (C 5), wakes when the player is within 159 px (D -1), rumbles (sound 35 every 4th frame),
   bursts out 31 frames later (sound 306, anim 11), rises 100 px in 44 frames, hovers; each character beats it (sound 74), it spins,
   flies (sound 195) into the wall of explosive blocks (sound 38: a block goes), explodes (sound 29) and comes back buried 60 frames later;
 - elite spawner (bg5): waits 300 px up, drops 20 px a frame when the player comes near (sound 345), lands (sound 346, the shock
   wave rises 4 px a frame for 15 frames), opens and releases an Elite 29 frames after landing (the spawner goes); the Elite alerts;
 - BG4's placed Elites are the enemies module's Elite (jc_enemy.c types 1326 / 1327, drawn) and each character defeats one (two
   knock-downs, sound 29); an idle Elite inside the view alerts (14408) where the JC rule (player ahead within 219 px) would not;
 - the Thermal Base saucers / lava squids have their sprites in BG.
usage: test_bg_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (0, 2, 4)
NAME = {0: 'lilac', 2: 'carol', 4: 'milla'}

def run(script, frames, env):
    """sids: (frame, id) (a sound belongs to the frame whose events played it); obs: (frame, type, fields); rows: player rows"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_ENERGY')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=900)
    os.unlink(name)
    fr, sids, obs, rows = 0, [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fr = int(t[1]); rows[fr] = dict(zip(t[2::2], t[3::2]))
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))
        elif t[0] == 'ob':
            d = dict(zip(t[4::2], t[5::2])); d['type'] = int(t[2]); obs.append((int(t[1]), d))
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def F(d, k): return float(d[k])
def first(obs, cond):
    return next((f for f, d in obs if cond(d)), None)
INV = '3 SETV others E 99999;'
def attacks(c, a, b):
    """the character's attack inputs from frame a to b: Lilac / Carol tap X every 9 frames, Milla holds X 60 frames (her cube) then
    taps it (the thrown cube)"""
    if c == 4: return ''.join('%d %d X;%d %d X;' % (f, f + 60, f + 64, f + 66) for f in range(a, b, 70))
    return ''.join('%d %d X;' % (f, f + 3) for f in range(a, b, 9))

# ---- drill sat (bg3 5520 264, the wall of explosive blocks at 5904-6032 x 120-376; RE 14276-14306)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '60 TP 5470 300;61 63 R;' + attacks(c, 100, 900), 900,
                       {'FP_LEVEL': 'bg3', 'FP_CHAR': str(c), 'FP_TRACK': '2336:5520,1913:-1', 'FP_ENERGY': '100'})
    dr = [(f, d) for f, d in obs if d['type'] == 2336]
    bl = [(f, d) for f, d in obs if d['type'] == 1913 and 5800 < F(d, 'x') < 6100]
    D = lambda f: next((F(d, 'a0') for g, d in dr if g == f), None)
    pre = [d for f, d in dr if f < 60]
    rum = [d for f, d in dr if 61 <= f < 90]
    check('%s_drill_buried_hidden' % n, pre and all(F(d, 'a0') == 0 and d['hd'] == '1' for d in pre) and rum and all(int(d['c']) > 0 and d['hd'] == '1' for d in rum),
          str([(d['a0'], d['hd'], d['c']) for d in rum[-1:]]))
    wake = first(dr, lambda d: F(d, 'a0') == -1)
    check('%s_drill_wakes_on_the_player' % n, wake == 60, str(wake))
    r35 = [f for f in sid(s, 35) if wake and wake <= f < wake + 31]
    check('%s_drill_rumble_35_every_4' % n, len(r35) >= 6 and all(b - a == 4 for a, b in zip(r35, r35[1:])), str(r35[:8]))
    out = first(dr, lambda d: F(d, 'a0') == 1)
    check('%s_drill_bursts_out_31_frames_later' % n, out and wake and out - wake == 31 and out in sid(s, 306), '%s %s' % (wake, out))
    rise = first(dr, lambda d: F(d, 'a0') == 2); hover = first(dr, lambda d: F(d, 'a0') == 3)
    ys = [F(d, 'y') for f, d in dr if hover and hover <= f < hover + 120 and F(d, 'a0') == 3]
    check('%s_drill_rises_100_in_44' % n, rise and hover and hover - rise == 44 and ys and min(ys) == 164 and max(ys) <= 264 and max(ys) > 200,
          '%s %s %s %s' % (rise, hover, ys and min(ys), ys and max(ys)))
    beat = first(dr, lambda d: F(d, 'a0') == 6)
    check('%s_drill_beaten' % n, beat is not None and beat in sid(s, 74), '%s %s' % (beat, sid(s, 74)[:3]))
    fly = first(dr, lambda d: F(d, 'a0') == 7)
    flyd = next((d for f, d in dr if f == fly), None)
    check('%s_drill_spins_then_flies' % n, fly and beat and 20 <= fly - beat <= 30 and fly in sid(s, 195) and flyd and F(flyd, 'ang') in (90, 270),
          '%s %s %s' % (beat, fly, flyd and flyd['ang']))
    gone = sorted(set(f for f, d in bl if d['hd'] == '1'))
    b38 = [f for f in sid(s, 38) if fly and f > fly]
    check('%s_flying_drill_blasts_a_block' % n, gone and b38 and gone[0] - 1 <= b38[0] <= gone[0], '%s %s' % (gone[:1], b38[:2]))
    home = first(dr, lambda d: F(d, 'a0') == -2)
    back = next((f for f, d in dr if home and f > home and F(d, 'a0') == 0), None)
    check('%s_drill_explodes_and_comes_back' % n, home and home in sid(s, 29) and back and back - home in (60, 61),
          '%s %s' % (home, back))
    hd = next((d for f, d in dr if f == home), None)
    check('%s_drill_back_home' % n, hd and F(hd, 'x') == 5520 and F(hd, 'y') == 264 and hd['hd'] == '1' and F(hd, 'A') == 5, str(hd and (hd['x'], hd['y'], hd['A'])))
    # the wall of explosive blocks (5904-6032) holds the player until two drill runs open its 248 row (RV drill3_wall: one block a run)
    s, obs, rows = run(INV + '60 TP 5470 300;61 63 R;' + attacks(c, 100, 640) + '700 950 R', 960, {'FP_LEVEL': 'bg3', 'FP_CHAR': str(c), 'FP_ENERGY': '100'})
    s0, obs0, rows0 = run(INV + '60 TP 5470 300;700 950 R', 960, {'FP_LEVEL': 'bg3', 'FP_CHAR': str(c)})
    check('%s_drills_open_the_wall' % n, float(rows0[950]['x']) < 5900 and float(rows[950]['x']) > 6200, '%s %s' % (rows0[950]['x'], rows[950]['x']))

# contact damage (14287 / 14288; RV drill3_idle): B 5 while the player is above a risen drill, else 0
s, obs, rows = run(INV + '60 TP 7180 1300', 230, {'FP_LEVEL': 'bg3', 'FP_CHAR': '0', 'FP_TRACK': '2336:7272'})
bd = {f: int(d['bd']) for f, d in obs}
want = {50: 0, 100: 5, 120: 0, 199: 5, 220: 5}       # RV drill3_idle rows (- 1): B 5 at 100 / 200 / 220, 0 at 120 (the drill above her)
check('drill_contact_damage', all(bd.get(f) == v for f, v in want.items()), str({f: bd.get(f) for f in want}))

# ---- elite spawner (bg5 8639 1887 + its transporter; RE 14656-14662, 14724)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '60 TP 8480 1860', 220, {'FP_LEVEL': 'bg5', 'FP_CHAR': str(c), 'FP_TRACK': '2337:8639,2370:-1,1327:-1,1326:-1'})
    sp = [(f, d) for f, d in obs if d['type'] == 2337]
    y0 = next((F(d, 'y') for f, d in sp if f == 10), None)
    check('%s_spawner_waits_300_up' % n, y0 == 1587 and all(d['hd'] == '1' for f, d in sp if f < 60), str(y0))
    trig = first(sp, lambda d: F(d, 'a1') >= 1)
    ys = [F(d, 'y') for f, d in sp if trig and trig <= f < trig + 14]
    check('%s_spawner_drops_20' % n, trig == 60 and trig in sid(s, 345) and ys and all(b - a == 20 for a, b in zip(ys, ys[1:])),
          '%s %s' % (trig, ys[:3]))
    land = first(sp, lambda d: F(d, 'a1') >= 2)
    check('%s_spawner_lands' % n, land and trig and land - trig == 14 and land in sid(s, 346), '%s %s' % (trig, land))
    wv = [(f, d) for f, d in obs if d['type'] == 2370]
    wy = [F(d, 'y') for f, d in wv]
    check('%s_spawner_wave' % n, wv and wv[0][0] == land and len(wv) == 16 and wy[-1] == wy[0] - 60, '%s %s' % (len(wv), wy[:1] + wy[-1:]))
    last = max((f for f, d in sp), default=None)
    m = [(f, d) for f, d in obs if d['type'] == 1327]
    born = m[0][0] if m else None
    check('%s_spawner_releases_an_elite' % n, land and last and born and last - land == 27 and born == last + 1
          and abs(F(m[0][1], 'x') - 8639) <= 1 and F(m[0][1], 'y') == 1887, '%s %s %s' % (land, last, born))
    q = first(m, lambda d: F(d, 'a3') == 1)
    sprite = [d for f, d in obs if d['type'] == 1326]
    check('%s_spawned_elite_alerts_and_shows' % n, q is not None and q - born <= 2 and sprite and sprite[-1]['an'] != '-1' and sprite[-1]['hd'] == '0',
          '%s %s' % (born, q))

# ---- BG4's placed Elites: the enemies module's types, each character defeats one (11839 1447; JC5 10949-11038)
for c in CHARS:
    n = NAME[c]
    s, obs, rows = run(INV + '60 TP 11760 1440;61 63 R;' + attacks(c, 70, 1300), 1300,
                       {'FP_LEVEL': 'bg4', 'FP_CHAR': str(c), 'FP_TRACK': '1327:11836,1326:11839,1498:-1,1499:-1', 'FP_ENERGY': '100'})
    j5 = [d for f, d in obs if d['type'] in (1498, 1499)]
    m = [(f, d) for f, d in obs if d['type'] == 1327]; e = [(f, d) for f, d in obs if d['type'] == 1326]
    check('%s_bg4_elite_types' % n, not j5 and m and e and e[0][1]['an'] != '-1', '%d %d %d' % (len(j5), len(m), len(e)))
    knocks = sorted(set(f for f, d in m if F(d, 'a0') == -2 and not any(g == f - 1 and F(dd, 'a0') == -2 for g, dd in m)))
    end = max((f for f, d in m), default=0)
    check('%s_bg4_elite_defeated' % n, len(knocks) >= 2 and end < 1299 and any(abs(f - end) <= 2 for f in sid(s, 29)),
          '%s end %s' % (knocks[:3], end))

# 14408: an idle Elite inside the view alerts though the player is not ahead of it within 219 px
s, obs, rows = run(INV + '60 TP 11600 1440', 120, {'FP_LEVEL': 'bg4', 'FP_CHAR': '0', 'FP_TRACK': '1327:11836'})
q = first([(f, d) for f, d in obs if f >= 60], lambda d: F(d, 'a3') == 1)
dx = [abs(F(d, 'x') - float(rows[f]['x'])) for f, d in obs if f == q] if q else []
check('bg4_elite_alerts_in_view', q is not None and q <= 62 and dx and dx[0] > 219, '%s %s' % (q, dx))

# ---- the Thermal Base saucers / lava squids have their rows in BG (bg1 saucer 7992 280, bg3 squid 922 728)
for lv, ty, x, tp in (('bg1', 1829, 7992, '60 TP 7900 300'), ('bg3', 1827, 922, '60 TP 820 740'), ('bg2', 1829, 2872, '60 TP 2800 900')):
    s, obs, rows = run(INV + tp, 160, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': '%d:%d' % (ty, x)})
    an = set(d['an'] for f, d in obs if f > 60)
    check('%s_tb_%d_drawn' % (lv, ty), an and '-1' not in an and all(d['hd'] == '0' for f, d in obs if f > 60), str(sorted(an)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
