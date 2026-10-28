#!/usr/bin/env python3
"""Host tests for the Pangu Lagoon stage flow (session 23; src/game/levels.c rows pl1-pl6 and the Classic route, src/game/pl_flow.c,
tools/import_frame.py PL_FLOW blocks; build/re/spec_pl_flow.md, RV traces build/orig_traces/pl_flow/):
every act loads for Lilac with its music (m_stage8 PL1-PL3, m_stage8b PL4-PL6) and a safe start; Carol / Milla are played as Lilac
(RE 12844: g1 = 0 at every act start); PL1's card (frames 6 / 8); the act chain PL1 -> ... -> PL6 (the right edge in PL1 / PL2 / PL4 /
PL5; PL3's stone door, RE 13009 / 12997-13006, with the door reported open) and the walk-ins (RV pl2..pl6_start_lilac: MV.Y at the end
of the walk-in); PL6 without an edge exit, its door into the Holo-Dragon arena (13236 / 13237: g4 3 at (4480, 208), MV (6080, 24)) and
the restart there; the door needs the door open (A < 0.4) and the player between the two doors; PL3's door room camera and music fade;
the checkpoints; no water; cards 81-90; the Classic route (TB6's results -> PL1 for Lilac, BG1 for Carol / Milla; PL6's results -> BG1); the stage-7
save (story block, last stage, progress, TA record) and the achievements row (51 under 480 s); Time Attack (Lilac's slot 7 -> PL1;
Carol's / Milla's slot 7 is Thermal Base: they never reach Pangu Lagoon); the loop pieces (types 2048-2051 follow the loop layer);
the parallax set.
usage: test_pl_flow.py [data_dir]   (default: converted2; the session-23 worktrees build Pangu Lagoon into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, NCH = 15, 8, 4
PL = 7                                                   # stage index of Pangu Lagoon
DOOR3 = '5976,6071,0'                                    # PL3 StoneDoor1 / StoneDoor2 x (json 53), StoneDoor1.A 0 = open
DOOR6 = '4432,4527,0'                                    # PL6 (json 56)

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD',
                                                     'FP_CARD_DBG', 'FP_DBG_PLDOOR', 'FP_INVULN')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('music', 'plflow', 'save', 'sid', 'ach') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def plf(ev, what): return [i for i, l in ev if l.startswith('plflow ' + what)]

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

ACTS = ('pl1', 'pl2', 'pl3', 'pl4', 'pl5', 'pl6')
# ---- every act loads for Lilac: act, the music (RE loop wrappers: slot 5 m_stage8 PL1-PL3, m_stage8b PL4-PL6) and a safe start (debug
# start = g4 1: the start point + 16, grounded, unhurt; PL1 / PL2 / PL4 / PL5 on flat ground at the RV walk-in / card heights MV.Y 1785 /
# 1818 / 434 / 754)
SAFE_Y = {'pl1': 1785, 'pl2': 1818, 'pl4': 434, 'pl5': 754}
MUS = {'pl1': 'bgm/m_stage8.adp', 'pl2': 'bgm/m_stage8.adp', 'pl3': 'bgm/m_stage8.adp', 'pl4': 'bgm/m_stage8b.adp', 'pl5': 'bgm/m_stage8b.adp',
       'pl6': 'bgm/m_stage8b.adp'}
for act, lv in enumerate(ACTS, 1):
    rows, ev, out = run('', 240, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_loads' % lv, rows and rows[-1]['act'] == act and rows[-1]['chr'] == 0 and loads(ev)[-1:] == [lv], str(loads(ev)))
    check('%s_music' % lv, music(ev) == [MUS[lv]], str(music(ev)))
    r = rows[-1]
    check('%s_safe_start' % lv, r['gnd'] == 1 and r['hp'] == 14 and r['st'] == 0 and (lv not in SAFE_Y or abs(r['y'] - 14 - SAFE_Y[lv]) <= 0.5),
          'y %s gnd %s hp %s' % (r['y'], r['gnd'], r['hp']))
    check('%s_dry' % lv, all(x.get('air', 0) == 0 for x in rows), '')   # ML.A 1 / B 2000 but no water events 4150-4170 (RE)
# ---- Lilac only (RE 12844 / PL1's start inline: g1 = 0 at every act start): Carol / Milla (the debug select) play it as Lilac
for nm, ch in (('carol', '2'), ('milla', '4')):
    for lv in ('pl1', 'pl4'):
        rows, ev, out = run('', 30, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        check('%s_forces_lilac_%s' % (lv, nm), rows and rows[-1]['chr'] == 0 and rows[-1]['bike'] == 0, str(rows and rows[-1]['chr']))
# ---- PL1's card (4307 / 12854: Intro3 frame 6 "STAGE 7", Intro2 frame 8 "PANGU LAGOON"; importer STAGE_CARDS (50, 55, 8, 6)): held during
# it (RV pl1_start_lilac: MV (141, 1785) grounded from frame 0)
rows, ev, out = run('', 200, {'FP_LEVEL': 'pl1', 'FP_CHAR': '0', 'FP_CARD': '1', 'FP_CARD_DBG': '1'})
check('pl1_card_images', 'card pl1 stage 103x16 name 172x41' in out, [l for l in out.splitlines() if l.startswith('card')][:1].__str__())
check('pl1_card_start', all(r['x'] == 141 and abs(r['y'] - 14 - 1785) <= 0.5 for r in rows[5:190]) and rows[-1]['g4'] == 1, str(rows[-1]['x']))

# ---- the act chain by the right edge (4135: X > W - 50, B > 0 -> 3578) and the walk-ins (4526: x -32, B 3; g4 1 after 30 frames; RV
# pl2..pl6_start_lilac frame 30 (g4 1): MV (61, 1818), (60.86, 694.45), (61, 434), (61, 754), (46.75, 499.8))
WALK = {'pl2': (61, 1818), 'pl3': (60.86, 694.45), 'pl4': (61, 434), 'pl5': (61, 754), 'pl6': (46.75, 499.8)}
WALK_Y = {k: v[1] for k, v in WALK.items()}
EDGE = {'pl1': (13600, 300), 'pl2': (12500, 300), 'pl4': (10800, 300), 'pl5': (11000, 1200)}
for lv, nx in (('pl1', 'pl2'), ('pl2', 'pl3'), ('pl4', 'pl5'), ('pl5', 'pl6')):
    rows, ev, out = run('3 SETV others E 99999\n5 TP %d %d\n100 450 R\n' % EDGE[lv], 450, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    act = ACTS.index(nx) + 1
    go = [r for r in rows if r['act'] == act]
    check('%s_exit_to_%s' % (lv, nx), loads(ev)[-2:] == [lv, nx] and go and go[0]['g4'] == -1 and go[0]['x'] == -32, str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['g4'])))
    g1 = [r for r in go if r['g4'] == 1]
    check('%s_walkin_ends' % nx, g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36 and abs(g1[0]['y'] - 14 - WALK[nx][1]) < 2.5 and abs(g1[0]['x'] - WALK[nx][0]) < 2.5 and g1[0]['gnd'] == 1,
          str(g1[:1] and (g1[0]['f'] - go[0]['f'], g1[0]['x'], g1[0]['y'] - 14)))
rows, ev, out = run('3 SETV others E 99999\n5 TP 11300 1300\n6 300 R\n', 300, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0'})
check('pl6_no_edge_exit', all(r['act'] == 6 for r in rows) and loads(ev).count('pl6') == 1, str(loads(ev)))

# ---- PL3's stone door (13009: between StoneDoor1 / 2, StoneDoor1.A < 0.4, MV.M 0, grounded; 12997 music stop; walk to door x + 48 (13000-
# 13002); C 120 -> the fade (13004 / 13005); alpha 255 -> next frame, g4 -1 (13006): PL4's walk-in)
rows, ev, out = run('5 TP 5990 150\n', 420, {'FP_LEVEL': 'pl3', 'FP_CHAR': '0', 'FP_DBG_PLDOOR': DOOR3})
st, fd = plf(ev, 'door start'), plf(ev, 'door fade')
check('pl3_door_starts', st and fd and fd[0] - st[0] == 120, str((st, fd)))
w = rows[st[0] + 20:fd[0]] if st and fd else []
check('pl3_door_walk_to_door', w and all(r['x'] == 6024 and r['gsp'] == 0 for r in w), str(w[:1] and (w[0]['x'], w[0]['gsp'])))
check('pl3_door_music_stop', any(l.startswith('music stop') for _, l in ev), '')
go = [r for r in rows if r['act'] == 4]
check('pl3_door_to_pl4', loads(ev)[-2:] == ['pl3', 'pl4'] and go and go[0]['g4'] == -1 and go[0]['x'] == -32, str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['g4'])))
check('pl4_walkin_after_door', go and abs(go[-1]['y'] - 14 - WALK_Y['pl4']) < 2.5 and go[-1]['g4'] == 1 and music(ev)[-1:] == ['bgm/m_stage8b.adp'], str(go[-1:] and go[-1]['y']))
for tag, env, scr in (('closed', '5976,6071,0.5', '5 TP 5990 150\n'), ('outside', DOOR3, '5 TP 5900 150\n'), ('noreport', None, '5 TP 5990 150\n')):
    e = {'FP_LEVEL': 'pl3', 'FP_CHAR': '0'}
    if env: e['FP_DBG_PLDOOR'] = env
    rows, ev, out = run(scr, 260, e)
    check('pl3_door_needs_%s' % tag, not plf(ev, 'door start') and loads(ev).count('pl3') == 1 and rows[-1]['act'] == 3, str(plf(ev, 'door')))
# the door room camera (13007 / 13008, levels.c CAM_PL3): CAM.M 1 to the markers (4976, 8, 6247, 279) on entering, fadeout_music(6000)
rows, ev, out = run('5 TP 6150 150\n', 130, {'FP_LEVEL': 'pl3', 'FP_CHAR': '0'})
check('pl3_room_camera', plf(ev, 'room lock 3') and rows[-1]['cx'] <= 6247 - 426 + 0.5, 'cx %s' % rows[-1]['cx'])
rows, ev, out = run('5 TP 4700 150\n', 130, {'FP_LEVEL': 'pl3', 'FP_CHAR': '0'})
check('pl3_room_camera_outside', not plf(ev, 'room lock'), '')

# ---- PL6's door into the arena (13329 room lock; 13330 + 13231-13235; 13236 g4 < 3 -> checkpoint (3, 4480, 208); 13237 MV (6080, 24), CTRL.M 0,
# camera C 4928 (13331)); a death there restarts at (4480, 208) with g4 3
rows, ev, out = run('5 TP 4470 1450\n', 420, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0', 'FP_DBG_PLDOOR': DOOR6})
ar = plf(ev, 'arena')
check('pl6_room_lock', plf(ev, 'room lock'), '')
check('pl6_door_to_arena', ar and rows[ar[0]]['x'] == 6080 and rows[ar[0]]['g4'] == 3 and loads(ev).count('pl6') == 1 and rows[-1]['act'] == 6, str(ar))
check('pl6_arena_lands', rows[-1]['gnd'] == 1 and rows[-1]['x'] == 6080 and rows[-1]['cx'] >= 4928, 'y %s cx %s' % (rows[-1]['y'], rows[-1]['cx']))
check('pl6_door_music_fade', any(l.startswith('music stop') for _, l in ev) and not plf(ev, 'door fade') == [], '')
rows, ev, out = run('5 TP 4470 1450\n300 HP 1\n302 HURT 99\n', 700, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0', 'FP_DBG_PLDOOR': DOOR6})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('pl6_restart_at_door_checkpoint', loads(ev).count('pl6') == 2 and rs and rs[0]['x'] == 4480 and rs[0]['y'] == 208 + 14 and rs[0]['g4'] == 3,
      str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))

# ---- checkpoints (RE: PL1 inline (2, 9264, 474); PL2 inline (2, 6936, 414) MV.Y < 444; PL4 inline (2, 3322, 1410) MV.Y > 1000 / (2, 6767, 1100)
# (g4 < 3: re-arms); PL5 inline (2, 5864, 1588) / (3, 7980, 370) MV.Y < 600; PL6 13327 (2, 2834, 1550))
for lv, tp, g4, g5 in (('pl1', '5 TP 9300 430\n', 2, 9264), ('pl1', '5 TP 9500 430\n', 1, 0), ('pl2', '5 TP 7000 400\n', 2, 6936),
                       ('pl2', '5 TP 7000 900\n', 1, 0), ('pl4', '5 TP 3400 1400\n', 2, 3322), ('pl4', '5 TP 6800 1050\n', 2, 6767),
                       ('pl5', '5 TP 5864 1560\n', 2, 5864), ('pl5', '5 TP 7950 350\n', 3, 7980), ('pl6', '5 TP 2900 1500\n', 2, 2834)):
    rows, ev, out = run(tp, 12, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_checkpoint_%s_%s' % (lv, tp.split()[2], tp.split()[3]), rows[-1]['g4'] == g4, 'g4 %s' % rows[-1]['g4'])
rows, ev, out = run('5 TP 9300 430\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'pl1', 'FP_CHAR': '0'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('pl1_restart_at_checkpoint', loads(ev).count('pl1') == 2 and rs and rs[0]['x'] == 9264 and rs[0]['y'] == 474 + 14 and rs[0]['g4'] == 2,
      str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))

# ---- cards (ObjectCodec A newest first from the start counters 81 / 83 / 86 / 86 / 88; the template takes the next number)
for lv, want in (('pl1', {81, 82}), ('pl2', {83, 84, 85}), ('pl4', {86, 87}), ('pl5', {88, 89, 90})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': '41:-1'})
    c = {int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c >= want and min(c) == min(want), str(c))

# ---- the Classic route (RE handle_frame_9 inline, Classic block): Thermal Base's results (I 6) -> frame 50 = PL1 for g1 = 0 (Lilac),
# frame 84 for Carol / Milla; PL6's results (I 7) -> frame 84 for g1 = 0. Frame 84 (the shoot-'em-up lead-in, not ported) ends in frame
# 59 = BG1 (22695; session 24): the port goes straight to Battle Glacier
JPRESS = '1200 1202 J\n'   # closes the achievement pop-up (frame 9) once
for nm, ch, want in (('lilac', '0', 'pl1'), ('carol', '2', 'bg1'), ('milla', '4', 'bg1')):
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('700 RESBOSS\n' + JPRESS, 2000, {'FP_LEVEL': 'tb6', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin'), 'FP_FILE': '2', 'FP_FILE_NEW': '1'})
    st = [l for l in loads(ev) if l not in ('gal', 'frontend')] + (['frontend'] if loads(ev)[-1:] in (['frontend'], ['gal']) else [])
    ok = st[-2:] == ['tb6', want] and (want != 'pl1' or (rows[-1]['act'] == 1 and rows[-1]['chr'] == 0 and music(ev)[-1:] == ['bgm/m_stage8.adp']))
    check('route_tb6_to_%s_%s' % (want, nm), ok and (want == 'pl1' or ('pl1' not in st and rows[-1]['chr'] == int(ch))), str((st, music(ev)[-2:])))
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run('700 RESBOSS\n' + JPRESS, 2000, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0', 'FP_SAVE': os.path.join(d, 's.bin'), 'FP_FILE': '2', 'FP_FILE_NEW': '1'})
check('route_pl6_to_bg1', loads(ev)[-1:] == ['bg1'] and loads(ev).count('pl6') == 1 and rows[-1]['chr'] == 0, str(loads(ev)))

# ---- PL6's results (the boss path, .res_wipe) -> the front end; the save: the story block of stage 7, last stage 7, progress 8, the record
# rec[7][Lilac] (RE 954 / 955 / 907 / 908), the achievements row (13437 / 13436: 51 with ML.Z <= 480000 ms); Time Attack: the record only
with tempfile.TemporaryDirectory() as d:
    sv = os.path.join(d, 'story.bin')
    rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0', 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
    s = parse(sv); f = s and s['files'][2]
    st = [l for l in loads(ev) if l != 'gal']
    check('pl6_results_to_frontend', st[-2:] == ['pl6', 'frontend'], str(loads(ev)))
    check('pl6_story_block_stage7', f and f['used'] == 1 and f['last'] == PL and f['ch'] == 0 and f['stage'][PL][0] > 10000 and
          all(f['stage'][x][0] == 0 for x in range(STAGES) if x != PL), str(f and (f['last'], f['stage'][PL])))
    check('pl6_progress_8', s and s['progress'][0] == PL + 1, str(s and s['progress']))
    check('pl6_record_stage7', s and s['rec'][PL][0][0] == f['stage'][PL][0], str(s and s['rec'][PL]))
    check('pl6_achievement_row', 'ach results stage 7' in out and 'ach earn 51' in out, '')
    ta = os.path.join(d, 'ta.bin')
    rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0', 'FP_TA': '1', 'FP_SAVE': ta})
    s = parse(ta)
    check('pl6_ta_record_only', s and s['rec'][PL][0][0] > 10000 and all(fl['used'] == 0 for fl in s['files']), str(s and (s['rec'][PL][0], s['progress'])))
rows, ev, out = run('29000 RESBOSS\n', 29900, {'FP_LEVEL': 'pl6', 'FP_CHAR': '0'})   # 29000 frames = 483 s; the limit is 480 s
check('pl6_speedrunner_limit', 'ach results stage 7' in out and 'ach earn 51' not in out, '')

# ---- Time Attack (title cheat: every stage's progress): Lilac's list holds Pangu Lagoon (E 7) in slot 7 (RE 623) -> PL1 with its card;
# Carol's / Milla's slot 7 is Thermal Base (RE 624 / 625: E 6), slot 6 Trap Hideout: no slot leads them into Pangu Lagoon
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
DOWN7 = ''.join('%d %d D\n' % (f, f + 1) for f in (630, 650, 670, 690, 710, 730, 745))
for nm, sel, chn, want in (('lilac', '', 0, 'pl1'), ('carol', '470 471 R\n', 2, 'tb1'), ('milla', '470 471 R\n480 481 R\n', 4, 'tb1')):
    keys = '200 202 S\n360 361 D\n380 381 J\n' + sel + '490 491 J\n' + DOWN7 + '760 761 J\n880 881 J\n'
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run(CHEAT + keys, 1300, {'FP_SAVE': os.path.join(d, 's.bin')})
    ok = ('pl1' in loads(ev)) if want == 'pl1' else (want in loads(ev) and 'pl1' not in loads(ev))
    check('ta_%s_slot7_%s' % (nm, want), ok, str(loads(ev)))
    if want == 'pl1': check('ta_lilac_pl1_lilac', rows and rows[-1]['act'] == 1 and rows[-1]['chr'] == chn, '')

# ---- the loop pieces (RE 12747-12750 = 3031 / 3032 on Q10 / Q11): PLLayer01 / 02 (2048 / 2049) solid on loop layer 0, PLLayer11 / 12
# (2050 / 2051) on layer 1: flag 0 (not solid) follows MV.P; the off-map templates (PL2 / PL3 x < 0) are gone
_, _, o = run('', 2, {'FP_LEVEL': 'pl1', 'FP_CHAR': '0', 'FP_TRACK': '2048:-1,2049:-1,2050:-1,2051:-1'})
pcs = [l.split() for l in o.splitlines() if l.startswith('ob 1 ')]
fl = {int(p[2]): int(p[p.index('fl') + 1], 16) & 1 for p in pcs}
check('pl1_loop_pieces', len(pcs) == 9 and fl == {2048: 0, 2049: 0, 2050: 1, 2051: 1} and all(p[p.index('an') + 1] != '-1' for p in pcs), str((len(pcs), fl)))
_, _, o = run('', 2, {'FP_LEVEL': 'pl2', 'FP_CHAR': '0', 'FP_TRACK': '2048:-1,2049:-1,2050:-1,2051:-1'})
pcs = [l.split() for l in o.splitlines() if l.startswith('ob 1 ')]
check('pl2_loop_templates_gone', len(pcs) == 5 and all(float(p[5]) > 0 for p in pcs), str(len(pcs)))

# ---- data: the parallax set (D, E, G per piece, boxes) (RE create_levelparallaxpiece* + the start events / 12960 / 13104)
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, wm, ver = struct.unpack_from('<4H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh))
    return sorted(out)
A13 = [(894, .9, .92, -.1, 144, 320, 73), (2909, .94, .92, 0, 0, 400, 380), (2910, .93, .92, 0, 120, 320, 80), (2911, .91, .92, 0, 40, 480, 180),
       (2912, .88, .92, 0, 136, 480, 104), (2913, .85, .92, 0, 136, 480, 128)]
check('par_pl1', par('pl1') == sorted(A13 + [(895, .92, .88, -.3, 1032, 320, 140)]), str(par('pl1')))
check('par_pl3', par('pl3') == A13, str(par('pl3')))
A45 = [(3024, .8, 1, 0, 0, 160, 320), (3034, .7, .8, 0, 0, 256, 640), (3040, .6, .6, 0, 88, 128, 154), (3041, .4, .4, 0, 0, 256, 308)]
check('par_pl4', par('pl4') == A45, str(par('pl4')))
check('par_pl6', par('pl6') == [(3070, .8, 1, 0, 0, 160, 320)], str(par('pl6')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
