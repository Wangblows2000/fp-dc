#!/usr/bin/env python3
"""Host tests for the Trap Hideout stage flow (session 21b; src/game/levels.c rows th1-th3 and the Classic route, src/game/th_flow.c,
parallax.c (par_set_hook), tools/import_frame.py TH_FLOW blocks; build/re/spec_th_flow.md, RV traces build/orig_traces/th_flow/):
each act loads for Carol and Milla with its music (m_stage7) and cards (71 / 72-75 / 76-80); TH1's fresh start (start point + 16,
held at the top of the shaft by the card fade until frame 120, then the RV fall to MV.Y 242.75 at frame 165), TH1's shaft camera
(E = the right marker 511 until MV.Y > 2180, then D 2016, E W + 100), the checkpoints (13501, 13690, 13878 / 13879) and a restart,
the act chain TH1 -> TH2 -> TH3 by the right edge and the walk-ins (RV: B 3, g4 1 after 30 frames), TH3 without an edge exit, the
Crystal Basher's end (13931: g4 5, restart (560, 170), the spotlights / piece2 shown, slot-4 music m_boss2), the Classic route JC5
results -> TH1 for Carol / Milla and not for Lilac (handle_frame_9 inline), TH3's results -> the front end (Thermal Base not
ported), the stage-8 save (story block, last stage, progress, TA record), the achievements row, Time Attack's TH icon (Carol),
the parallax set and the scenery types.
usage: test_th_flow.py [data_dir]   (default: converted2; the session-21b worktrees build Trap Hideout into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (('carol', '2'), ('milla', '4'))
STAGES, FILES, NCH = 15, 8, 4
TH = 8                                                   # stage index of Trap Hideout
SAVE_CH = {'0': 0, '2': 1, '4': 2}

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD', 'FP_CARD_DBG')]
    e.update({'FP_TRACE_SND': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('music', 'thflow', 'save') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def thf(ev, what): return [i for i, l in ev if l.startswith('thflow ' + what)]

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c
def parse(path):
    if not os.path.exists(path): return None
    d = open(path, 'rb').read()
    if d[:3] != b'FPS' or crc16(d[:-2]) != struct.unpack('<H', d[-2:])[0]: return None
    p = 4 + 5; sv = {'files': []}
    for i in range(FILES):
        used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', d, p); p += 8
        st = []
        for s in range(STAGES): st.append(struct.unpack_from('<IHB', d, p)); p += 7
        sv['files'].append(dict(used=used, ch=ch, last=last, stage=st))
    sv['rec'] = [[struct.unpack_from('<IH', d, p + 6 * (s * NCH + c)) for c in range(NCH)] for s in range(STAGES)]; p += 6 * STAGES * NCH
    sv['progress'] = list(d[p:p + NCH])
    return sv

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- every act loads for Carol and Milla: the music (loop_play_music_5_56: m_stage7 in all three acts), the walk-in / start position
for nm, ch in CHARS:
    for lv, act in (('th1', 1), ('th2', 2), ('th3', 3)):
        rows, ev, out = run('', 60, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        check('%s_loads_%s' % (lv, nm), rows and rows[-1]['act'] == act and rows[-1]['chr'] == int(ch) and loads(ev)[-1:] == [lv], str(loads(ev)))
        check('%s_music_stage7_%s' % (lv, nm), music(ev)[:1] == ['bgm/m_stage7.adp'], str(music(ev)))

# ---- TH1's fresh start (4307 g4 1; 2956 X = start.x + 16 = 269, MV.Y = -35 + 16 = -19) and the hold of 13533 (RV th1_start_carol /
# _milla: MV.Y -19 / G 0 for frames 0-119; G 0.25 at 120 (the card fade's B = 96), then MV.Y -18.75, -18.25, -17.5 ...; lands at
# frame 165 on MV.Y 242.75; the port matches frame for frame)
RVF = {120: -19.0, 121: -18.75, 122: -18.25, 123: -17.5, 124: -16.5, 150: 97.25, 160: 186.0, 164: 228.5, 165: 242.75}
for nm, ch in CHARS:
    rows, ev, out = run('', 200, {'FP_LEVEL': 'th1', 'FP_CHAR': ch, 'FP_CARD': '1'})
    check('th1_start_point_%s' % nm, rows[0]['x'] == 269 and rows[0]['y'] - 14 == -19 and rows[0]['g4'] == 1, '%s %s' % (rows[0]['x'], rows[0]['y']))
    check('th1_held_until_120_%s' % nm, all(r['y'] - 14 == -19 and r['vy'] == 0 for r in rows[:120]) and thf(ev, 'th1 fall'),
          str([(i, r['y']) for i, r in enumerate(rows[:120]) if r['y'] - 14 != -19][:3]))
    got = {f: rows[f]['y'] - 14 for f in RVF}
    check('th1_fall_matches_rv_%s' % nm, got == RVF and rows[165]['gnd'] == 1 and rows[164]['gnd'] == 0, str(got))
rows, ev, out = run('', 200, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})   # no card (debug start): no hold
check('th1_no_hold_without_card', rows[5]['y'] - 14 > -19, str(rows[5]['y']))

# ---- TH1's shaft camera (13498-13500; RV th1_start: CAM.E 511 from frame 0; th1_bottom: Y > 2180 -> Sign D 1, CAM.D 2016, E 6100)
rows, ev, out = run('100 TP 450 1500\n110 300 R\n', 300, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})
check('th1_shaft_right_bound_511', max(r['cx'] for r in rows) <= 511 - 426 and rows[-1]['x'] > 300, 'max cx %s x %s' % (max(r['cx'] for r in rows), rows[-1]['x']))
rows, ev, out = run('100 TP 300 2250\n110 400 R\n', 400, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})
check('th1_bottom_opens_right', max(r['cx'] for r in rows) > 300, 'max cx %s' % max(r['cx'] for r in rows))
rows, ev, out = run('100 TP 300 2250\n120 TP 300 1900\n', 300, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})   # back up the shaft: the top stays at 2016
check('th1_bottom_top_bound_2016', min(r['cy'] for r in rows[125:]) == 2016 and rows[-1]['y'] < 2000, 'min cy %s y %s' % (min(r['cy'] for r in rows[125:]), rows[-1]['y']))
rows, ev, out = run('120 TP 300 1900\n', 300, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})
check('th1_shaft_no_top_bound', min(r['cy'] for r in rows[125:]) < 2016, 'min cy %s' % min(r['cy'] for r in rows[125:]))
rows, ev, out = run('100 TP 300 2150\n', 200, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})
check('th1_above_2180_keeps_the_shaft', max(r['cx'] for r in rows) <= 85, 'max cx %s' % max(r['cx'] for r in rows))

# ---- checkpoints: TH1 13501 (g4 < 2, MV.Y > 2208, X < 2000 -> (2, 180, 2288); RV th1_bottom: g4 2 g5 180 g6 2288 at TP (300, 2250)),
# TH2 13690 (7280 < X < 7480 -> (2, 7380, 570)), TH3 13878 (5446 < X < 5646 -> 6, (5546, 1520)), 13879 (11460 < X < 11560, MV.Y < 400 -> 7)
for lv, tp, g4 in (('th1', '5 TP 300 2250\n', 2), ('th1', '5 TP 2100 2250\n', 1), ('th1', '5 TP 300 2150\n', 1), ('th2', '5 TP 7380 560\n', 2),
                   ('th2', '5 TP 7600 560\n', 1), ('th3', '5 TP 5546 1500\n', 6), ('th3', '5 TP 11500 150\n', 7), ('th3', '5 TP 11500 600\n', 1)):
    rows, ev, out = run(tp, 30, {'FP_LEVEL': lv, 'FP_CHAR': '4'})
    check('%s_checkpoint_%s_%s' % (lv, tp.split()[2], tp.split()[3]), rows[-1]['g4'] == g4, 'g4 %s' % rows[-1]['g4'])
rows, ev, out = run('5 TP 300 2250\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'th1', 'FP_CHAR': '2'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('th1_restart_at_checkpoint', loads(ev).count('th1') == 2 and rs and rs[0]['x'] == 180 and rs[0]['y'] == 2288 + 14 and rs[0]['g4'] == 2,
      str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))
check('th1_restart_bottom_camera', rs and min(r['cy'] for r in rs[2:]) >= 2016, str(rs and min(r['cy'] for r in rs[2:])))

# ---- the act chain by the right edge (4135: X > W - 50, B > 0; 3578: g4 -1, frame + 1) and the walk-ins (4526: from x -32 at the
# start point + 16, B 3; RV th2_start / th3_start: g4 0 -> 1 after 30 frames, MV.Y 1642 / 2338 = the floor)
for lv, x, y, nx, act, sy in (('th1', 5900, 2300, 'th2', 2, 1637), ('th2', 12700, 1500, 'th3', 3, 2333)):
    for nm, ch in CHARS:
        rows, ev, out = run('5 TP %d %d\n6 400 R\n' % (x, y), 400, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        go = [r for r in rows if r['act'] == act]
        check('%s_exit_to_%s_%s' % (lv, nx, nm), nx in loads(ev) and go and go[0]['g4'] == -1 and go[0]['x'] == -32,
              str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['y'], go[0]['g4'])))
        g1 = [r for r in go if r['g4'] == 1]
        check('%s_walkin_ends_%s' % (nx, nm), g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36, str(g1[:1] and g1[0]['f'] - go[0]['f']))
rows, ev, out = run('5 TP 12700 700\n6 300 R\n', 300, {'FP_LEVEL': 'th3', 'FP_CHAR': '2'})
check('th3_no_edge_exit', all(r['act'] == 3 for r in rows) and loads(ev).count('th3') == 1, str(loads(ev)))

# ---- TH3: the Crystal Basher's end (13931: g4 5, (560, 170), camera released; 14083: the spotlights and piece2 shown, piece0 hidden),
# then the slot-4 music is m_boss2 (13702) and a death restarts at (560, 170)
rows, ev, out = run('30 THBASH\n40 HP 1\n42 HURT 99\n', 400, {'FP_LEVEL': 'th3', 'FP_CHAR': '2', 'FP_DBG_THPAR': '1'})
check('th3_basher_end_checkpoint', thf(ev, 'th3 basher cleared') and thf(ev, 'th3 spotlights'), str([l for _, l in ev if l.startswith('thflow')]))
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('th3_restart_after_basher', rs and rs[0]['x'] == 560 and rs[0]['y'] == 170 + 14 and rs[0]['g4'] == 5, str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))
pv = [l for l in out.splitlines() if l.startswith('thpar ')]
check('th3_parallax_switch', pv and pv[0].split()[2:] == ['0', '1'] and pv[-1].split()[2:] == ['1', '0'], str(pv[:1] + pv[-1:]))
rows, ev, out = run('', 5, {'FP_LEVEL': 'th3', 'FP_CHAR': '2', 'FP_DBG_THMUS': '1'})
m4 = [l for l in out.splitlines() if l.startswith('thmus4 ')]
check('th3_slot4_midboss_before_basher', m4 and m4[0].split()[1] == 'bgm/m_midboss.adp', str(m4[:1]))
rows, ev, out = run('2 THBASH\n', 5, {'FP_LEVEL': 'th3', 'FP_CHAR': '2', 'FP_DBG_THMUS': '1'})
m4 = [l for l in out.splitlines() if l.startswith('thmus4 ')]
check('th3_slot4_boss2_after_basher', m4 and m4[-1].split()[1] == 'bgm/m_boss2.adp', str(m4[-1:]))

# ---- cards (ObjectCodec A newest first, the template takes the next number): TH1 71, TH2 72-75, TH3 76-80
for lv, want in (('th1', {(3458.0, 2093.0): 71}), ('th2', {(8514.0, 541.0): 72, (4994.0, 1381.0): 73, (1762.0, 1205.0): 74, (2530.0, 1629.0): 75}),
                 ('th3', {(10482.0, 533.0): 76, (7810.0, 965.0): 77, (4770.0, 845.0): 78, (2906.0, 805.0): 79, (170.0, 1037.0): 80})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '2', 'FP_TRACK': '41:-1'})
    c = {(float(l.split()[5]), float(l.split()[7])): int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c == want, str(c))

# ---- the Classic route (RE handle_frame_9 inline, Classic block): JC5's results (MenuSequence.I 5) -> frame 56 = TH1 for g1 != 0
# (Carol, Milla), frame 44 = Thermal Base for Lilac (session 21c: TB1 when the data dir holds Thermal Base, else the front end;
# tests/test_tb_flow.py checks that route in full)
TB_PORTED = os.path.isdir(os.path.join(DATA, 'tb1'))
for nm, ch, want in (('carol', '2', 'th1'), ('milla', '4', 'th1'), ('lilac', '0', 'tb1' if TB_PORTED else 'frontend')):
    with tempfile.TemporaryDirectory() as d:
        # (session 21: the fast run earns JC's speedrunner achievement: its pop-up loads the front end, Jump closes it; then
        # Carol / Milla go on to TH1 and Lilac stays on the title menu: the Jump presses stop before they could start a game)
        rows, ev, out = run('60 RESBOSS\n' + ''.join('%d %d J\n' % (f, f + 2) for f in range(560, 1100 if ch != '0' else 640, 40)), 1500, {'FP_LEVEL': 'jc5', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin')})
    st = [l for l in loads(ev) if l != 'gal']
    st = [l for i, l in enumerate(st) if not (l == 'frontend' and i + 1 < len(st))]   # the pop-up's front end on the way
    ok = st[-2:] == ['jc5', want] and (want != 'th1' or (rows[-1]['act'] == 1 and rows[-1]['g4'] == 1 and music(ev)[-1:] == ['bgm/m_stage7.adp']))
    check('route_jc5_to_%s_%s' % (want, nm), ok and 'th1' not in st[:-1] and (want == 'th1' or 'th1' not in st), str((st, music(ev)[-2:])))

# ---- TH3's results (boss path, .res_wipe) -> frame 8 -> I 8 -> Thermal Base (g1 != 0, not ported): the front end; the save: story file
# block of stage 8, last stage 8, progress 9, the record rec[8][char] (RE 954 / 955 / 907 / 908; ML.Z > 10000 ms)
with tempfile.TemporaryDirectory() as d:
    for nm, ch in CHARS:
        c = SAVE_CH[ch]
        sv = os.path.join(d, 'story_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'th3', 'FP_CHAR': ch, 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        s = parse(sv); f = s and s['files'][2]
        st = [l for l in loads(ev) if l != 'gal']   # (the gallery's unlock notice screen may follow the front end)
        check('th3_results_to_frontend_%s' % nm, st[-2:] == ['th3', 'frontend'], str(loads(ev)))
        check('th3_story_block_stage8_%s' % nm, f and f['used'] == 1 and f['last'] == TH and f['stage'][TH][0] > 10000 and
              all(f['stage'][x][0] == 0 for x in range(STAGES) if x != TH), str(f and (f['last'], f['stage'][TH])))
        check('th3_progress_9_%s' % nm, s and s['progress'][c] == TH + 1, str(s and s['progress']))
        check('th3_record_stage8_%s' % nm, s and s['rec'][TH][c][0] == f['stage'][TH][0], str(s and s['rec'][TH]))
        check('th3_achievement_row_%s' % nm, 'ach results stage 8' in out, '')
        ta = os.path.join(d, 'ta_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'th3', 'FP_CHAR': ch, 'FP_TA': '1', 'FP_SAVE': ta})
        s = parse(ta)
        check('th3_ta_record_only_%s' % nm, s and s['rec'][TH][c][0] > 10000 and all(fl['used'] == 0 for fl in s['files']) and s['progress'][c] in (0, STAGES),
              str(s and (s['rec'][TH][c], s['progress'])))

# ---- Time Attack: Carol's list puts Trap Hideout (s 8) in slot 6 (RE 624); with every stage's progress (title cheat) it starts TH1
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
TA = '200 202 S\n360 361 D\n380 381 J\n470 471 R\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n690 691 D\n710 711 D\n730 731 D\n760 761 J\n880 881 J\n'
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(CHEAT + TA, 1200, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_carol_th1_with_progress', 'th1' in loads(ev) and rows[-1]['mode'] == 1 and rows[-1]['act'] == 1 and rows[-1]['chr'] == 2, str(loads(ev)))
# Lilac's list keeps Thermal Base (s 6) in that slot (RE 623): the same keys start TB1 (session 21c; nothing without its data), never TH1
TAL = TA.replace('470 471 R\n', '')
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(CHEAT + TAL, 1200, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_lilac_no_th', 'th1' not in loads(ev) and loads(ev)[-1:] == (['tb1'] if TB_PORTED else ['tasel']) and rows[-1]['chr'] == 0, str(loads(ev)))

# ---- scenery (th_flow.c): the backlights / flames take their rows and frames; TH3's spotlights stay off before the Crystal Basher
_, ev, out = run('', 3, {'FP_LEVEL': 'th1', 'FP_CHAR': '2', 'FP_TRACK': '1536:-1'})
bl = [l for l in out.splitlines() if l.startswith('ob 1 1536 ')]
check('th1_backlights', len(bl) == 7, str(len(bl)))
_, ev, out = run('', 3, {'FP_LEVEL': 'th2', 'FP_CHAR': '2', 'FP_TRACK': '1537:-1'})
check('th2_flames', len([l for l in out.splitlines() if l.startswith('ob 1 1537 ')]) == 58, '')
rows, ev, out = run('', 30, {'FP_LEVEL': 'th3', 'FP_CHAR': '4'})
check('th3_no_spotlights_at_start', not thf(ev, 'th3 spotlights'))

# ---- data: the parallax set (D, E, G per piece, boxes), the layer-1 scroll 0.8, no water
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, wm, ver = struct.unpack_from('<4H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh))
    return wm, out
check('par_th1', par('th1') == (0, [(3109, .6, .6, 0, 0, 128, 863)]), str(par('th1')))
check('par_th2', par('th2') == (0, [(3147, .6, .6, 0, 0, 100, 720)]), str(par('th2')))
check('par_th3', par('th3') == (0, [(3207, .95, .95, 0, 0, 320, 240), (3192, .6, .6, 0, 0, 100, 1000), (3208, .8, .85, 0, 184, 200, 320)]), str(par('th3')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
