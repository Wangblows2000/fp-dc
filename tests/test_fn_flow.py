#!/usr/bin/env python3
"""Host tests for the Fortune Night stage flow (session 17; src/game/levels.c rows fn1-fn6, src/game/fn_flow.c, game.c,
results.c; build/re/spec_fn_flow.md): the Classic route RM5 -> FN1, every act's start (card / walk-in) and its exit into the
next act (FN1 / FN2 at X > W - 50, FN3 by the Serpentine pass, FN4 / FN5 at X > W + 20), checkpoints and restarts, the FN6
death into FN5 with g4 2, music per act, card ids, the Time Attack entry (selectable with the stage's progress), the FN3
class sets per character, the FN6 walk-in / fixed camera, the stage sound group and the parallax data.
usage: test_fn_flow.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'music', 'voice') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def at(rows, f): return next((r for r in rows if r['f'] == f), None)

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- FN1: fresh start with the card at (77, 533) (4307 + 2955: start + 16; RV X 77), m_stage3, cards 31 / 32 (RE counter 0x1f)
rows, ev, out = run('', 40, {'FP_LEVEL': 'fn1', 'FP_TRACK': '41:-1'})
check('fn1_start', rows[0]['x'] == 77 and rows[-1]['g4'] == 1, 'x %s g4 %s' % (rows[0]['x'], rows[-1]['g4']))
check('fn1_music_stage3', [l for _, l in ev if l.startswith('music')][:1] == ['music bgm/m_stage3.adp @0'])
cards = {(float(l.split()[5]), float(l.split()[7])): int(l.split()[13]) for l in out.splitlines() if l.startswith('ob 1 41 ')}
check('fn1_card_ids', cards == {(3058.0, 493.0): 31, (10506.0, 1093.0): 32}, str(cards))
for lv, want in (('fn2', {(9714.0, 1357.0): 33, (5474.0, 1389.0): 34, (2304.0, 981.0): 35}), ('fn3', {(6274.0, 933.0): 36}),
                 ('fn4', {(7127.0, 1125.0): 37, (3970.0, 717.0): 38}), ('fn5', {(2818.0, 1053.0): 39, (4730.0, 1181.0): 40})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_TRACK': '41:-1'})
    c = {(float(l.split()[5]), float(l.split()[7])): int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c == want, str(c))

# ---- FN1 -> FN2 (5045: X > W - 50, B > 0; ~53-frame fade; 3578 g4 -1) -> walk-in from x -32 at B 3 for 32 frames, g4 1;
# the checkpoint passed on the way (7366: X > 7700, g4 2) does not leak into the next act; m_stage3 continues
rows, ev, out = run('5 TP 11500 1100\n10 300 R\n', 140, {'FP_LEVEL': 'fn1'})
f2 = [r for r in rows if r['act'] == 2][1:2]; f2 = f2[0] if f2 else None
check('fn1_checkpoint_g4_2', any(r['g4'] == 2 and r['act'] == 1 for r in rows))
check('fn1_exit_to_fn2_walkin', f2 is not None and f2['x'] < -20 and f2['g4'] == -1 and abs(f2['gsp'] - 3) < 1e-6, str(f2 and (f2['x'], f2['g4'])))
w = [r for r in rows if r['act'] == 2]
k = next((i for i, r in enumerate(w) if r['g4'] == 1), None)
check('fn2_walkin_32_frames_then_g4_1', k is not None and 31 <= k <= 33 and abs(w[k]['x'] - 61) < 4, str(k and (k, w[k]['x'])))
check('fn2_music_continues_stage3', [l for _, l in ev if l.startswith('music b')][-1].startswith('music bgm/m_stage3.adp'))

# ---- FN3: the Serpentine pass (7710-7725; RV FN3 with the player placed at (15060, 190)): NPCSerpentine at x - 500, the
# crash (17 + 134), the first dash (61) braking (6) to x 14910, the taunt for ~100 frames, the second dash passes the player,
# the fade and FN4 ~282 frames after the trigger (RV 283); the player walks at B 2 to X 15117 (CAM.E 15133 - 16); the timer
# and the pause are off from 7710; FN4 plays m_stage3b (8326) and walks in
for ch, nm in (('0', 'lilac'), ('3', 'carolbike'), ('4', 'milla')):
    rows, ev, out = run('5 TP 15060 190\n20 30 S\n60 70 L\n', 320, {'FP_LEVEL': 'fn3', 'FP_CHAR': ch, 'FP_TRACK': '900:-1'})
    ob = [(int(l.split()[1]), float(l.split()[5])) for l in out.splitlines() if l.startswith('ob ') and l.split()[2] == '900']
    sids = [(f, int(l.split()[1])) for f, l in ev if l.startswith('sid ')]
    xs = [x for f, x in ob]
    first = [f for f, s in sids if s == 134]
    check('%s_fn3_serpentine_appears' % nm, ob and ob[0][1] == 15060 - 500 and first and 4 <= first[0] <= 8, str(ob[:1]) + str(first[:1]))
    check('%s_fn3_crash_dash_brake_sounds' % nm, [s for f, s in sids if s in (17, 134, 61, 6)][:5] == [17, 134, 61, 6, 61], str([s for f, s in sids if s in (17, 134, 61, 6)][:6]))
    check('%s_fn3_first_dash_stops_14910' % nm, 14910 in xs and xs.count(14910) > 80, 'stopped %d frames' % xs.count(14910))
    f4 = next((r for r in rows if r['act'] == 4), None)
    check('%s_fn3_exit_to_fn4' % nm, f4 is not None and 280 <= f4['f'] <= 292 and f4['g4'] == -1, str(f4 and f4['f']))
    pre = [r for r in rows if r['act'] == 3 and 60 <= r['f'] <= 230]
    check('%s_fn3_exit_walk_to_edge' % nm, pre and all(r['x'] == 15117 for r in pre[30:]) and all(r['mode'] == 1 for r in pre), str(sorted({r['x'] for r in pre})[:4]))
    tm = [r['tm'] for r in rows if r['act'] == 3 and r['f'] >= 6]
    check('%s_fn3_timer_stops' % nm, tm and len(set(tm)) == 1, str(sorted(set(tm))[:3]))
    check('%s_fn3_no_pause' % nm, not any(s == 63 for f, s in sids), str([s for f, s in sids if s == 63]))
    check('%s_fn4_music_stage3b' % nm, any(l.startswith('music bgm/m_stage3b.adp') for _, l in ev))
    if ch == '3': check('carolbike_fn3_off_the_bike', rows[2]['chr'] == 3 and at(rows, 40)['chr'] == 2, '%s %s' % (rows[2]['chr'], at(rows, 40)['chr']))

# ---- FN3 class sets by character (8082 MV.A = 0: MonitorBombs go; 8083 MV.A != 0: the limit markers move around the samurai)
for ch, nm in (('0', 'lilac'), ('2', 'carol'), ('4', 'milla')):
    _, _, out = run('', 1, {'FP_LEVEL': 'fn3', 'FP_CHAR': ch, 'FP_OBJLIST': '1'})
    objs = [tuple(int(v) for v in l.split()[1:4]) for l in out.splitlines() if l.startswith('obj ')]
    bombs = sum(1 for t, x, y in objs if t == 179)
    mk = {t: (x, y) for t, x, y in objs if 368 <= t <= 371}
    if ch == '0': check('lilac_fn3_bombs_removed_markers_kept', bombs == 0 and mk.get(368) == (9872, 160) and mk.get(371) == (10504, 391), '%d %s' % (bombs, mk))
    else: check('%s_fn3_bombs_kept_markers_moved' % nm, bombs == 8 and mk.get(368, (0,))[0] == 10831 and mk.get(369, (0,))[0] == 11441 and mk.get(371, (0, 0))[1] == 1424, '%d %s' % (bombs, mk))

# ---- FN4 -> FN5 (8295: X > W + 20 = 11644, B > 0) -> FN5's walk-in; FN5 -> FN6 (X > 15220): FN6 walks in clamped to X 400
# without a B event (8550) inside its fixed camera box (8553: view (214, 71), RV); m_boss
rows, ev, out = run('5 TP 11600 1040\n10 300 R\n', 140, {'FP_LEVEL': 'fn4'})
last4 = max((r['x'] for r in rows if r['act'] == 4 and r['f'] > 10), default=0)
f5 = next((r for r in rows if r['act'] == 5), None)
check('fn4_exit_past_w_plus_20', f5 is not None and last4 > 11644 and f5['x'] < -20 and f5['g4'] == -1, 'max x %s' % last4)
# (the Panther chase, fn_panther.c 8372, takes the control first at X > limR - 300 - 20 B: the exit comes after its timeline)
rows, ev, out = run('5 TP 15100 200\n10 400 R\n', 400, {'FP_LEVEL': 'fn5'})
w6 = [r for r in rows if r['act'] == 6]
check('fn5_exit_to_fn6', bool(w6) and max(r['x'] for r in rows if r['act'] == 5) > 15220, str(w6[:1] and w6[0]['x']))
# (no B event from the flow; the Panther's floor auto-run then holds CTRL.D during the walk-in, 8516 / 8517, RV T-G1)
check('fn6_walkin_x400_no_b', w6 and w6[0]['x'] == 400 and w6[0]['g4'] == -1 and w6[0]['vx'] == 0, str(w6[:1] and (w6[0]['x'], w6[0]['gsp'])))
check('fn6_fixed_camera_view', w6 and all(r['cx'] == 214 and r['cy'] == 71 for r in w6), str(sorted({(r['cx'], r['cy']) for r in w6})[:3]))
check('fn6_music_boss', [l for _, l in ev if l.startswith('music b')][-1].startswith('music bgm/m_boss.adp'))

# ---- checkpoints and restarts: FN5 8421 (X 10560..11560, g4 < 3 -> 3, (10880, 560)); a death restarts there with one life
# less; an FN6 death (loop_death_34) goes to FN5 with g4 2 -> (g5, g6) = (10880, 560) -> 8421 takes g4 back to 3
rows, ev, out = run('5 TP 10900 540\n40 HP 0\n40 HURT 10\n', 260, {'FP_LEVEL': 'fn5'})
check('fn5_checkpoint_g4_3', at(rows, 30)['g4'] == 3)
rs = [r for r in rows if r['f'] > 60 and r['lv'] == 4]
check('fn5_restart_at_checkpoint', rs and abs(rs[0]['x'] - 10880) < 2 and rs[0]['g4'] in (2, 3), str(rs[:1] and (rs[0]['x'], rs[0]['g4'])))
# (FN6 has the Panther's scrolling floor now: the death is forced, HP 0 + a hit, once the act runs)
rows, ev, out = run('5 TP 10900 540\n30 TP 15100 200\n35 400 R\n380 HP 0\n380 HURT 10\n', 650, {'FP_LEVEL': 'fn5'})
back = [r for r in rows if r['act'] == 5 and r['lv'] == 4]
check('fn6_death_to_fn5_g4_2', any(r['act'] == 6 for r in rows) and back and abs(back[0]['x'] - 10880) < 8 and back[0]['g4'] in (2, 3) and loads(ev)[-1] == 'fn5',
      str(back[:1] and (back[0]['x'], back[0]['g4'])) + str(loads(ev)))
check('fn6_death_g4_back_to_3', back and back[-1]['g4'] == 3)
# a debug start in FN6 (no FN5 checkpoint): the death target starts fresh (INFERRED)
rows, ev, out = run('100 HP 0\n100 HURT 10\n', 400, {'FP_LEVEL': 'fn6'})
back = [r for r in rows if r['act'] == 5]
check('fn6_death_without_checkpoint_fresh_fn5', back and back[0]['x'] == 61 and back[0]['g4'] == 1, str(back[:1] and (back[0]['x'], back[0]['g4'])))

# ---- FN2 / FN3 / FN4 restarts at their checkpoints (FN2 inline 6520 / 490, FN3 7818 9536 / 360, FN4 inline 7670 / 840)
for lv, tp, cp in (('fn2', '5 TP 6600 470\n', 6520), ('fn3', '5 TP 9900 340\n', 9536), ('fn4', '5 TP 7700 820\n', 7670)):
    rows, ev, out = run(tp + '40 HP 0\n40 HURT 10\n', 260, {'FP_LEVEL': lv})
    rs = [r for r in rows if r['f'] > 60 and r['lv'] == 4]
    check('%s_checkpoint_restart' % lv, at(rows, 30)['g4'] == 2 and rs and abs(rs[0]['x'] - cp) < 2, str(rs[:1] and rs[0]['x']))

# ---- FN6 results (the boss module calls results_begin_boss at 8785; host: RESBOSS): m_clear, the group voice for everyone
# (8545), the Fortune Night achievements at D = 149 (stage 3: speedrunner 46 under 480000 ms), the wipe (8547 sound 56) and
# frame 8 -> the front end (Sky Battalion, frame 35, is not ported). TP keeps the player in the arena (no floor without the boss)
for ch, nm in (('0', 'lilac'), ('4', 'milla')):
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('60 RESBOSS\n' + ''.join('%d TP 500 150\n' % f for f in range(10, 1000, 30)), 900,
                            {'FP_LEVEL': 'fn6', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin')})
    lines = out.splitlines()
    check('%s_fn6_results_voice_clear' % nm, 'voice teamlilac_madeit' in lines and any(l.startswith('music bgm/m_clear.adp') for l in lines))
    check('%s_fn6_results_ach_stage3' % nm, any(l.startswith('ach results stage 3') for l in lines) and 'ach earn 46' in lines)
    st = [l for l in loads(ev) if l not in ('frontend', 'gal')]
    check('%s_fn6_wipe_to_front_end' % nm, 'sid 56' in lines and rows[-1]['mode'] == 0 and st[-1:] == ['fn6'], str(st))

# ---- the Classic route: RM5's results -> frame 8 -> FN1 (MenuSequence.I = 2), with the card (g4 1)
MT = '30 TP 2690 500\n30 SETV others E 99999\n40 520 R\n' + ''.join('%d BOSSHP 0\n' % f for f in (1000, 1400, 1800, 2200, 2600)) + '4900 4901 J\n'
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(MT, 5100, {'FP_LEVEL': 'rm5', 'FP_CHAR': '0', 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': os.path.join(d, 's.bin')})
st = [l for l in loads(ev) if l not in ('frontend', 'gal')]
check('route_rm5_to_fn1', st[-2:] == ['rm5', 'fn1'] and rows[-1]['mode'] == 1 and rows[-1]['act'] == 1, str(st))
fn1 = [r for r in rows if r['act'] == 1 and r['mode'] == 1]
check('route_fn1_fresh_start', fn1 and fn1[-1]['g4'] == 1 and abs(fn1[-1]['x'] - 77) < 40, str(fn1[-1:] and (fn1[-1]['x'], fn1[-1]['g4'])))

# ---- Time Attack: Fortune Night (s 3, list index 3) is listed by the level table and locked until its progress (RE 641-643);
# with the title cheat (every stage's progress) it starts FN1 in Time Attack
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
TA = '200 202 S\n360 361 D\n380 381 J\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n700 701 J\n820 821 J\n'
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(CHEAT + TA, 1100, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_fn1_with_progress', 'fn1' in loads(ev) and rows[-1]['mode'] == 1 and rows[-1]['act'] == 1, str(loads(ev)))
    rows, ev, out = run(TA, 1100, {'FP_SAVE': os.path.join(d, 'empty.bin')})
    check('ta_fn_locked_without_progress', 'fn1' not in loads(ev), str(loads(ev)))

# ---- data: the stage sound group (flow ids 61 / 134, converted), the FN parallax (night sky / mall sets, the 0.8 layer)
check('sfx_fn_converted', all(os.path.exists(os.path.join(DATA, 'sfx', 's%d.wav' % i)) for i in (61, 134)))
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_ = struct.unpack_from('<2H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += 6 + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh))
    return out
p1, p2, p4, p6 = par('fn1'), par('fn2'), par('fn4'), par('fn6')
check('par_fn1_night_set', [h for h, *_ in p1] == [1629, 1640, 1630, 1633, 1632, 1634, 1635, 1725, 1631] and (1630, .9, .9, 0, 96, 160, 120) in p1 and (1725, .4, .505, -4, 656, 640, 32) in p1, str(p1))
check('par_fn2_piece5_1802', 1802 in [h for h, *_ in p2] and (1725, .4, .505, -4, 688, 640, 32) in p2)
check('par_fn4_mall_set', [(h, D, E) for h, D, E, *_ in p4] == [(1897, .99, .94), (1898, .72, .72), (1899, .6, .7)], str(p4))
check('par_fn6_drift', [(h, G) for h, D, E, G, *_ in p6] == [(1629, -.1), (1640, -.35)], str(p6))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
