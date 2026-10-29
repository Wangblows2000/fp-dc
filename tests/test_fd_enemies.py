#!/usr/bin/env python3
"""Host tests for the Final Dreadnought enemies (session 24; src/game/fd_enemy.c + the reused bg_enemy.c pods, jc_enemy.c Elites /
lasers, tb_enemy.c tank bots / homing bullets, relic.c hoppers / fire spitters, sb_enemy.c HopperMkII; build/re/spec_fd_enemies.md).
RE = the decompiled events (15162-15164 / foreach_spawner_65 + 15246-15249 pod payloads, 15287 pod rain, 15255-15271 pounder,
16053-16088 / 16202 / 16181-16183 gatling guns, 14761-14780 beast iris, 16690-16698 spider beamer). No original traces (FD needs a
save that reaches it): the checks follow the decompiled rules. Lilac (0), Carol (2) and Milla (4):
 - fd1a / fd1b pods: the enemies placed under a pod are gone at the start; the pod lands and releases them (tank bot code 1 at y - 38
   with flag 16, hopper 2 at y - 31, fire spitter 3 at y - 15, HopperMkII 4 at y - 31) on the landing frame; a pod without one an
   Elite 29 frames later; FD1a's pod rain (drop points within 199 px, g3 % 10 == 5, sound 345) drops pods that release Elites;
 - pounder (fd1a): walks, slams down when the player is below it (sound 132, two shockwaves running apart), rises back to its home y,
   and each character beats it (sound 38, keystone, gone);
 - gatling gun (fd3b): comes out when the player is near (sound 254), aims and fires lasers along its barrels (sound 376) in bursts
   (120 of 200 frames), each character beats it (H 10 -> 11, sound 38); fd3a's guns stay dormant until the alarm (16183, sound 310);
 - beast iris (fd4a): runs at the player, bites (B 10 on its frames 14-16), each character beats it;
 - spider beamer (fd4a): hidden until the player is level with it in the view, comes out, fires a homing bullet when its clock passes 60
   (sound 171), each character beats it (sound 29, the wreck stays);
 - every Final Dreadnought enemy object has its sprite row in its act.
usage: test_fd_enemies.py [data_dir] [--only name,...]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
DATA = ARGS[0] if ARGS else os.path.join(HERE, '..', 'converted2')
ONLY = sys.argv[sys.argv.index('--only') + 1].split(',') if '--only' in sys.argv else None
CHARS = (0, 2, 4)
NAME = {0: 'lilac', 2: 'carol', 4: 'milla'}

def run(script, frames, env):
    """sids: (frame, id); obs: (frame, fields); rows: player rows by frame"""
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
            d = dict(zip(t[4::2], t[5::2])); d['type'] = int(t[2]); d['key'] = int(t[3]); obs.append((int(t[1]), d))
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def want(name): return ONLY is None or name in ONLY
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def F(d, k): return float(d[k])
def near(d, x, y, r=8): return abs(F(d, 'x') - x) <= r and abs(F(d, 'y') - y) <= r
INV = '3 SETV others E 99999;'
def attacks(c, a, b):
    """attack inputs from frame a to b: Lilac / Carol tap X every 9 frames, Milla holds X 60 frames (her cube) then taps it"""
    if c == 4: return ''.join('%d %d X;%d %d X;' % (f, f + 60, f + 64, f + 66) for f in range(a, b, 70))
    return ''.join('%d %d X;' % (f, f + 3) for f in range(a, b, 9))
def hold(x, y, a, b, step=2): return ''.join('%d TP %d %d;' % (f, x, y) for f in range(a, b, step))

# ---- fd1a pods with payloads (RE 15162-15164 + foreach_spawner_65, 15246-15248, 14657-14660)
if want('pods'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 2250 360;250 TP 520 330;450 TP 5100 870', 600,
                           {'FP_LEVEL': 'fd1a', 'FP_CHAR': str(c), 'FP_TRACK': '2337:-1,1828:-1,234:-1,235:-1'})
        tank = [(f, d) for f, d in obs if d['type'] == 1828 and near(d, 2399, 345, 40)]
        check('%s_pod_tank_hidden_at_start' % n, not any(f < 60 for f, d in tank), str([f for f, d in tank][:3]))
        land = [f for f in sid(s, 346) if 60 < f < 250]
        t0 = min((f for f, d in tank), default=None)
        check('%s_pod_tank_on_landing' % n, land and t0 is not None and 0 <= t0 - land[0] <= 1 and
              any(f == t0 and abs(F(d, 'y') - 345) < 1 and int(d['fl'], 16) & 0x10000 for f, d in tank), '%s %s' % (land[:2], t0))
        hop = [(f, d) for f, d in obs if d['type'] == 234 and abs(F(d, 'x') - 607) < 60 and abs(F(d, 'y') - 320) < 40]
        check('%s_pod_hopper' % n, hop and min(f for f, d in hop) > 250 and any(abs(F(d, 'y') - 320) < 1 for f, d in hop[:1]),
              str([(f, d['x'], d['y']) for f, d in hop[:2]]))
        fs = [(f, d) for f, d in obs if d['type'] == 235 and abs(F(d, 'x') - 5183) < 80 and abs(F(d, 'y') - 880) < 30]
        check('%s_pod_firespitter' % n, fs and min(f for f, d in fs) > 450, str([(f, d['x'], d['y']) for f, d in fs[:2]]))
    # an empty pod: an Elite 29 frames after the landing (14660 / 15245: B > 30)
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 7180 1380', 200, {'FP_LEVEL': 'fd1a', 'FP_CHAR': str(c), 'FP_TRACK': '1327:-1'})
        land = [f for f in sid(s, 346) if f > 60]
        el = [f for f, d in obs if d['type'] == 1327 and abs(F(d, 'x') - 7263) < 4]
        check('%s_pod_elite_29_frames_later' % n, land and el and min(el) - (land[0] - 1) == 29, '%s %s' % (land[:1], el[:1]))
    s, obs, rows = run(INV + '60 TP 3800 490', 200, {'FP_LEVEL': 'fd1b', 'FP_CHAR': '0', 'FP_TRACK': '1024:-1'})
    hm = [(f, d) for f, d in obs if d['type'] == 1024 and abs(F(d, 'x') - 3871) < 40 and abs(F(d, 'y') - 480) < 20]
    check('fd1b_pod_hoppermkii', hm and min(f for f, d in hm) > 60, str([(f, d['x'], d['y']) for f, d in hm[:2]]))

# ---- FD1a pod rain (RE 15287): from the drop points near the player, every 10 frames on g3 % 10 == 5, once the jumbotron script
# reached A 3 (X > 11111, Y > 1760: 15277) and B 160 (15284: C 1; session 26c, before: from X > 800)
if want('rain'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 11300 1850;240 TP 11800 1340;' + hold(11800, 1340, 242, 440, 4), 440,
                           {'FP_LEVEL': 'fd1a', 'FP_CHAR': str(c), 'FP_TRACK': '2337:-1,1327:-1'})
        r345 = [f for f in sid(s, 345) if f > 240]
        pods = {(int(F(d, 'x')), int(F(d, 'a0'))) for f, d in obs if d['type'] == 2337 and f > 100 and int(F(d, 'x')) in
                (11615, 11743, 11871, 11999, 12127, 12255, 12383, 12511, 11615)}
        els = [f for f, d in obs if d['type'] == 1327 and 11400 < F(d, 'x') < 12700]
        check('%s_pod_rain' % n, len(r345) >= 3 and all(b - a == 10 for a, b in zip(r345, r345[1:4])) and len(pods) >= 3 and els,
              '%s pods %d elites %s' % (r345[:4], len(pods), els[:1]))

# ---- pounder (fd1a 776 1104; RE 15255-15271, foreach_pound_65)
if want('pounder'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 740 1150;61 63 R;' + attacks(c, 120, 600), 600,
                           {'FP_LEVEL': 'fd1a', 'FP_CHAR': str(c), 'FP_TRACK': '2624:776,367:-1'})
        po = [(f, d) for f, d in obs if d['type'] == 2624]
        slam = next((f for f, d in po if F(d, 'a0') == 1), None)
        check('%s_pounder_slams_on_the_player' % n, slam is not None and slam > 55 and slam in [f for f, d in po if int(d['dir']) == 24], str(slam))
        hitg = [f for f in sid(s, 132) if f > 60]
        sh = [(f, d) for f, d in obs if d['type'] == 367 and hitg and f == hitg[0] + 5]
        check('%s_pounder_shockwaves' % n, hitg and len(sh) == 2 and {int(d['dir']) for f, d in sh} == {0, 16}, '%s %d' % (hitg[:1], len(sh)))
        last = max((f for f, d in po), default=0)
        check('%s_pounder_beaten' % n, last < 599 and any(f > last for f in sid(s, 38)), '%s %s' % (last, sid(s, 38)[:2]))
    s, obs, rows = run(INV + '60 TP 790 1150', 260, {'FP_LEVEL': 'fd1a', 'FP_CHAR': '0', 'FP_TRACK': '2624:776'})
    po = [(f, d) for f, d in obs if d['type'] == 2624]
    back = next((f for f, d in po if f > 100 and F(d, 'a0') == 0), None)
    check('pounder_rises_home_d0_f_m30', back and any(f == back and F(d, 'y') == 1104 and F(d, 'a2') == -30 for f, d in po), str(back))

# ---- gatling guns (fd3b 6192 704: in a floor, k 3 -> D 630 = 270 (RE 16202); RE 16053-16088)
if want('gatling'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 6100 660;' + hold(6140, 576, 230, 700, 2) + '232 234 R;' + attacks(c, 240, 700), 700,
                           {'FP_LEVEL': 'fd3b', 'FP_CHAR': str(c), 'FP_TRACK': '2628:6192,1347:-1'})
        g = [(f, d) for f, d in obs if d['type'] == 2628]
        h1 = next((f for f, d in g if F(d, 'a0') == 1), None)
        r254 = [f for f in sid(s, 254) if f > 60]
        check('%s_gatling_comes_out' % n, h1 and r254 and r254[0] - h1 == 31, '%s %s' % (h1, r254[:1]))
        h5 = next((f for f, d in g if F(d, 'a0') == 5), None)
        ys = [F(d, 'y') for f, d in g if h5 and f == h5]
        check('%s_gatling_rises_128_turns' % n, h5 and r254 and h5 - r254[0] == 37 and ys == [576.0], '%s %s' % (h5, ys))
        las = [(f, d) for f, d in obs if d['type'] == 1347 and int(d['an']) >= 0]
        r376 = [f for f in sid(s, 376) if h5 and f < h5 + 60]
        check('%s_gatling_fires' % n, las and len(r376) >= 8, '%d lasers %s' % (len(las), r376[:4]))
        h10 = next((f for f, d in g if F(d, 'a0') == 10), None)
        h11 = next((f for f, d in g if F(d, 'a0') == 11), None)
        check('%s_gatling_beaten' % n, h10 and h11 and h11 - h10 == 60 and any(abs(f - h11) <= 1 for f in sid(s, 38)), '%s %s' % (h10, h11))
    # fd3a: dormant until the alarm (16181: past x 5000 and below y 800; 16183: B 540 -> sound 310)
    s, obs, rows = run(INV + '60 TP 5250 1450;' + hold(5300, 1450, 62, 760, 4), 760,
                       {'FP_LEVEL': 'fd3a', 'FP_CHAR': '0', 'FP_TRACK': '2628:5368'})
    g = [(f, d) for f, d in obs if d['type'] == 2628]
    w = [f for f in sid(s, 310)]
    early = [f for f, d in g if 60 < f < (w[0] if w else 760) and F(d, 'a0') > 0]
    late = [f for f, d in g if w and f > w[0] and F(d, 'a0') > 0]
    check('fd3a_gatling_dormant_until_alarm', w and 595 <= w[0] <= 610 and not early and late, '%s %s %s' % (w[:1], early[:1], late[:1]))

# ---- beast iris (fd4a 135 5981; RE 14761-14780)
if want('iris'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + '60 TP 300 5960;' + attacks(c, 70, 500), 500,
                           {'FP_LEVEL': 'fd4a', 'FP_CHAR': str(c), 'FP_TRACK': '2631:135'})
        ir = [(f, d) for f, d in obs if d['type'] == 2631]
        runs = [F(d, 'a1') for f, d in ir if 60 < f < 70]
        check('%s_iris_runs_at_the_player' % n, runs and all(abs(v) == 4 for v in runs), str(runs[:3]))
        gone = max((f for f, d in ir), default=0)
        beaten = next((f for f, d in ir if F(d, 'a3') == 6), None)
        check('%s_iris_beaten' % n, beaten is not None and gone < 499, '%s %s' % (beaten, gone))
    s, obs, rows = run(INV + '60 TP 300 5960', 200, {'FP_LEVEL': 'fd4a', 'FP_CHAR': '0', 'FP_TRACK': '2631:135'})
    ir = [(f, d) for f, d in obs if d['type'] == 2631]
    bite = [(int(d['fr']), int(d['bd']), int(d['fl'], 16) & 0x20) for f, d in ir if F(d, 'a3') == 2]
    check('iris_bite_b10_frames_14_16', bite and all((b == 10) == (14 <= fr <= 16) for fr, b, a in bite if b or fr in (13, 17)) and
          any(b == 10 and a for fr, b, a in bite), str([x for x in bite if x[1]][:3]))

# ---- spider beamer (fd4a 2319 4224, facing right: left of the laser tunnel; RE 16690-16698)
if want('spider'):
    for c in CHARS:
        n = NAME[c]
        s, obs, rows = run(INV + hold(2350, 4230, 60, 420) + '61 63 L;' + attacks(c, 140, 420), 420,
                           {'FP_LEVEL': 'fd4a', 'FP_CHAR': str(c), 'FP_TRACK': '2632:2319,1856:-1'})
        sp = [(f, d) for f, d in obs if d['type'] == 2632 and abs(F(d, 'y') - 4224) < 1]
        pre = [d for f, d in sp if f < 59]
        on = next((f for f, d in sp if F(d, 'a0') == 1), None)
        check('%s_spider_hidden_then_out' % n, pre and all(d['hd'] == '1' and F(d, 'a0') == 0 for d in pre) and on is not None and
              any(f == on and d['hd'] == '0' and int(d['fl'], 16) & 0x20 and int(d['dir']) == 0 for f, d in sp), str(on))
        r171 = sid(s, 171)
        bul = [f for f, d in obs if d['type'] == 1856 and int(d['an']) >= 0]
        check('%s_spider_fires_at_e_61' % n, on and r171 and r171[0] - on == 60 and bul and min(bul) == on + 60,
              '%s %s %s' % (on, r171[:1], bul[:1]))
        dead = next((f for f, d in sp if F(d, 'a0') == 6), None)
        check('%s_spider_beaten' % n, dead and dead in sid(s, 29) and any(f > dead + 5 for f, d in sp), str(dead))

# ---- sprite rows: every FD enemy object of each act has an animation
if want('rows'):
    T = '2624:-1,2625:-1,2626:-1,2627:-1,2628:-1,2629:-1,2630:-1,2631:-1,2632:-1,2337:-1,1326:-1,1327:-1,1828:-1,1824:-1,1039:-1'
    for lv in ('fd1a', 'fd1b', 'fd2a', 'fd2b', 'fd3a', 'fd3b', 'fd4a', 'fd4b'):
        s, obs, rows = run('', 3, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': T})
        miss = sorted({d['type'] for f, d in obs if f == 2 and int(d['an']) < 0 and d['type'] != 1326 and F(d, 'x') >= 0})
        check('%s_enemy_rows' % lv, not miss, str(miss))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
