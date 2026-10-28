#!/usr/bin/env python3
"""Host tests for the Battle Glacier stage flow (session 24; src/game/levels.c rows bg1-bg6, the Classic route, results.c ACH_STAGE,
src/game/bg_flow.c, tools/import_frame.py BG_FLOW blocks, parallax.c flag 8192; build/re/spec_bg_flow.md, RV traces
build/orig_traces/bg_flow/): every act loads for Lilac, Carol and Milla (no character forced) with its music (m_stage9 BG1-BG3, m_stage9b
BG5 / BG6, BG4 silent until its opening mech is beaten: 14607 / 14608) and a safe start; BG1's card ("STAGE 8" / "BATTLE GLACIER", RV
bg1_start: MV (77, 635) grounded, held); the act chain BG1 -> ... -> BG6 by the right edge (4135 -> 3578) for the three characters and the
walk-ins (RV bg2..bg6_start: MV at g4 1); BG6 without an edge exit; the checkpoints and a restart; BG4's opening clear (14502: g4 5 at
(444, 1800), play_music(5)) and its restart; cards 91-100; Milla's dig (BG1 / BG2 anywhere on the ground, none in BG3); the Classic route
(TB6 -> BG1 for Carol / Milla, PL6 -> BG1 for Lilac, BG6 -> Final Dreadnought round 1 = fd1a for everyone); the stage-9 save (story
block, last stage, progress, TA record) and the achievements row (32 / 52 under 720 s); Time Attack (slot 8 = Battle Glacier for the
three characters) and its preview strip (bg10layer1-4); the loop pieces (types 2304-2306); the parallax set and Brevon's ship.
usage: test_bg_flow.py [data_dir]   (default: converted2; the session-24 worktrees build Battle Glacier into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, NCH = 15, 8, 4
BG = 9                                                   # stage index of Battle Glacier
CHARS = (('lilac', '0'), ('carol', '2'), ('milla', '4'))
SAVE_CH = {'0': 0, '2': 1, '4': 2}                       # record column per character (chr_slot)

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD',
                                                     'FP_CARD_DBG', 'FP_DBG_BGOPEN', 'FP_INVULN')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('music', 'bgflow', 'save', 'sid', 'ach') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def bgf(ev, what): return [i for i, l in ev if l.startswith('bgflow ' + what)]

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

ACTS = ('bg1', 'bg2', 'bg3', 'bg4', 'bg5', 'bg6')
# ---- every act loads for every character, keeps her (no g1 write: RV bg1_start_carol g1 2, bg1_start_milla g1 4), plays slot 5 (RE loop
# wrappers: m_stage9 BG1-BG3, m_stage9b BG5 / BG6; BG4 14607: fadeout while g4 < 5) and starts safely (debug start = g4 1: the start point
# + 16, grounded, unhurt; BG1 at RV MV (77, 635)); dry (ML.A 1 / B 2000-3000, no water events 4150-4170)
MUS = {'bg1': ['bgm/m_stage9.adp'], 'bg2': ['bgm/m_stage9.adp'], 'bg3': ['bgm/m_stage9.adp'], 'bg4': [], 'bg5': ['bgm/m_stage9b.adp'],
       'bg6': ['bgm/m_stage9b.adp']}
for act, lv in enumerate(ACTS, 1):
    for nm, ch in CHARS:
        if nm != 'lilac' and lv in ('bg2', 'bg3', 'bg6'): continue   # (time: the three characters on BG1 / BG4 / BG5)
        rows, ev, out = run('', 200, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        r = rows[-1]
        check('%s_loads_%s' % (lv, nm), r['act'] == act and r['chr'] == int(ch) and loads(ev)[-1:] == [lv], str((loads(ev), r['chr'])))
        check('%s_music_%s' % (lv, nm), music(ev) == MUS[lv], str(music(ev)))
        check('%s_safe_start_%s' % (lv, nm), r['gnd'] == 1 and r['hp'] == 14 and r['st'] == 0 and r['g4'] == 1 and
              (lv != 'bg1' or (r['x'] == 77 and abs(r['y'] - 14 - 635) <= 0.5)), 'x %s y %s gnd %s hp %s' % (r['x'], r['y'], r['gnd'], r['hp']))
        check('%s_dry_%s' % (lv, nm), all(x.get('air', 0) == 0 for x in rows), '')

# ---- BG1's card (4307; 14178: Intro2 frame 10 "BATTLE GLACIER", Intro3 frame 7 "STAGE 8"): the player held at the start (RV bg1_start_*:
# MV (77, 635), L 1 from frame 0, g4 1)
for nm, ch in CHARS:
    rows, ev, out = run('', 200, {'FP_LEVEL': 'bg1', 'FP_CHAR': ch, 'FP_CARD': '1', 'FP_CARD_DBG': '1'})
    if nm == 'lilac': check('bg1_card_images', 'card bg1 stage 103x16 name 165x35' in out, str([l for l in out.splitlines() if l.startswith('card')][:1]))
    check('bg1_card_start_%s' % nm, all(r['x'] == 77 and abs(r['y'] - 14 - 635) <= 0.5 and r['gnd'] == 1 for r in rows[5:190]) and rows[-1]['g4'] == 1,
          str((rows[-1]['x'], rows[-1]['y'])))

# ---- the act chain by the right edge (4135: X > W - 50, B > 0 -> 3578) and the walk-ins (4526: x -32, B 3; g4 1 after 30 frames; RV
# bg2..bg6_start_lilac / bg4_start_carol / bg5_start_milla frame 30: MV (60.92, 356.17), (60.92, 1060.17), (61, 1810), (61, 1042), (61, 1938))
WALK = {'bg2': (60.92, 356.17), 'bg3': (60.92, 1060.17), 'bg4': (61, 1810), 'bg5': (61, 1042), 'bg6': (61, 1938)}
EDGE = {'bg1': (12516, 100), 'bg2': (13300, 100), 'bg3': (10836, 100), 'bg4': (12516, 1150), 'bg5': (14484, 100)}
for i, lv in enumerate(ACTS[:-1]):
    nx = ACTS[i + 1]
    for nm, ch in CHARS:
        rows, ev, out = run('3 SETV others E 99999\n5 TP %d %d\n60 300 R\n' % EDGE[lv], 300, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        go = [r for r in rows if r['act'] == i + 2]
        check('%s_exit_to_%s_%s' % (lv, nx, nm), loads(ev)[-2:] == [lv, nx] and go and go[0]['g4'] == -1 and go[0]['x'] == -32 and go[0]['chr'] == int(ch),
              str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['g4'])))
        g1 = [r for r in go if r['g4'] == 1]
        check('%s_walkin_ends_%s' % (nx, nm), g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36 and abs(g1[0]['y'] - 14 - WALK[nx][1]) < 2.5 and
              abs(g1[0]['x'] - WALK[nx][0]) < 2.5 and g1[0]['gnd'] == 1, str(g1[:1] and (g1[0]['f'] - go[0]['f'], g1[0]['x'], g1[0]['y'] - 14)))
        if nx == 'bg4': check('bg4_silent_after_bg3_%s' % nm, music(ev) == ['bgm/m_stage9.adp'], str(music(ev)))   # 14607: no play_music(5)
rows, ev, out = run('3 SETV others E 99999\n5 TP 3000 1700\n6 300 R\n', 300, {'FP_LEVEL': 'bg6', 'FP_CHAR': '0'})
check('bg6_no_edge_exit', all(r['act'] == 6 for r in rows) and loads(ev).count('bg6') == 1, str(loads(ev)))

# ---- checkpoints (RE inline handle_frame_60..64: BG1 (2, 4438, 900) / (3, 9458, 620); BG2 (2, 6150, 96); BG3 (2, 6138, 420); BG5 (1, 2836,
# 1260) only while g4 < 1 (MV.Y > 1000) / (2, 5000, 720) / (3, 7550, 400) / (4, 8260, 1870) MV.Y > 1600; BG4 / BG6 none)
for lv, tp, g4, g5 in (('bg1', '5 TP 4480 880\n', 2, 4438), ('bg1', '5 TP 9450 600\n', 3, 9458), ('bg1', '5 TP 4600 880\n', 1, 0),
                       ('bg2', '5 TP 6100 80\n', 2, 6150), ('bg3', '5 TP 6100 400\n', 2, 6138), ('bg5', '5 TP 2836 1240\n', 1, 0),
                       ('bg5', '5 TP 5100 700\n', 2, 5000), ('bg5', '5 TP 7550 380\n', 3, 7550), ('bg5', '5 TP 8250 1850\n', 4, 8260),
                       ('bg5', '5 TP 8250 1000\n', 1, 0)):
    rows, ev, out = run(tp, 12, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_checkpoint_%s_%s' % (lv, tp.split()[2], tp.split()[3]), rows[-1]['g4'] == g4, 'g4 %s' % rows[-1]['g4'])
for nm, ch in CHARS:
    rows, ev, out = run('5 TP 4480 880\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'bg1', 'FP_CHAR': ch})
    dead = [i for i, r in enumerate(rows) if r['st'] == 6]
    rs = rows[dead[-1] + 1:] if dead else []
    check('bg1_restart_at_checkpoint_%s' % nm, loads(ev).count('bg1') == 2 and rs and rs[0]['x'] == 4438 and rs[0]['y'] == 900 + 14 and rs[0]['g4'] == 2,
          str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))

# ---- BG4's opening (14607 fadeout while g4 < 5; the Brevon mech's end 14502 -> bg_flow_opening_cleared: g4 5 at (444, 1800), play_music(5);
# host stand-in FP_DBG_BGOPEN = the boss module's call); a death after it restarts there with the music (14608)
rows, ev, out = run('', 200, {'FP_LEVEL': 'bg4', 'FP_CHAR': '0', 'FP_DBG_BGOPEN': '100'})
oc = bgf(ev, 'opening cleared')
check('bg4_opening_cleared', oc and rows[oc[0]]['g4'] == 5 and music(ev) == ['bgm/m_stage9b.adp'] and rows[-1]['g4'] == 5, str((oc, music(ev))))
rows, ev, out = run('120 HP 1\n122 HURT 99\n', 500, {'FP_LEVEL': 'bg4', 'FP_CHAR': '2', 'FP_DBG_BGOPEN': '100'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('bg4_restart_after_opening', loads(ev).count('bg4') == 2 and rs and rs[0]['x'] == 444 and rs[0]['y'] == 1800 + 14 and rs[0]['g4'] == 5 and
      music(ev)[-1:] == ['bgm/m_stage9b.adp'] and len(music(ev)) == 2, str((rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4']), music(ev))))
rows, ev, out = run('', 200, {'FP_LEVEL': 'bg5', 'FP_CHAR': '0', 'FP_DBG_BGOPEN': '100'})
check('bg_opening_only_bg4', not bgf(ev, 'opening') and rows[-1]['g4'] == 1, '')

# ---- cards (ObjectCodec A newest first from the start counters 91 / 93 / 95 / 97 / 99; the template takes the next number)
for lv, want in (('bg1', {91, 92}), ('bg2', {93, 94}), ('bg3', {95, 96}), ('bg4', {97, 98}), ('bg5', {99, 100})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': '41:-1'})
    c = {int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c >= want and min(c) == min(want), str(c))

# ---- Milla's dig (BG1 inline / BG2 14165: MV.L 1 -> MILLA.D 1 anywhere; BG3: no writer, RE by absence): the BG1 spot (1496, 808) is A 0
# (5821: MonitorEarthShield) and its use earns records (2, 38) (4189)
DIG = '80 300 D\n100 102 X\n' + ''.join('%d %d X\n' % (f, f + 2) for f in range(110, 290, 10))
rows, ev, out = run('5 TP 1500 790\n' + DIG, 320, {'FP_LEVEL': 'bg1', 'FP_CHAR': '4'})
check('bg1_milla_digs_spot', any(r['mm'] == 27 for r in rows) and 'ach earn 38' in out, str(max(r['mm'] for r in rows)))
rows, ev, out = run('5 TP 1904 640\n' + DIG, 320, {'FP_LEVEL': 'bg3', 'FP_CHAR': '4'})
check('bg3_milla_no_dig', not any(r['mm'] == 27 for r in rows) and 'ach earn 38' not in out, '')

# ---- the Classic route (RE handle_frame_9 inline, Classic block): Thermal Base's results (I 6) -> frame 84 for Carol / Milla, Pangu
# Lagoon's (I 7) -> frame 84 for Lilac; frame 84 (the shoot-'em-up lead-in, not ported) ends in frame 59 = BG1 (22695): BG1 with the
# character kept; BG6's results (I 9) -> frame 65 = Final Dreadnought round 1 (fd1a) for every character
JPRESS = '1200 1202 J\n'   # closes the achievement pop-up (frame 9) once
for lv, nm, ch, want in (('tb6', 'carol', '2', 'bg1'), ('tb6', 'milla', '4', 'bg1'), ('pl6', 'lilac', '0', 'bg1'),
                         ('bg6', 'lilac', '0', 'fd1a'), ('bg6', 'carol', '2', 'fd1a'), ('bg6', 'milla', '4', 'fd1a')):
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('700 RESBOSS\n' + JPRESS, 2000, {'FP_LEVEL': lv, 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin'), 'FP_FILE': '2', 'FP_FILE_NEW': '1'})
    st = [l for l in loads(ev) if l not in ('gal', 'frontend')]
    check('route_%s_to_%s_%s' % (lv, want, nm), st[-2:] == [lv, want] and rows[-1]['chr'] == int(ch) and rows[-1]['act'] == 1, str((st, rows[-1]['chr'])))

# ---- BG6's results (the boss path, .res_wipe); the save: the story block of stage 9, last stage 9, progress 10, the record rec[9][char]
# (RE 954 / 955 / 907 / 908), the achievements row (15127: cards 91-100 -> 32; handle_frame_65 inline: 52 with ML.Z <= 720000 ms); Time
# Attack: the record only
with tempfile.TemporaryDirectory() as d:
    for nm, ch in CHARS:
        c = SAVE_CH[ch]
        sv = os.path.join(d, 'story_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'bg6', 'FP_CHAR': ch, 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        s = parse(sv); f = s and s['files'][2]
        check('bg6_story_block_stage9_%s' % nm, f and f['used'] == 1 and f['last'] == BG and f['stage'][BG][0] > 10000 and
              all(f['stage'][x][0] == 0 for x in range(STAGES) if x != BG), str(f and (f['last'], f['stage'][BG])))
        check('bg6_progress_10_%s' % nm, s and s['progress'][c] == BG + 1, str(s and s['progress']))
        check('bg6_record_stage9_%s' % nm, s and s['rec'][BG][c][0] == f['stage'][BG][0], str(s and s['rec'][BG]))
        check('bg6_achievement_row_%s' % nm, 'ach results stage 9' in out and 'ach earn 52' in out, '')
    ta = os.path.join(d, 'ta.bin')
    rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'bg6', 'FP_CHAR': '0', 'FP_TA': '1', 'FP_SAVE': ta})
    s = parse(ta)
    check('bg6_ta_record_only', s and s['rec'][BG][0][0] > 10000 and all(fl['used'] == 0 for fl in s['files']), str(s and (s['rec'][BG][0], s['progress'])))
rows, ev, out = run('43300 RESBOSS\n', 44200, {'FP_LEVEL': 'bg6', 'FP_CHAR': '0'})   # 43300 frames = 721.7 s; the limit is 720 s
check('bg6_speedrunner_limit', 'ach results stage 9' in out and 'ach earn 52' not in out, '')

# ---- Time Attack (title cheat: every stage's progress): slot 8 holds Battle Glacier (E 9) for every character (RE 623-625 change slots 6 / 7
# only) -> BG1 with its card; the preview strip of E 9 = bg10layer1-4 (frame 6, y 488 = 56 + 48 * 9) is in the screen's data
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
DOWN8 = ''.join('%d %d D\n' % (f, f + 1) for f in (630, 645, 660, 675, 690, 705, 720, 735))
for nm, sel, chn in (('lilac', '', 0), ('carol', '470 471 R\n', 2), ('milla', '470 471 R\n480 481 R\n', 4)):
    keys = '200 202 S\n360 361 D\n380 381 J\n' + sel + '490 491 J\n' + DOWN8 + '760 761 J\n880 881 J\n'
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run(CHEAT + keys, 1300, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_%s_slot8_bg1' % nm, 'bg1' in loads(ev) and rows and rows[-1]['act'] == 1 and rows[-1]['chr'] == chn, str(loads(ev)))
ui = open(os.path.join(DATA, 'ui', 'tasel_6.ui'), 'rb').read()
check('ta_preview_strip_bg10', all(b'bg10layer%d' % k in ui for k in (1, 2, 3, 4)), '')

# ---- the loop pieces (RE on_frame_63 / 64_init: BGLoop1 Q11 = layer 1, BGLoop2 / 3 Q10 = layer 0; 14345-14349 = 3031 / 3032): flag 0 (not
# solid) follows MV.P (0 at the start: the layer-1 pieces off); BG4 6 / 2 / 31, BG5 3 / 1 / 1 (the off-map templates gone)
for lv, want in (('bg4', {2304: 6, 2305: 2, 2306: 31}), ('bg5', {2304: 3, 2305: 1, 2306: 1})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': '2304:-1,2305:-1,2306:-1'})
    pcs = [l.split() for l in o.splitlines() if l.startswith('ob 1 ')]
    cnt = {t: sum(1 for p in pcs if int(p[2]) == t) for t in want}
    fl = {int(p[2]): int(p[p.index('fl') + 1], 16) & 1 for p in pcs}
    check('%s_loop_pieces' % lv, cnt == want and fl == {2304: 1, 2305: 0, 2306: 0} and all(float(p[5]) > 0 for p in pcs), str((cnt, fl)))

# ---- data: the parallax set (D, E, G per piece, boxes) (RE create_levelparallaxpiece* + BG1 start inline / 14173 / 14603 / BG5 inline /
# 15151) and Brevon's ship (BG4 3310: D .96, E .9 at x 560; BG5 close views 3353 / 3364: D .94, E .8 at x 744 / 1049; flag 8192)
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, wm, ver = struct.unpack_from('<4H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh, fl))
    return sorted(out)
A13 = [(3223, .9, .9, -.3, 0, 320, 200, 0), (3224, .96, .96, 0, 0, 320, 180, 0), (3225, .9, .9, 0, 168, 320, 180, 0),
       (3226, .85, .85, 0, 224, 320, 160, 0), (3227, .75, .75, 0, 304, 320, 320, 0)]
A46 = [(3224, .96, .96, 0, 0, 320, 180, 0), (3226, .98, .92, 0, 224, 320, 160, 0), (3307, .9, .9, -.3, 0, 320, 314, 0),
       (3308, .95, .85, 0, 280, 320, 280, 0)]
check('par_bg1', par('bg1') == A13, str(par('bg1')))
check('par_bg3', par('bg3') == A13, str(par('bg3')))
check('par_bg4', par('bg4') == sorted(A46 + [(3310, .96, .9, 0, 16, 560, 329, 8192)]), str(par('bg4')))
check('par_bg5', par('bg5') == sorted(A46 + [(3353, .94, .8, 0, -40, 744, 657, 8192), (3364, .94, .8, 0, -40, 1049, 657, 8192)]), str(par('bg5')))
check('par_bg6', par('bg6') == A46, str(par('bg6')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
