#!/usr/bin/env python3
"""Host tests for the Sky Battalion stage flow (session 19; src/game/levels.c rows sb1-sb4, src/game/sb_flow.c, game.c,
parallax.c; build/re/spec_sb_flow.md): the hub (SB1 = Zao's airship, json 36): fresh start with the card, the three launch
pads (SwitchPlatform sinks, fade, the ship with g4 -1), a done ship's pad staying down, the ships' opening (Zao's airship
fires the planks, the hover platforms are released, control and the timer return), the ships' right-edge exit back to SB1
(flown in from the left at B 15), checkpoints, the boss camera lock, the night / Kujacker hand-off, the results in SB1 with
the wipe to the front end, the Classic route FN6 -> SB1, the Time Attack entry, music slots, card ids, sounds and parallax.
usage: test_sb_flow.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

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
        elif t[0] in ('sid', 'music', 'voice', 'sbflow') or t[0].startswith('load['):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

def loads(ev): return [l.split('[', 1)[1].split(']', 1)[0] for _, l in ev if l.startswith('load[')]
def at(rows, f): return next((r for r in rows if r['f'] == f), None)
def sbf(ev, what): return [i for i, l in ev if l.startswith('sbflow ' + what)]

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- SB1 fresh start (g4 0 -> 1, 5839): start point (949, 1189) + 16 with the card, m_stage4 (loop_play_music_5_35), the
# stage sound group, the card images (name frame 4, STAGE 4 = frame 3)
rows, ev, out = run('', 30, {'FP_LEVEL': 'sb1', 'FP_CARD_DBG': '1'})
check('sb1_fresh_start', rows[0]['x'] == 965 and rows[-1]['g4'] == 1 and rows[-1]['act'] == 1, 'x %s g4 %s' % (rows[0]['x'], rows[-1]['g4']))
check('sb1_music_stage4', [l for _, l in ev if l.startswith('music')][:1] == ['music bgm/m_stage4.adp @0'])
check('sb1_stage_sounds', 'stage sounds 11' in out)   # SFX_SB1: 10 flow / Kujacker ids + 99 (the Kujacker's end, sb_boss session 19)
card = [l for l in out.splitlines() if l.startswith('card sb1')]
check('sb1_card_images', card and 'stage 0x0' not in card[0] and 'name 0x0' not in card[0], str(card))

# ---- the launch pads (8866-8878): standing on a pad sinks it 0.5 a frame to 24 (sounds 62, 49), the fade, the ship (g4 -1,
# x -32); pad C 0 = the newest json instance (1704, 1152) -> SB2, C 1 (872, 1024) -> SB3, C 2 (2448, 1152) -> SB4
for tp, lv, act in (('5 TP 1704 1120', 'sb2', 2), ('5 TP 872 990', 'sb3', 3), ('5 TP 2448 1120', 'sb4', 4)):
    rows, ev, out = run(tp + '\n', 200, {'FP_LEVEL': 'sb1'})
    lines = [l for _, l in ev]
    down = sbf(ev, 'pad'); go = [r for r in rows if r['act'] == act]
    check('pad_%s_sinks_and_launches' % lv, down and 'sid 62' in lines and 'sid 49' in lines and any(l.startswith('sbflow launch') and l.split()[3] == lv for l in lines), str(lines[:8]))
    check('pad_%s_ship_walkin' % lv, go and go[0]['g4'] == -1 and go[0]['x'] == -32 and 40 <= go[0]['f'] - down[0] <= 70 if go and down else False,
          str(go[:1] and (go[0]['f'], go[0]['x'], go[0]['g4'])))
    check('pad_%s_music_continues' % lv, [l for l in lines if l.startswith('music b')][-1].startswith('music bgm/m_stage4.adp'))

# ---- a done ship (ML flag 0x10, 9220): its pad stays down, no launch
rows, ev, out = run('2 FLAGS 16\n5 TP 1704 1100\n', 160, {'FP_LEVEL': 'sb1'})
check('pad_done_ship_no_launch', not sbf(ev, 'launch') and all(r['act'] == 1 for r in rows), str([l for _, l in ev][:5]))

# ---- a ship's opening (group 0x228f3, 9460-9470): the four planks are fired every 20 frames (sound 99), then the hover
# platforms are released (9467 B > 7), the timer stays stopped until then (9460 / 9874) and control returns with g4 1
rows, ev, out = run('5 TP 1704 1120\n', 400, {'FP_LEVEL': 'sb1'})
planks = sbf(ev, 'plank'); rel = sbf(ev, 'release'); end = sbf(ev, 'intro end')
check('intro_four_planks', len(planks) == 4 and all(20 <= b - a <= 21 for a, b in zip(planks, planks[1:])), str(planks))
check('intro_release_after_planks', rel and end and rel[0] > planks[-1] and end[0] >= rel[0], str((rel, end)))
sb2 = [r for r in rows if r['act'] == 2]
tm = [r['tm'] for r in sb2 if r['f'] < (rel[0] if rel else 0)]
check('intro_timer_stopped', len(tm) > 100 and len(set(tm[2:])) == 1, str(sorted(set(tm))[:3]))   # 9460 runs from the act's second frame

# ---- the ships' right edge (9455-9458: X > W - 50, B > 0) -> SB1 (g4 -1): flown in from x -40, 400 px up, B 15, G -5 (8882 /
# 8883), control and g4 1 after 32 frames (5047)
for lv, x in (('sb2', 12740), ('sb3', 14340), ('sb4', 15564)):
    rows, ev, out = run('5 TP %d 600\n6 120 R\n' % x, 150, {'FP_LEVEL': lv})
    s1 = [r for r in rows if r['act'] == 1]
    check('%s_exit_to_sb1_flying' % lv, len(s1) > 2 and s1[0]['g4'] == -1 and s1[0]['x'] == -40 and s1[0]['vy'] == -5 and abs(s1[2]['vx'] - 15) < 1e-6 and s1[0]['y'] < 900,
          str(s1[:3] and [(r['x'], r['y'], r['vx'], r['vy']) for r in s1[:3]]))
    k = next((i for i, r in enumerate(s1) if r['g4'] == 1), None)
    check('%s_sb1_walkin_32' % lv, k is not None and 31 <= k <= 33, str(k))

# ---- checkpoints (SB2 9550 / 9551, SB3 9876 / 9877, SB4 10076 / 10077 with MV.Y > 830) and a restart at one
for lv, tp, g4 in (('sb2', '5 TP 7300 600', 2), ('sb2', '5 TP 10850 570', 3), ('sb3', '5 TP 6000 560', 2), ('sb3', '5 TP 10300 690', 3),
                   ('sb4', '5 TP 6000 250', 2), ('sb4', '5 TP 12200 1140', 3)):
    rows, ev, out = run(tp + '\n', 12, {'FP_LEVEL': lv})
    check('%s_checkpoint_g4_%d' % (lv, g4), rows[-1]['g4'] == g4, str(rows[-1]['g4']))
rows, ev, out = run('5 TP 6000 250\n30 HP 0\n30 HURT 10\n', 260, {'FP_LEVEL': 'sb4'})
rs = [r for r in rows if r['f'] > 60 and r['lv'] == 4]
check('sb4_checkpoint_restart', rs and abs(rs[0]['x'] - 5980) < 2 and rs[0]['act'] == 4, str(rs[:1] and (rs[0]['x'], rs[0]['act'])))

# ---- the boss camera (9552: past the left marker + 600 while the arena's barriers stand)
rows, ev, out = run('5 TP 11500 700\n', 20, {'FP_LEVEL': 'sb2'})
check('sb2_boss_lock', sbf(ev, 'lock'), str([l for _, l in ev][-3:]))

# ---- the night (9005-9009): the three ships done, grounded past X 1200 -> darkening, music fade-out over 4 s (240 frames),
# timer stopped; 178 frames later (B 128 -> 64 by 0.5, then C 50) the Kujacker's group (the boss module)
rows, ev, out = run('2 FLAGS 112\n5 TP 1300 1180\n', 260, {'FP_LEVEL': 'sb1'})
n, k = sbf(ev, 'night'), sbf(ev, 'kujacker')
check('sb1_night_then_kujacker', n and k and 175 <= k[0] - n[0] <= 180, str((n, k)))
# session 20: the fade belongs to m_stage4; m_boss3 starts with the Kujacker and is not stopped by the fade's end (RV k_def 237)
mb = [f for f, l in ev if l.startswith('music bgm/m_boss3')]
check('sb1_night_music_fade', mb and not any(l.startswith('music stop') and f >= mb[0] for f, l in ev), str([l for _, l in ev if l.startswith('music')]))
check('sb1_night_timer_stop', len({at(rows, f)['tm'] for f in (20, 100, 150)}) == 1)
rows, ev, out = run('2 FLAGS 48\n5 TP 1300 1180\n', 60, {'FP_LEVEL': 'sb1'})
check('sb1_no_night_before_three_ships', not sbf(ev, 'night'))

# ---- results in SB1 (the boss module calls results_begin_boss at the Kujacker's end; host: RESBOSS): m_clear, the stage-4
# results, the wipe (sound 56) and frame 8 -> the front end (Jade Creek, frame 39, is not ported)
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run('60 RESBOSS\n' + ''.join('%d TP 1300 1180\n' % f for f in range(10, 1000, 30)), 900,
                        {'FP_LEVEL': 'sb1', 'FP_SAVE': os.path.join(d, 's.bin')})
lines = out.splitlines()
check('sb1_results_clear', any(l.startswith('music bgm/m_clear.adp') for l in lines) and any(l.startswith('ach results stage 4') for l in lines))
st = [l for l in loads(ev) if l not in ('frontend', 'gal')]
check('sb1_wipe_to_front_end', 'sid 56' in lines and rows[-1]['mode'] == 0 and st[-1:] == ['sb1'], str(st))

# ---- the Classic route: FN6's results -> frame 8 -> SB1 (MenuSequence.I = 3 -> frame 35), fresh with the card (g4 1)
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run('60 RESBOSS\n' + ''.join('%d TP 500 150\n' % f for f in range(10, 1000, 30)) + ''.join('%d %d J\n' % (f, f + 1) for f in range(820, 1300, 40)), 1400,
                        {'FP_LEVEL': 'fn6', 'FP_CHAR': '0', 'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': os.path.join(d, 's.bin')})   # J: the achievement pop-up (frame 9)
st = [l for l in loads(ev) if l not in ('frontend', 'gal')]
s1 = [r for r in rows if r['act'] == 1 and r['mode'] == 1]
check('route_fn6_to_sb1', st[1:3] == ['fn6', 'sb1'] and s1 and s1[0]['g4'] == 1 and s1[0]['x'] == 965, str((st, s1[:1] and (s1[0]['x'], s1[0]['g4']))))

# ---- Time Attack: Sky Battalion (s 4, list index 4) is listed by the level table; with the title cheat (every stage's
# progress) it starts SB1 in Time Attack
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n'
TA = '200 202 S\n360 361 D\n380 381 J\n490 491 J\n630 631 D\n650 651 D\n670 671 D\n690 691 D\n720 721 J\n840 841 J\n'
with tempfile.TemporaryDirectory() as d:
    rows, ev, out = run(CHEAT + TA, 1150, {'FP_SAVE': os.path.join(d, 's.bin')})
    check('ta_sb1_with_progress', 'sb1' in loads(ev) and rows[-1]['mode'] == 1 and rows[-1]['act'] == 1, str(loads(ev)))

# ---- music slots of the acts (levels.c rows; loop tables of frames 35-38): sb1 4 m_boss3 / 6 m_menu2, sb2 7 m_trouble
src = open(os.path.join(HERE, '..', 'src', 'game', 'levels.c')).read()
check('music_slots', 'SB("sb1", 35, 1, "bgm/m_stage4.adp"), .mus4 = "bgm/m_boss3.adp", .mus6 = "bgm/m_menu2.adp"' in src and '.mus7 = "bgm/m_trouble.adp", .card_first = 41' in src)
check('music_files', all(os.path.exists(os.path.join(DATA, 'bgm', m + '.adp')) for m in ('m_stage4', 'm_boss3', 'm_menu2', 'm_midboss', 'm_trouble')))

# ---- card ids (9763 / inline: 41, 44, 48, newest instance first)
for lv, want in (('sb2', {(9842.0, 589.0): 41, (6450.0, 101.0): 42, (3034.0, 429.0): 43}),
                 ('sb3', {(13322.0, 453.0): 44, (2410.0, 189.0): 45, (8202.0, 77.0): 46, (3026.0, 789.0): 47}),
                 ('sb4', {(11650.0, 741.0): 48, (8642.0, 285.0): 49, (5330.0, 189.0): 50})):
    _, _, o = run('', 2, {'FP_LEVEL': lv, 'FP_TRACK': '41:-1'})
    c = {(float(l.split()[5]), float(l.split()[7])): int(l.split()[13]) for l in o.splitlines() if l.startswith('ob 1 41 ')}
    check('%s_card_ids' % lv, c == want, str(c))

# ---- data: the stage sounds (converted at 11025 Hz), the parallax (9215 / 9764: D, E, G; the bob flag 2048, the mist in
# ARGB4444 at half size, Zao's airship at its world position in SB2-SB4)
check('sfx_sb_converted', all(os.path.exists(os.path.join(DATA, 'sfx', 's%d.wav' % i)) for i in (62, 99, 103, 156, 237, 255, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 438, 446)))
def par(lv):
    dd = open(os.path.join(DATA, lv, lv + '_par.bin'), 'rb').read()
    ni, np_, _, ver = struct.unpack_from('<4H', dd, 4); off = 12; imgs = []
    for _ in range(ni):
        w, h, n = struct.unpack_from('<3H', dd, off); fl = struct.unpack_from('<H', dd, off + 6)[0] if ver >= 2 else 0
        imgs.append(fl); off += (8 if ver >= 2 else 6) + 8 * n
    out = []
    for _ in range(np_):
        ii, D, E, G, y0, bw, bh, fl, hd = struct.unpack_from('<H3fi4H', dd, off); off += 26
        out.append((hd, round(D, 3), round(E, 3), round(G, 3), y0, fl, imgs[ii]))
    return out
p1, p2 = par('sb1'), par('sb2')
check('par_sb1_set', [h for h, *_ in p1] == [2059, 2134, 2060, 2133, 2062, 2063, 2061, 2064, 2065, 2094] and (2064, .7, .97, -3, 192, 2048, 3) in p1, str(p1))
check('par_sb2_zaoship', [h for h, *_ in p2] == [2059, 2060, 2062, 2063, 2061, 2064, 2065, 2243, 2094] and (2243, 128, 0, 0, 112, 4096, 0) in p2, str(p2))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
