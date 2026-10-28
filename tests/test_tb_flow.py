#!/usr/bin/env python3
"""Host tests for the Thermal Base stage flow (session 21c; src/game/levels.c rows tb1-tb6 and the Classic route, src/game/tb_flow.c,
tools/import_frame.py TB_FLOW blocks; build/re/spec_tb_flow.md, RV traces build/orig_traces/tb_flow/):
every act loads for Lilac, Carol and Milla with its music (m_stage6 / m_stage6b, TB6 silent) and the player starts safely (grounded,
unhurt; RV start heights); TB1's start lift (RE 11881 / 11709-11719; RV tb1_start_lilac / _carol / _milla: standing on it at MV.Y
2386.5, rising 8 px a frame from frame 162, at the top 2040 with the player at MV.Y 2026.5, sound 49); the other lifts (a lift on its
top marker starts there and sinks once when stood on; TB5's lifts without markers stay still); the checkpoints (inline / 12096 /
12539-12541, TB5's last one re-arms at g4 4) and a restart; the act chain TB1 -> ... -> TB6 by the right edge and the walk-ins; TB6
without an edge exit; water in TB4 / TB5 only; cards 61-70; the Classic route (JC5 results -> TB1 for Lilac only, TH3 results -> TB1
for Carol / Milla, TB6 results -> the front end); the stage-6 save (story block, last stage, progress, TA record), the achievements
row (49 under 540 s); Time Attack (Lilac's slot 6, Carol's slot 7 -> TB1); the parallax set and the wall lights.
usage: test_tb_flow.py [data_dir]   (default: converted2; the session-21c worktrees build Thermal Base into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (('lilac', '0'), ('carol', '2'), ('milla', '4'))
STAGES, FILES, NCH = 15, 8, 4
TB = 6                                                   # stage index of Thermal Base
SAVE_CH = {'0': 0, '2': 1, '4': 2}

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD', 'FP_CARD_DBG')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('music', 'tbflow', 'save', 'sid', 'ach') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def tbf(ev, what): return [i for i, l in ev if l.startswith('tbflow ' + what)]
def sids(ev, sid): return [i for i, l in ev if l == 'sid %d' % sid]

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

# ---- every act loads for Lilac, Carol and Milla: act, character, the music (RE loop wrappers: slot 5 m_stage6 TB1-TB3, m_stage6b TB4 /
# TB5; TB6's start stops channel 31, 12735) and a safe start (debug start = g4 1: the start point + 16; RV start MV.Y + 14 at frame 0 of
# the walk-in acts: 2240 / 1408 / 2600 / 2088 / 736; TB1 ends on its lift at the top: 2026.5 + 14)
SAFE_Y = {'tb1': 2040.5, 'tb2': 2240, 'tb3': 1408, 'tb4': 2600, 'tb5': 2088, 'tb6': 736}
MUS = {'tb1': ['bgm/m_stage6.adp'], 'tb2': ['bgm/m_stage6.adp'], 'tb3': ['bgm/m_stage6.adp'], 'tb4': ['bgm/m_stage6b.adp'],
       'tb5': ['bgm/m_stage6b.adp'], 'tb6': []}
for nm, ch in CHARS:
    for act, lv in enumerate(('tb1', 'tb2', 'tb3', 'tb4', 'tb5', 'tb6'), 1):
        rows, ev, out = run('', 240, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        check('%s_loads_%s' % (lv, nm), rows and rows[-1]['act'] == act and rows[-1]['chr'] == int(ch) and loads(ev)[-1:] == [lv], str(loads(ev)))
        check('%s_music_%s' % (lv, nm), music(ev) == MUS[lv], str(music(ev)))
        r = rows[-1]
        check('%s_safe_start_%s' % (lv, nm), r['gnd'] == 1 and r['hp'] == 14 and r['st'] == 0 and abs(r['y'] - SAFE_Y[lv]) <= 0.5,
              'y %s gnd %s hp %s' % (r['y'], r['gnd'], r['hp']))
rows, ev, out = run('3 SETV others E 99999\n5 TP 14950 2300\n40 120 R\n300 1200 R\n', 1000, {'FP_LEVEL': 'tb5', 'FP_CHAR': '0'})   # TB5 -> TB6 (the boss module's end elevator, then right): the stage music stops at TB6's start
mi = [l for _, l in ev if l.startswith('music')]
check('tb6_silent_start', 'tb6' in loads(ev) and mi[-1:] and mi[-1].startswith('music stop'), str(mi[-3:]))

# ---- TB1's start lift with the act-start card (RV tb1_start_*: MV.Y 2386.5 on the lift until frame 161, Q 1 on 161 (11881), MV.Y 2378.5 on
# 162, then 8 px a frame (the original carries on the lift's lagging N: the port is a frame ahead, INFERRED), the top 2040 / MV.Y 2026.5
# on 206-207, sound 49 at the stop (RV .snd: frame 205))
for nm, ch in CHARS:
    rows, ev, out = run('', 260, {'FP_LEVEL': 'tb1', 'FP_CHAR': ch, 'FP_CARD': '1'})
    my = [r['y'] - 14 for r in rows]
    check('tb1_card_stand_on_lift_%s' % nm, all(abs(y - 2386.5) < 0.01 for y in my[30:162]) and all(r['gnd'] == 1 for r in rows[30:162]),
          str([(i, y) for i, y in enumerate(my[30:162], 30) if abs(y - 2386.5) >= 0.01][:3]))
    check('tb1_card_lift_starts_162_%s' % nm, abs(my[162] - 2378.5) < 0.01 and abs(my[161] - 2386.5) < 0.01 and tbf(ev, 'lift 1 card start'), str(my[160:164]))
    top = [i for i, y in enumerate(my) if abs(y - 2026.5) < 0.01]
    check('tb1_card_lift_top_%s' % nm, top and 206 <= top[0] <= 207 and all(abs(y - 2026.5) < 0.01 and r['gnd'] == 1 for y, r in zip(my[210:], rows[210:])),
          str(top[:1]))
    s49 = sids(ev, 49)
    check('tb1_card_lift_sound49_%s' % nm, len(s49) == 1 and 204 <= s49[0] <= 207 and tbf(ev, 'lift 1 stop 2'), str(s49))
rows, ev, out = run('', 200, {'FP_LEVEL': 'tb1', 'FP_CHAR': '2'})      # debug start, no card: the rider rule (11710) starts it on landing
check('tb1_no_card_rider_start', tbf(ev, 'lift 1 go 1') and abs(rows[-1]['y'] - 14 - 2026.5) < 0.01, str(rows[-1]['y']))

# ---- the other lifts: TB1's second / third and TB3's start on their top markers (12030 / the start inline: Q 2) and sink once when
# stood on (11713: Q 3, 8 px a frame, 11719: at the bottom marker Q 0, sound 49); stepping off and on again moves nothing (flag 6 is
# never cleared); TB2's starts at the bottom; TB5's three lifts (no markers) belong to the boss module's end elevator: still
for lv, x, y, bot in (('tb1', 4752, 1900, 2264), ('tb1', 6720, 1140, 2040), ('tb3', 2160, 2460, 3456)):
    rows, ev, out = run('5 TP %d %d\n' % (x, y), 260, {'FP_LEVEL': lv, 'FP_CHAR': '4', 'FP_DBG_TBLIFT': '1'})
    q0 = [l.split() for l in out.splitlines() if l.startswith('tblift 1 ')]
    k = [int(q[2]) for q in q0 if abs(float(q[3]) - x) < 1][:1]
    st = [int(l.split()[5]) for l in out.splitlines() if l.startswith('tblift ') and k and l.split()[2] == str(k[0])]
    check('%s_lift_%d_starts_top' % (lv, x), st[:1] == [2], str(st[:1]))
    check('%s_lift_%d_sinks_to_%d' % (lv, x, bot), st[-1:] == [0] and 3 in st and abs(rows[-1]['y'] - 14 - (bot - 13.5)) < 1 and rows[-1]['gnd'] == 1 and sids(ev, 49),
          'y %s st %s' % (rows[-1]['y'], st[-1:]))
rows, ev, out = run('5 TP 4752 1900\n100 120 L\n140 160 R\n', 260, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0', 'FP_DBG_TBLIFT': '1'})
check('tb1_lift_rides_once', len(tbf(ev, 'lift 0 go')) == 1, str([l for _, l in ev if l.startswith('tbflow')]))
rows, ev, out = run('', 3, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0', 'FP_DBG_TBLIFT': '1'})
check('tb2_lift_starts_bottom', [l.split()[5] for l in out.splitlines() if l.startswith('tblift 1 0 ')] == ['0'], '')
rows, ev, out = run('5 TP 15136 2300\n', 120, {'FP_LEVEL': 'tb5', 'FP_CHAR': '2', 'FP_DBG_TBLIFT': '1'})
check('tb5_lifts_not_flow', not tbf(ev, 'lift'), '')   # TB5's markerless lifts are the boss module's end elevator (test_tb_bosses)

# ---- checkpoints (RE: TB1 inline (2, 6932, 2026) / (3, 9914, 1374); TB2 inline (2, 2120, 2222) MV.Y < 2260 / (3, 6144, 3184) MV.Y > 2900;
# TB3 12096 (2, 3333, 3440) MV.Y > 3200; TB4 inline (2, 2664, 2648) / (3, 2912, 2648) MV.Y > 2500; TB5 12539-12541 (2, 1855, 3096) MV.Y >
# 3260 / (3, 6880, 2774) / (4, 10400, 2712), inline g4 < 5 -> (4, 14720, 2328))
for lv, tp, g4 in (('tb1', '5 TP 7000 1990\n', 2), ('tb1', '5 TP 9914 1360\n', 3), ('tb1', '5 TP 7300 1990\n', 1),
                   ('tb2', '5 TP 2120 2200\n', 2), ('tb2', '5 TP 2300 2500\n', 1), ('tb2', '5 TP 6144 3170\n', 3),
                   ('tb3', '5 TP 3150 3420\n', 2), ('tb3', '5 TP 3150 3100\n', 1), ('tb4', '5 TP 2550 2630\n', 2),
                   ('tb4', '5 TP 2850 2630\n', 3), ('tb5', '5 TP 1855 3280\n', 2), ('tb5', '5 TP 6640 2760\n', 3),
                   ('tb5', '5 TP 10400 2700\n', 4)):
    rows, ev, out = run(tp, 12, {'FP_LEVEL': lv, 'FP_CHAR': '4'})
    check('%s_checkpoint_%s_%s' % (lv, tp.split()[2], tp.split()[3]), rows[-1]['g4'] == g4, 'g4 %s' % rows[-1]['g4'])
rows, ev, out = run('5 TP 7000 1990\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'tb1', 'FP_CHAR': '2'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('tb1_restart_at_checkpoint', loads(ev).count('tb1') == 2 and rs and rs[0]['x'] == 6932 and rs[0]['y'] == 2026 + 14 and rs[0]['g4'] == 2,
      str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))
rows, ev, out = run('5 TP 10400 2700\n15 TP 14720 2300\n30 HP 1\n32 HURT 99\n', 420, {'FP_LEVEL': 'tb5', 'FP_CHAR': '0'})
dead = [i for i, r in enumerate(rows) if r['st'] == 6]
rs = rows[dead[-1] + 1:] if dead else []
check('tb5_last_checkpoint_rearms_g4_4', rs and rs[0]['x'] == 14720 and rs[0]['y'] == 2328 + 14 and rs[0]['g4'] == 4,
      str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))

# ---- the act chain by the right edge (4135 / 12089: X > W - 50, B > 0 -> 3578) and the walk-ins (4526: x -32, B 3; g4 1 after 30 frames)
for lv, x, y, nx, act in (('tb1', 10460, 2150, 'tb2', 2), ('tb2', 7250, 1400, 'tb3', 3), ('tb3', 7900, 2000, 'tb4', 4),
                          ('tb4', 7900, 2000, 'tb5', 5), ('tb5', 15950, 1300, 'tb6', 6)):
    for nm, ch in CHARS:
        if lv == 'tb5': rows, ev, out = run('3 SETV others E 99999\n5 TP 14950 2300\n40 120 R\n300 1200 R\n', 1000, {'FP_LEVEL': lv, 'FP_CHAR': ch})   # the end elevator
        else: rows, ev, out = run('5 TP %d %d\n6 400 R\n' % (x, y), 400, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        go = [r for r in rows if r['act'] == act]
        check('%s_exit_to_%s_%s' % (lv, nx, nm), nx in loads(ev) and go and go[0]['g4'] == -1 and go[0]['x'] == -32,
              str(loads(ev)) + ' ' + str(go[:1] and (go[0]['x'], go[0]['y'], go[0]['g4'])))
        g1 = [r for r in go if r['g4'] == 1]
        check('%s_walkin_ends_%s' % (nx, nm), g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36 and g1[0]['y'] == SAFE_Y[nx], str(g1[:1] and (g1[0]['f'] - go[0]['f'], g1[0]['y'])))
rows, ev, out = run('5 TP 1600 1000\n6 300 R\n', 300, {'FP_LEVEL': 'tb6', 'FP_CHAR': '2'})
check('tb6_no_edge_exit', all(r['act'] == 6 for r in rows) and loads(ev).count('tb6') == 1, str(loads(ev)))

# ---- water: TB4 ML.B 3400, TB5 2920 (MasterLevel A 1; RV tb4_start / tb5_start ml_A 1, ml_B 3400 / 2920); TB1-TB3 A 0; TB6 dry in effect
for lv, x, y, wet in (('tb4', 4400, 3500, 1), ('tb5', 3200, 3000, 1), ('tb2', 4400, 3500, 0), ('tb6', 800, 1800, 0)):
    rows, ev, out = run('5 TP %d %d\n' % (x, y), 40, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_water_%d' % (lv, wet), (rows[-1].get('air', 0) != 0) == bool(wet), 'air %s' % rows[-1].get('air'))

# ---- cards (ObjectCodec A newest first from the start events' counters 61 / 62 / 63 / 65 / 67; the template takes the next number)
for lv, want in (('tb1', {61}), ('tb2', {62}), ('tb3', {63, 64}), ('tb4', {65, 66}), ('tb5', {67, 68, 69, 70})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '2', 'FP_TRACK': '41:-1'})
    c = {int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c == want, str(c))

# ---- the Classic route (RE handle_frame_9 inline, Classic block): JC5's results (MenuSequence.I 5) -> frame 44 = TB1 for g1 = 0 (Lilac),
# frame 56 = TH1 for Carol / Milla; TH3's results (I 8) -> frame 44 = TB1 for g1 != 0; TB6's results (I 6) -> frame 50 (Pangu Lagoon,
# Lilac) / 84 (Carol / Milla): not ported, the front end
JPRESS = ''.join('%d %d J\n' % (f, f + 2) for f in range(560, 1100, 40))
for nm, ch, want in (('lilac', '0', 'tb1'), ('carol', '2', 'th1'), ('milla', '4', 'th1')):
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('60 RESBOSS\n' + JPRESS, 1500, {'FP_LEVEL': 'jc5', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin')})
    st = [l for l in loads(ev) if l not in ('gal', 'frontend')]
    ok = st[-2:] == ['jc5', want] and rows[-1]['act'] == 1 and (want != 'tb1' or music(ev)[-1:] == ['bgm/m_stage6.adp'])
    check('route_jc5_to_%s_%s' % (want, nm), ok and (want == 'tb1' or 'tb1' not in st), str((st, music(ev)[-2:])))
for nm, ch in CHARS[1:]:
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('700 RESBOSS\n' + ''.join('%d %d J\n' % (f, f + 2) for f in range(1200, 1800, 40)), 2000,
                            {'FP_LEVEL': 'th3', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin')})
    st = [l for l in loads(ev) if l not in ('gal', 'frontend')]
    check('route_th3_to_tb1_%s' % nm, st[-2:] == ['th3', 'tb1'] and rows[-1]['act'] == 1 and rows[-1]['chr'] == int(ch), str(st))

# ---- TB6's results (the boss path, .res_wipe) -> the front end; the save: the story block of stage 6, last stage 6, progress 7, the record
# rec[6][char] (RE 954 / 955 / 907 / 908), the achievements row (12717 / handle_frame_50 inline: 49 with ML.Z <= 540000 ms)
with tempfile.TemporaryDirectory() as d:
    for nm, ch in CHARS:
        c = SAVE_CH[ch]
        sv = os.path.join(d, 'story_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'tb6', 'FP_CHAR': ch, 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        s = parse(sv); f = s and s['files'][2]
        st = [l for l in loads(ev) if l != 'gal']
        check('tb6_results_to_frontend_%s' % nm, st[-2:] == ['tb6', 'frontend'], str(loads(ev)))
        check('tb6_story_block_stage6_%s' % nm, f and f['used'] == 1 and f['last'] == TB and f['stage'][TB][0] > 10000 and
              all(f['stage'][x][0] == 0 for x in range(STAGES) if x != TB), str(f and (f['last'], f['stage'][TB])))
        check('tb6_progress_7_%s' % nm, s and s['progress'][c] == TB + 1, str(s and s['progress']))
        check('tb6_record_stage6_%s' % nm, s and s['rec'][TB][c][0] == f['stage'][TB][0], str(s and s['rec'][TB]))
        check('tb6_achievement_row_%s' % nm, 'ach results stage 6' in out and 'ach earn 49' in out, '')
        ta = os.path.join(d, 'ta_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'tb6', 'FP_CHAR': ch, 'FP_TA': '1', 'FP_SAVE': ta})
        s = parse(ta)
        check('tb6_ta_record_only_%s' % nm, s and s['rec'][TB][c][0] > 10000 and all(fl['used'] == 0 for fl in s['files']),
              str(s and (s['rec'][TB][c], s['progress'])))
rows, ev, out = run('9000 RESBOSS\n', 9900, {'FP_LEVEL': 'tb6', 'FP_CHAR': '0'})   # 9000 frames = 150 s; the limit is 540 s
check('tb6_speedrunner_limit', 'ach results stage 6' in out, '')

# ---- Time Attack (title cheat: every stage's progress): Lilac's list keeps Thermal Base (s 6) in slot 6 (RE 623), Carol's moves it to
# slot 7 (RE 624 / 625); both start TB1 (the card and the start lift)
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
TAL = '200 202 S\n360 361 D\n380 381 J\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n690 691 D\n710 711 D\n730 731 D\n760 761 J\n880 881 J\n'
TAC = '200 202 S\n360 361 D\n380 381 J\n470 471 R\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n690 691 D\n710 711 D\n730 731 D\n745 746 D\n760 761 J\n880 881 J\n'
for nm, keys, chn in (('lilac', TAL, 0), ('carol', TAC, 2)):
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run(CHEAT + keys, 1300, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_%s_tb1' % nm, 'tb1' in loads(ev) and rows[-1]['act'] == 1 and rows[-1]['chr'] == chn and tbf(ev, 'lift 1 card start'), str(loads(ev)))

# ---- scenery: TBWallLightAnim (TB1 12, TB2 7): frame (g3 / 4) mod 12 (11826 / 12091; row 39 = g3 40)
for lv, n in (('tb1', 12), ('tb2', 7)):
    _, ev, out = run('', 40, {'FP_LEVEL': lv, 'FP_CHAR': '2', 'FP_TRACK': '1795:-1'})
    wl = [l.split() for l in out.splitlines() if l.startswith('ob ') and l.split()[2] == '1795']
    last = [w for w in wl if w[1] == '39']
    check('%s_wall_lights' % lv, len(last) == n and all(int(w[w.index('fr') + 1]) == (40 // 4) % 12 for w in last), str((len(last), last[:1] and last[0][last[0].index('fr') + 1])))

# ---- data: the parallax set (D, E, G per piece, boxes), the layer-1 scroll 0.8 (TB4 1.0)
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, wm, ver = struct.unpack_from('<4H', dd, 4); off = 12
    for _ in range(ni):
        n = struct.unpack_from('<3H', dd, off)[2]; off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, bw, bh))
    return out
A13 = [(2589, .99, .94, 0, 0, 320, 256), (2590, .98, .93, 0, 128, 480, 160), (2591, .95, .91, 0, 288, 480, 96), (2592, .84, .8, 0, 512, 320, 160),
       (2593, .5, .6, 0, 696, 384, 416)]
check('par_tb1', par('tb1') == A13, str(par('tb1')))
check('par_tb3', par('tb3') == A13 + [(2725, .5, .6, 0, 1112, 512, 640)], str(par('tb3')))
check('par_tb4', par('tb4') == [(2783, .7, .7, -4, 0, 64, 1280), (2784, .7, .7, 0, 0, 160, 1280), (2851, .5, .5, 0, 0, 512, 1920)], str(par('tb4')))
check('par_tb6', par('tb6') == [(2783, .7, .7, -4, 0, 64, 1280), (2885, .7, .7, 0, 0, 160, 1280)], str(par('tb6')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
