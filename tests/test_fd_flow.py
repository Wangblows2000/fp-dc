#!/usr/bin/env python3
"""Host tests for the Final Dreadnought stage flow (session 24; src/game/levels.c rows fd1a-fd4b, the Classic route, src/game/fd_flow.c,
game.c's exit hold, tools/import_frame.py FD_FLOW blocks, tools/import_ui.py's wide-strip crop; build/re/spec_fd_flow.md, RV traces
build/orig_traces/fd_flow/): every act loads for Lilac, Carol and Milla with its music (m_stage10 / 10b / 10c / 10d / 10e) and a safe start;
the round cards ("ROUND n" / "Final Dreadnought"); fd1a's entry (Lilac / Carol held at x -40, the hangar-wall explosions, the wall at D 160,
thrown in at D > 180; Milla dropped from (128, -60): RV fd1a_start_*); the transporter-ride ends of fd2a / fd3a; the act chain inside each
round (fd1a / fd3a right edge, fd2a right edge below y 1100, fd4a right edge + 100 frames of stopped world) and the walk-ins (RV fd1b /
fd2b / fd3b / fd4b_start: MV (61, Y) at g4 1); no edge exit in the b frames; the checkpoints and a restart; the rounds' results -> the next
round (I 10 -> 67, 11 -> 69, 12 -> 71) and round 4's -> the ending, for the three characters; the round saves (story block, last stage,
progress, the record; Time Attack: the record only); Time Attack slots 9-12 (rounds 1-4) and their preview strips (bg11-bg14); the FD3
loop-layer pieces (types 2560-2569); the parallax sets.
usage: test_fd_flow.py [data_dir]   (default: converted2; the session-24 worktrees build Final Dreadnought into cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, NCH = 15, 8, 4
CHARS = (('lilac', '0'), ('carol', '2'), ('milla', '4'))
SAVE_CH = {'0': 0, '2': 1, '4': 2}
REF = 14                                                 # ORIG_REF_DY (port y = MV.Y + 14)

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_CARD',
                                                     'FP_CARD_DBG', 'FP_INVULN')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('music', 'fdflow', 'save', 'sid', 'ach', 'ending') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def stages(ev): return [l for l in loads(ev) if l not in ('boot', 'frontend', 'gal')]
def music(ev): return [l.split()[1] for _, l in ev if l.startswith('music b')]
def fdf(ev, what): return [i for i, l in ev if l.startswith('fdflow ' + what)]
def sids(ev): return [int(l.split()[1]) for _, l in ev if l.startswith('sid ')]

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

ACTS = ('fd1a', 'fd1b', 'fd2a', 'fd2b', 'fd3a', 'fd3b', 'fd4a', 'fd4b')
# ---- every act loads for every character (no g1 write), plays slot 5 (RE loop wrappers: 5_65 m_stage10, 5_67 / 5_68 m_stage10b, 5_69 /
# 5_70 m_stage10c, fd4a 7_15 m_stage10d, 5_72 m_stage10e) and starts safely; dry (fd2b's air rule is its siren's vacuum, not ML.B)
MUS = {'fd1a': 'm_stage10', 'fd1b': 'm_stage10', 'fd2a': 'm_stage10b', 'fd2b': 'm_stage10b', 'fd3a': 'm_stage10c', 'fd3b': 'm_stage10c',
       'fd4a': 'm_stage10d', 'fd4b': 'm_stage10e'}
for lv in ACTS:
    for nm, ch in CHARS:
        if nm != 'lilac' and lv not in ('fd1a', 'fd2b', 'fd4a'): continue   # (time: the three characters on three acts)
        rows, ev, out = run('', 400, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        r = rows[-1]
        check('%s_loads_%s' % (lv, nm), r['chr'] == int(ch) and stages(ev) == [lv], str((loads(ev), r['chr'])))
        check('%s_music_%s' % (lv, nm), music(ev) == ['bgm/%s.adp' % MUS[lv]], str(music(ev)))
        check('%s_safe_start_%s' % (lv, nm), r['gnd'] == 1 and r['hp'] == 14 and r['st'] == 0 and r['g4'] == 1, 'x %s y %s gnd %s hp %s' % (r['x'], r['y'], r['gnd'], r['hp']))
        check('%s_dry_%s' % (lv, nm), all(x.get('air', 0) == 0 for x in rows), '')

# ---- the round cards (4307 in the a frames; 15314 / 15685 / 16205 / 16733: Intro2 frame 11-14 "ROUND n", Intro3 frame 8 "Final Dreadnought")
sizes = []
for lv in ('fd1a', 'fd2a', 'fd3a', 'fd4a'):
    rows, ev, out = run('', 20, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_CARD': '1', 'FP_CARD_DBG': '1'})
    c = [l for l in out.splitlines() if l.startswith('card ' + lv)]
    check('%s_card_final_dreadnought' % lv, c and ' stage 218x41 ' in c[0], str(c[:1]))
    sizes.append(c[0].split()[-1] if c else '')
check('fd_card_round_names_differ_from_bg', all(s and s != '165x35' for s in sizes), str(sizes))

# ---- fd1a's entry (handle_frame_66 card group, 15298-15301; RV fd1a_start_lilac / carol: MV (-40, 997) held, sounds 98 / 99 / 311 every 7
# frames, 354 at Sign.D 160 (19 frames before the throw), thrown at D 181 with B 10; Milla (g1 4) at (128, -60), falling when the card
# ends, landing at MV.Y 178.5 27 frames later)
for nm, ch in CHARS[:2]:
    rows, ev, out = run('', 260, {'FP_LEVEL': 'fd1a', 'FP_CHAR': ch})
    th = fdf(ev, 'thrown'); wl = fdf(ev, 'wall')
    held = [r for r in rows[:th[0] - 1]] if th else []
    check('fd1a_entry_held_%s' % nm, held and all(r['x'] == -40 and abs(r['y'] - REF - 997) < 0.5 for r in held), str(held[-1:] and (held[-1]['x'], held[-1]['y'])))
    check('fd1a_entry_thrown_%s' % nm, th and wl and th[0] - wl[0] == 21 and rows[th[0] + 4]['x'] >= -40 + 30 and rows[th[0] + 4]['tm'] < 8,
          str((th, wl)))
    s = sids(ev)
    check('fd1a_entry_sounds_%s' % nm, 354 in s and sum(1 for x in s if x in (98, 99, 311)) >= 20, str(s[:30]))
rows, ev, out = run('', 300, {'FP_LEVEL': 'fd1a', 'FP_CHAR': '4', 'FP_CARD': '1'})
dr = fdf(ev, 'milla drop')
check('fd1a_entry_milla_held', dr and all(r['x'] == 128 and abs(r['y'] - REF + 60) < 0.5 for r in rows[2:dr[0] - 1]), '')
land = [i for i, r in enumerate(rows) if i > (dr[0] if dr else 0) and r['gnd'] == 1]
check('fd1a_entry_milla_lands', land and rows[land[0]]['x'] == 128 and abs(rows[land[0]]['y'] - REF - 178.5) < 1 and 20 <= land[0] - dr[0] <= 34,
      str(land[:1] and (land[0] - dr[0], rows[land[0]]['y'] - REF)))

# ---- the transporter rides of fd2a / fd3a (RV fd2a / fd3a_start: MV held at -40, carried in after the card): the ride's end
for lv, (x, y) in (('fd2a', (320, 690)), ('fd3a', (96, 394))):
    for nm, ch in CHARS:
        rows, ev, out = run('', 60, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        r = rows[-1]
        check('%s_ride_end_%s' % (lv, nm), abs(r['x'] - x) < 2 and abs(r['y'] - REF - y) < 2 and r['gnd'] == 1, str((r['x'], r['y'] - REF)))

# ---- the act chain inside the rounds and the walk-ins (4526: x -32, B 3; g4 1 after 30 frames)
WALK = {'fd1b': (61, 690), 'fd2b': (61, 4946), 'fd3b': (61, 658.5), 'fd4b': (61, 626)}
EDGE = {'fd1a': (16850, 1200), 'fd2a': (6000, 1400), 'fd3a': (14250, 250), 'fd4a': (5450, 150)}
for lv, nx in (('fd1a', 'fd1b'), ('fd2a', 'fd2b'), ('fd3a', 'fd3b'), ('fd4a', 'fd4b')):
    for nm, ch in CHARS:
        rows, ev, out = run('3 SETV others E 99999\n5 TP %d %d\n60 400 R\n' % EDGE[lv], 400, {'FP_LEVEL': lv, 'FP_CHAR': ch})
        go = [r for r in rows if r['g4'] == -1]
        check('%s_exit_to_%s_%s' % (lv, nx, nm), stages(ev) == [lv, nx] and go and go[0]['x'] == -32 and go[0]['chr'] == int(ch), str(stages(ev)))
        k = rows.index(go[0]) if go else 0
        g1 = [r for r in rows[k:] if r['g4'] == 1]
        check('%s_walkin_ends_%s' % (nx, nm), g1 and 30 <= g1[0]['f'] - go[0]['f'] <= 36 and abs(g1[0]['y'] - REF - WALK[nx][1]) < 2.5 and
              abs(g1[0]['x'] - WALK[nx][0]) < 2.5 and g1[0]['gnd'] == 1, str(g1[:1] and (g1[0]['f'] - go[0]['f'], g1[0]['x'], g1[0]['y'] - REF)))
        if lv == 'fd4a':   # 16721 / 16723: the world stands 100 frames (timer stopped, player still) before the fade
            ex = [r for r in rows if r['x'] > 5534][:1]
            i0 = rows.index(ex[0]) if ex else 0
            st = rows[i0 + 5:i0 + 95]
            check('fd4a_exit_hold_%s' % nm, st and all(r['x'] == st[0]['x'] and r['tm'] == st[0]['tm'] for r in st), '')
rows, ev, out = run('3 SETV others E 99999\n5 TP 6000 200\n60 400 R\n', 400, {'FP_LEVEL': 'fd2a', 'FP_CHAR': '0'})
check('fd2a_no_exit_above_1100', stages(ev) == ['fd2a'] and rows[-1]['x'] > 6090, str((stages(ev), rows[-1]['x'])))
for lv, x, y in (('fd1b', 16700, 600), ('fd2b', 8300, 4900), ('fd3b', 14100, 600), ('fd4b', 3200, 600)):
    rows, ev, out = run('3 SETV others E 99999\n5 TP %d %d\n6 300 R\n' % (x, y), 300, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_no_edge_exit' % lv, set(stages(ev)) == {lv}, str(stages(ev)))   # (a fall restarts the act)

# ---- checkpoints (RE inline handle_frame_66 / 68 / 70 / 72, evaluated in order in one frame: fd4a (2400, 2900) passes g4 2 and lands on
# the last zone's 5) and a restart there
for lv, tp, g4 in (('fd1a', (4900, 860), 2), ('fd1a', (8800, 1500), 3), ('fd1a', (8800, 1300), 1), ('fd1a', (14150, 1600), 4),
                   ('fd2a', (3000, 1380), 2), ('fd2a', (4000, 2000), 3), ('fd3a', (3900, 1080), 2), ('fd3a', (7600, 1060), 3),
                   ('fd3a', (10100, 470), 4), ('fd4a', (2400, 2900), 5), ('fd4a', (2400, 6300), 4)):
    rows, ev, out = run('3 SETV others E 99999\n5 TP %d %d\n' % tp, 12, {'FP_LEVEL': lv, 'FP_CHAR': '0'})
    check('%s_checkpoint_%d_%d' % (lv, tp[0], tp[1]), rows[-1]['g4'] == g4, 'g4 %s' % rows[-1]['g4'])
for nm, ch in CHARS:
    rows, ev, out = run('5 TP 4900 860\n20 HP 1\n22 HURT 99\n', 400, {'FP_LEVEL': 'fd1a', 'FP_CHAR': ch})
    dead = [i for i, r in enumerate(rows) if r['st'] == 6]
    rs = rows[dead[-1] + 1:] if dead else []
    check('fd1a_restart_at_checkpoint_%s' % nm, stages(ev) == ['fd1a', 'fd1a'] and rs and rs[0]['x'] == 4856 and rs[0]['y'] == 880 + REF and rs[0]['g4'] == 2,
          str(rs[:1] and (rs[0]['x'], rs[0]['y'], rs[0]['g4'])))

# ---- the rounds' results (handle_frame_67 / 69 / 71: 4560-4586, 5394, the wipe 8547-8549 -> frame 8 -> I 10 -> 67, 11 -> 69, 12 -> 71)
# and round 4's (handle_frame_73: no wipe; the final boss's script ends the results) -> the ending (s 13 -> frame 83)
CONFIRM = ''.join('%d %d J\n' % (f, f + 2) for f in range(760, 1000, 40))
for lv, nx in (('fd1b', 'fd2a'), ('fd2b', 'fd3a'), ('fd3b', 'fd4a')):
    for nm, ch in CHARS:
        with tempfile.TemporaryDirectory() as d:
            rows, ev, out = run('700 RESBOSS\n1200 1202 J\n', 2000, {'FP_LEVEL': lv, 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin'), 'FP_FILE': '2', 'FP_FILE_NEW': '1'})
        check('route_%s_to_%s_%s' % (lv, nx, nm), stages(ev) == [lv, nx] and rows[-1]['chr'] == int(ch), str((stages(ev), rows[-1]['chr'])))
for nm, ch in CHARS:
    with tempfile.TemporaryDirectory() as d:
        rows, ev, out = run('30 RESBOSS\n700 RESEND\n' + CONFIRM, 1300, {'FP_LEVEL': 'fd4b', 'FP_CHAR': ch, 'FP_SAVE': os.path.join(d, 's.bin'), 'FP_FILE': '2', 'FP_FILE_NEW': '1'})
    check('fd4b_results_to_ending_%s' % nm, 'ending begin chr %s' % ch in out and [x for x in stages(ev) if x != 'end'] == ['fd4b'], str(stages(ev)))

# ---- the round saves (RE 954 / 955 / 907 / 908: block x3 y(6+5s) for s = 10-13, last stage s, progress s + 1, the record rec[s][char]);
# Time Attack: the record only
with tempfile.TemporaryDirectory() as d:
    for (lv, s), (nm, ch) in zip((('fd1b', 10), ('fd2b', 11), ('fd3b', 12)), CHARS):
        c = SAVE_CH[ch]
        sv = os.path.join(d, 'story_%s.bin' % nm)
        rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': lv, 'FP_CHAR': ch, 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        p = parse(sv); f = p and p['files'][2]
        check('%s_story_block_stage%d_%s' % (lv, s, nm), f and f['used'] == 1 and f['last'] == s and f['stage'][s][0] > 10000 and
              all(f['stage'][x][0] == 0 for x in range(STAGES) if x != s), str(f and (f['last'], f['stage'][s])))
        check('%s_progress_%d_%s' % (lv, s + 1, nm), p and p['progress'][c] == s + 1, str(p and p['progress']))
        check('%s_record_stage%d_%s' % (lv, s, nm), p and p['rec'][s][c][0] == f['stage'][s][0], str(p and p['rec'][s]))
        check('%s_achievement_row_%s' % (lv, nm), 'ach results stage %d' % s in out, '')
    ta = os.path.join(d, 'ta.bin')
    rows, ev, out = run('700 RESBOSS\n', 1600, {'FP_LEVEL': 'fd1b', 'FP_CHAR': '0', 'FP_TA': '1', 'FP_SAVE': ta})
    p = parse(ta)
    check('fd1b_ta_record_only', p and p['rec'][10][0][0] > 10000 and all(fl['used'] == 0 for fl in p['files']) and 'fd2a' not in loads(ev),
          str(p and (p['rec'][10][0], p['progress'])))

# ---- Time Attack (title cheat: every stage's progress): slots 9-12 hold the rounds (E 10-13, RE 623-631) for every character -> the round's
# first act; the preview strips of E 10-13 = bg11layer1-4 / bg12layer1-4 / bg13layer1-2 / bg14layer1 (frame 6, y 536 / 584 / 632 / 680) are
# in the screen's data (bg13layer1, 2048 px wide, keeps its left 1024 px: the 272-px window shows only that)
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
for slot, want in ((9, 'fd1a'), (10, 'fd2a'), (11, 'fd3a'), (12, 'fd4a')):
    for nm, sel, chn in (('lilac', '', 0), ('carol', '470 471 R\n', 2), ('milla', '470 471 R\n480 481 R\n', 4)):
        if slot in (10, 11) and nm != 'lilac': continue    # (time)
        down = ''.join('%d %d D\n' % (f, f + 1) for f in range(630, 630 + 15 * slot, 15))
        keys = '200 202 S\n360 361 D\n380 381 J\n' + sel + '490 491 J\n' + down + '%d %d J\n%d %d J\n' % (630 + 15 * slot + 25, 630 + 15 * slot + 26, 630 + 15 * slot + 145, 630 + 15 * slot + 146)
        with tempfile.TemporaryDirectory() as d:
            rows, ev, out = run(CHEAT + keys, 1400, {'FP_SAVE': os.path.join(d, 's.bin')})
        check('ta_%s_slot%d_%s' % (nm, slot, want), want in loads(ev) and rows and rows[-1]['chr'] == chn, str(loads(ev)))
ui = open(os.path.join(DATA, 'ui', 'tasel_6.ui'), 'rb').read()
check('ta_preview_strips_fd', all(b'bg%dlayer%d' % (s, k) in ui for s, n in ((11, 4), (12, 4), (13, 2), (14, 1)) for k in range(1, n + 1)), '')

# ---- the FD3 loop-layer pieces (RE on_frame_70_init: Q10 = 01-05 solid on loop layer 0, Q11 = 11-15 on loop layer 1; 3031 / 3032 by MV.P):
# json layer 3 -> the play layer 2, json 4 -> 3 (in front); fd3b places many beyond its 14208-px width (removed); at the start (MV.P 0) the layer-1 pieces are off (flag 0)
for lv, want in (('fd3a', {2560: 2, 2561: 27, 2566: 41}), ('fd3b', {2560: 0, 2561: 9, 2566: 8})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_CHAR': '0', 'FP_TRACK': ','.join('%d:-1' % t for t in range(2560, 2570))})
    pcs = [l.split() for l in o.splitlines() if l.startswith('ob 1 ')]
    cnt = {t: sum(1 for p in pcs if int(p[2]) == t) for t in want}
    fl = {(int(p[2]) >= 2565, int(p[p.index('fl') + 1], 16) & 1) for p in pcs}
    lay = {int(p[p.index('lay') + 1]) for p in pcs}
    check('%s_loop_pieces' % lv, cnt == want and fl == {(False, 0), (True, 1)} and lay == {2, 3}, str((cnt, fl, lay)))

# ---- data: the parallax sets (D, E, G per piece, boxes) (RE create_levelparallaxpiece* + 15309 / the start inlines / 16200 / 16728)
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
F1 = [(3390, .9, .9, 0, 96, 960, 248), (3391, .8, .88, 0, 360, 400, 40), (3393, .7, .9, 0, 0, 400, 96), (3394, .7, .86, 0, 416, 400, 128),
      (3395, .95, .95, 0, 72, 400, 238), (3413, 1, 1, -.3, 0, 400, 238)]
check('par_fd1a', par('fd1a') == F1, str(par('fd1a')))
check('par_fd1b', par('fd1b') == sorted(F1 + [(3463, .5, 0, 0, 1904, 64, 240), (3464, .25, 0, 0, 1904, 64, 240)]), str(par('fd1b')))
check('par_fd2a', par('fd2a') == [(3496, .3, .3, 0, 0, 480, 1920), (3497, .6, .6, 0, 0, 240, 1440), (3558, .45, .45, 0, 0, 360, 1600),
                                  (3559, .6, .6, 0, 0, 240, 1440), (3560, .6, .6, 0, 0, 240, 1440)], str(par('fd2a')))
check('par_fd2b', [p[:4] for p in par('fd2b')] == [(3224, 1, 1, 0), (3565, .3, .3, 0), (3566, .6, .6, 0), (3569, .45, .45, 0), (3570, .6, .6, 0),
                                                   (3571, .6, .6, 0), (3590, 1, 1, -1)], str(par('fd2b')))
F3 = [(3608, .99, .94, 0, 8, 2048, 320), (3609, .8, .8, 0, 0, 584, 160), (3611, .8, .8, 0, 256, 584, 192), (3612, .7, .7, 0, 432, 128, 128),
      (3613, .6, .65, 0, 552, 128, 320)]
check('par_fd3a', par('fd3a') == F3, str(par('fd3a')))
check('par_fd3b', par('fd3b') == F3, str(par('fd3b')))
check('par_fd4a', par('fd4a') == [(3725, 1, .98, 0, 0, 426, 380)], str(par('fd4a')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
