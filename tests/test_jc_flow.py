#!/usr/bin/env python3
"""Host tests for the Jade Creek stage flow (session 21; src/game/levels.c rows jc1-jc5, src/game/jc_flow.c, parallax.c,
tools/import_frame.py JC_FLOW blocks; build/re/spec_jc_flow.md): JC1's fresh start (start point x -27 + 16, the card, m_stage5,
ML.B 1200), the act chain JC1 -> JC2 -> JC3 -> JC4 -> JC5 by the right edge (4135 -> 3578, walk-ins with g4 -1), JC3's edge
closed by the Sign camera bound until its boss is beaten (jc_flow_act3_cleared: g4 10, m_stage5b, camera free), JC5 without
an edge exit, the checkpoints (10483 / 10655 / 10848 / 11192), the music slots, cards and water lines per act, the Classic
route SB1 results -> JC1, the loop pieces of the JC2 / JC3 shaft (layer sets Q10 / Q11, JCLoop1's flag 0), the parallax and
the water surface band (10368-10373), for Lilac, Carol and Milla.
usage: test_jc_flow.py [data_dir]   (default: converted2; the session-21 worktrees build Jade Creek into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (('lilac', '0'), ('carol', '2'), ('milla', '4'))

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD_DBG')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'music', 'voice', 'jcflow') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def jcf(ev, what): return [i for i, l in ev if l.startswith('jcflow ' + what)]

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- JC1 fresh start (4307: g4 0 -> 1, the card): the start point (-27, 213) + 16 (2956: X -11, MV.Y 229 = feet 243), m_stage5
# (loop_play_music_5_39), the stage sounds, the card images (name frame 5, STAGE 4); the player drops onto the ledge
for nm, ch in CHARS:
    rows, ev, out = run('', 120, {'FP_LEVEL': 'jc1', 'FP_CHAR': ch, 'FP_CARD_DBG': '1'})
    check('jc1_fresh_start_%s' % nm, rows[0]['x'] == -11 and rows[0]['y'] == 243 and rows[-1]['g4'] == 1 and rows[-1]['act'] == 1,
          'x %s y %s g4 %s' % (rows[0]['x'], rows[0]['y'], rows[-1]['g4']))
    check('jc1_lands_on_the_ledge_%s' % nm, rows[-1]['gnd'] == 1 and 600 < rows[-1]['y'] < 650, 'y %s gnd %s' % (rows[-1]['y'], rows[-1]['gnd']))
    check('jc1_music_stage5_%s' % nm, music(ev)[:1] == ['bgm/m_stage5.adp'], str(music(ev)))
card = [l for l in out.splitlines() if l.startswith('card jc1')]
check('jc1_card_images', card and 'stage 0x0' not in card[0] and 'name 0x0' not in card[0], str(card))
check('jc1_stage_sounds', 'stage sounds 12' in out)   # SFX_JC1 (session 21 merge: the modules' ids)

# ---- the act chain by the right edge (4135: X > W - 50, B > 0; 3578: g4 -1, frame + 1) and the walk-ins from x -32 at the
# start point + 16 (4490 / 4526 / 10482: B 3 for 32 frames, then g4 1); JC3 after its boss (JCACT3 = jc_flow_act3_cleared)
CHAIN = (('jc1', 13300, 700, '', 'jc2', 2, 253), ('jc2', 14660, 400, '', 'jc3', 3, 1573), ('jc3', 14800, 600, '4 JCACT3\n', 'jc4', 4, 1573),
         ('jc4', 10100, 1200, '', 'jc5', 5, 1429))
for lv, x, y, extra, nx, act, sy in CHAIN:
    for nm, ch in CHARS:
        rows, ev, out = run(extra + '5 TP %d %d\n6 400 R\n' % (x, y), 400, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        go = [r for r in rows if r['act'] == act]
        check('%s_exit_to_%s_%s' % (lv, nx, nm), nx in loads(ev) and go and go[0]['g4'] == -1 and go[0]['x'] == -32 and go[0]['y'] == sy + 16 + 14,
              str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['y'], go[0]['g4'])))
        g1 = [r for r in go if r['g4'] == 1]
        check('%s_walkin_ends_%s' % (nx, nm), g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36, str(g1[:1] and g1[0]['f'] - go[0]['f']))

# ---- JC3: before the boss the Sign camera bound (E = 9624 + 213) keeps the player from the right part; the boss's end
# (10772 / 10773) gives the checkpoint g4 10 at (9128, 1550), m_stage5b and the free camera (C = 0, E = W + 50)
rows, ev, out = run('5 TP 9500 1450\n6 300 R\n', 300, {'FP_LEVEL': 'jc3', 'FP_CHAR': '0'})
check('jc3_sign_bound_before_boss', max(r['x'] for r in rows) <= 9837 - 16 + 1 and rows[-1]['act'] == 3, 'max x %.1f' % max(r['x'] for r in rows))
rows, ev, out = run('5 TP 9500 1450\n20 JCACT3\n21 300 R\n', 300, {'FP_LEVEL': 'jc3', 'FP_CHAR': '0'})
check('jc3_boss_end_checkpoint_and_music', rows[-1]['g4'] == 10 and jcf(ev, 'act3 cleared') and music(ev)[-1:] == ['bgm/m_stage5b.adp'],
      'g4 %s %s' % (rows[-1]['g4'], music(ev)))
check('jc3_boss_end_frees_the_camera', max(r['x'] for r in rows) > 9900, 'max x %.1f' % max(r['x'] for r in rows))

# ---- JC5: no right-edge exit (no 4135 / 3578 in handle_frame_44: the boss's results end the stage)
rows, ev, out = run('5 TP 13700 1400\n6 300 R\n', 300, {'FP_LEVEL': 'jc5', 'FP_CHAR': '0'})
check('jc5_no_edge_exit', all(r['act'] == 5 for r in rows) and loads(ev).count('jc5') == 1, str(loads(ev)))

# ---- checkpoints: JC2 10483 (7050 < X < 7250, MV.Y < 820 -> 2, (7080, 790)), JC3 10655 (5300 < X < 5700, MV.Y > 800 -> (5555,
# 1530)), JC4 10848 (4100 < X < 4400 -> (4184, 1640)); a death restarts there (2957: X = g5, MV.Y = g6)
for lv, tp, ok in (('jc2', '5 TP 7100 760\n', True), ('jc2', '5 TP 7400 760\n', False), ('jc3', '5 TP 5555 1500\n', True),
                   ('jc3', '5 TP 5555 700\n', False), ('jc4', '5 TP 4184 1600\n', True)):
    rows, ev, out = run(tp, 30, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_checkpoint_%s' % (lv, tp.split()[3]), (rows[-1]['g4'] == 2) == ok, 'g4 %s' % rows[-1]['g4'])
rows, ev, out = run('5 TP 4184 1600\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'jc4', 'FP_CHAR': '0'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]   # (load lines come on stderr: the row after the last dying frame)
rs = rows[dead[-1] + 1:] if dead else []
check('jc4_restart_at_checkpoint', loads(ev).count('jc4') == 2 and rs and rs[0]['x'] == 4184 and rs[0]['y'] == 1640 + 14, str(rs[:1] and (rs[0]['x'], rs[0]['y'])))

# ---- music slots and the water line per act (levels.c; ML.B 10378 / 10566 / 10822 / 10863 / 11643)
for lv, mus in (('jc2', 'bgm/m_stage5.adp'), ('jc4', 'bgm/m_stage5b.adp'), ('jc5', 'bgm/m_stage5b.adp')):
    rows, ev, out = run('', 10, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_music' % lv, music(ev)[:1] == [mus], str(music(ev)))

# ---- JC1's entry (10365-10367; RV build/orig_traces/jc_flow/jc1_start_lilac.csv / _milla.csv): with the card (FP_CARD) the player
# is held at MV.X -40, MV.Y 229 (feet 243), then thrown in at the card's end with B 10, G 15: X -30, -20 ... and MV.Y + 15 per
# frame (RV frames 162-171), 38 frames later at (350, 814) (RV frame 200); Carol on foot gets an ObjectFuel (10366) = her bike
RV1 = [(-30, 244), (-20, 259), (-10, 274), (0, 289), (10, 304), (20, 319), (30, 334), (40, 349), (50, 364), (60, 379)]
for nm, ch in CHARS:
    rows, ev, out = run('', 300, {'FP_LEVEL': 'jc1', 'FP_CHAR': ch, 'FP_CARD': '1'})
    card = [r for r in rows if r['tm'] == 0 or r['f'] == 0]
    held = all(r['x'] == -40 and r['y'] == 243 for r in rows[:150])
    k = next((i for i, r in enumerate(rows) if r['x'] != -40), None)
    seq = [(r['x'], r['y'] - 14) for r in rows[k:k + 10]] if k else []
    check('jc1_entry_held_%s' % nm, held and jcf(ev, 'enter'), str([(r['x'], r['y']) for r in rows[:2]]))
    check('jc1_entry_thrown_in_%s' % nm, seq == RV1, str(seq[:4]))
    r38 = rows[k + 38] if k and k + 38 < len(rows) else None
    check('jc1_entry_rv_frame200_%s' % nm, r38 and r38['x'] == 350 and r38['y'] - 14 == 814, str(r38 and (r38['x'], r38['y'])))
    if nm == 'carol':
        check('jc1_carol_gets_the_bike', jcf(ev, 'fuel') and rows[-1]['bike'] == 1, 'bike %s' % rows[-1]['bike'])
    else:
        check('jc1_no_fuel_%s' % nm, not jcf(ev, 'fuel') and rows[-1]['bike'] == 0)

# ---- the water surface band (10368-10373: C = (piece5 box bottom - ML.B) / 70.0001, the strip anchor ML.B + (C + 1) 70 for
# C <= -1, else ML.B): RV ws_C / ws_y (LevelParalaxWaterSurface_14 alterable C and y) of the act-start traces
def band(lv, script, frames, env=None):
    e = {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_DBG_JCW': '1'}; e.update(env or {})
    rows, ev, out = run(script, frames, e)
    return {int(l.split()[1]): (float(l.split()[3]), float(l.split()[5])) for l in out.splitlines() if l.startswith('jcw ')}
b = band('jc1', '', 260, {'FP_CARD': '1'})
check('jc1_band_rv_start', b.get(1) == (-11.2857, 480.0), str(b.get(1)))                 # RV jc1_start_lilac f0: -11.2857, 480
check('jc1_band_rv_after_entry', b.get(40) == (-4.3714, 964.0), str(b.get(40)))          # RV f200: -4.37142, 964 (39 frames after the throw)
b = band('jc3', '', 5)
check('jc3_band_rv_start', b.get(1) == (0.3143, 1600.0), str(b.get(1)))                  # RV jc3_start_lilac f0: 0.314285, 1600
b = band('jc2', '', 5)
check('jc2_band_near_rv', b.get(1) and abs(b[1][0] - -15.8143) < 0.15, str(b.get(1)))     # RV f0 -15.8143 (the port's walk-in camera is 4 px lower)

# ---- the loop pieces (JC2 shaft x 5336-5700): JCLoop4 (Q10) holds the player on loop layer 0 (5600, top 712); JCLoop1 starts
# open (flag 0 on, RV jl1_fl 1) and turns solid once the player is below it and not touching it (10462)
rows, ev, out = run('5 TP 5600 640\n', 60, {'FP_LEVEL': 'jc2', 'FP_CHAR': '0'})
check('jc2_loop4_floor_layer0', rows[-1]['gnd'] == 1 and 712 <= rows[-1]['y'] < 714, 'y %s' % rows[-1]['y'])
check('jc2_loop1_turns_solid_below', jcf(ev, 'loop1 solid 1') and not jcf(ev, 'loop1 solid 0'), str([l for _, l in ev if l.startswith('jcflow')]))
rows, ev, out = run('5 TP 5620 1180\n', 30, {'FP_LEVEL': 'jc2', 'FP_CHAR': '0'})
check('jc2_loop1_both_below', jcf(ev, 'loop1 solid 0') and jcf(ev, 'loop1 solid 1'))
rows, ev, out = run('5 TP 5600 250\n', 30, {'FP_LEVEL': 'jc3', 'FP_CHAR': '0'})
check('jc3_loop1_open_on_layer0', not jcf(ev, 'loop1'), str([l for _, l in ev if l.startswith('jcflow')]))   # 10644 then 10645 (MV.P 0) each frame: stays open

# ---- cards (ObjectCodec A, newest first): JC1 51-53, JC2 54-56, JC3 57, JC4 58-60
for lv, want in (('jc1', {(10074.0, 237.0): 51, (5722.0, 1429.0): 52, (5594.0, 589.0): 53}),
                 ('jc2', {(12330.0, 981.0): 54, (6110.0, 989.0): 55, (5202.0, 1389.0): 56}), ('jc3', {(2066.0, 77.0): 57}),
                 ('jc4', {(8578.0, 1733.0): 58, (3434.0, 2229.0): 59, (2250.0, 853.0): 60})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_TRACK': '41:-1'})
    c = {(float(l.split()[5]), float(l.split()[7])): int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c == want, str(c))

# ---- the Classic route: SB1's results -> frame 8 -> JC1 (MenuSequence.I = 4 -> frame 39, RE handle_frame_9), fresh (g4 1)
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run('60 RESBOSS\n' + ''.join('%d TP 1300 1180\n' % f for f in range(10, 1000, 30)), 900,
                        {'FP_LEVEL': 'sb1', 'FP_SAVE': os.path.join(d, 's.bin')})
st = [l for l in loads(ev) if l not in ('frontend', 'gal')]
j1 = [r for r in rows if r['mode'] == 1 and r['act'] == 1 and r['f'] < 50 and r['x'] < 0]
check('route_sb1_to_jc1', st[-2:] == ['sb1', 'jc1'] and rows[-1]['g4'] == 1 and music(ev)[-1:] == ['bgm/m_stage5.adp'], str((st, music(ev)[-2:])))

# ---- Time Attack: Jade Creek (s 5, list index 5) with the title cheat (every stage's progress) starts JC1
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
TA = '200 202 S\n360 361 D\n380 381 J\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n690 691 D\n710 711 D\n740 741 J\n860 861 J\n'
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(CHEAT + TA, 1150, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_jc1_with_progress', 'jc1' in loads(ev) and rows[-1]['mode'] == 1 and rows[-1]['act'] == 1, str(loads(ev)))

# ---- data: the parallax set (D, E, G per piece; JC4 drifts G = (1 - D) * -4; boxes), the water mode, the layer-1 scroll 0.8
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, wm, ver = struct.unpack_from('<4H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh))
    return wm, out, struct.unpack_from('<i6H', dd, off) if wm == 2 else None
wm, p1, w1 = par('jc1')
check('par_jc1_set', wm == 2 and p1 == [(2336, 1, .94, 0, 0, 768, 240), (2337, .95, .95, 0, 96, 480, 160), (19, .9, .9, 0, 192, 320, 128),
      (2338, .85, .85, 0, 48, 454, 270), (2374, 1, 0, 0, 1200, 640, 320)] and w1 == (1200, 7, 454, 70, 2338, 270, 2374), str((p1, w1)))
wm, p4, w4 = par('jc4')
check('par_jc4_drift', (2524, .8, .85, -.8, 200, 391, 260) in p4 and (19, .9, .9, -.4, 208, 320, 128) in p4 and w4[0] == 1600, str(p4))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
