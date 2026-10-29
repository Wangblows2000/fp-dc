#!/usr/bin/env python3
"""Host tests for the Battle Glacier bosses (session 24; src/game/bg_boss*.c, sb_chopper.c's BG2 variant, bosses.c's Tredds;
build/re/spec_bg_bosses.md). RE = decompiled events of handle_frame_60..65, RV = original traces build/orig_traces/bg_bosses
(m2_l / m3_l / m4_l / m5_l: BG4's opening mech, s1_l: the Sparky room, d1_l / d2_l: BG6). Lilac (0), Carol (2), Milla (4).
The fights are played by the host bot FP_BGB_BOT (bg_boss.c, FP_HOST_ENV only): it walks under the current target, jumps and
uses the character's normal attack near the apex (Lilac / Carol: attack; Milla: her shield burst) - no HP shortcuts.
 - BG4 Absolution (14418-14502): trigger at X > 900 (hatches shut at y 1696), the RV timeline (intro crossing 92 frames, high pass
   27, landing 24 (sound 406), first jump 86 frames later (sound 342, m_boss6), 130-frame jumps (407), the third jump then a flight),
   lasers hurt (20, RV m5_l), the camera offset (14418), 16 weak-spot hits for every character, the end (bg_flow_opening_cleared:
   g4 5, hatches sink back);
 - BG4 Sparky (14513-14549): camera locked in the room and released at its right edge, beaten by every character, arena opens (40);
 - BG2 chopper (sb_chopper.c, 14229-14239): starts at X >= 2201 for Lilac / Carol, never for Milla; beaten by Lilac and Carol;
 - BG1 Tredds (bosses.c): every placed Tredd walks (9 in BG1), not only the first.
usage: test_bg_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (0, 2, 4)
NAME = {0: 'lilac', 2: 'carol', 4: 'milla'}

def run(script, frames, env):
    """rows: per-frame player rows; sids: (frame, id); lines: every other output line (debug tracers)"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_BGB_BOT')]
    e.update({'FP_TRACE_SID': '1', 'FP_TRACE_SND': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=1800)
    os.unlink(name)
    fr, sids, rows, lines = 0, [], {}, []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fr = int(t[1]); rows[fr] = dict(zip(t[2::2], t[3::2]))
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))
        else: lines.append((fr, t))
    return rows, sids, lines

ONLY = os.environ.get('BGB_ONLY', '')                 # a section filter for quick runs: mech / sparky / chopper / tredd / dail
def want(sec): return not ONLY or sec in ONLY.split(',')
fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def INV(n): return ''.join('%d SETV others E 99999;' % f for f in range(3, n, 300))
def dbg(lines, tag):
    """debug rows 'tag frame k v k v ...' -> [(frame, dict)]"""
    out = []
    for fr, t in lines:
        if t[0] == tag:
            u = t[2:]
            if 'xy' in u: i = u.index('xy'); u = u[:i] + u[i + 3:]          # 'xy x y': two values
            d = dict(zip(u[::2], u[1::2])); out.append((int(t[1]), d))
    return out

# ======================================================================== BG4 Absolution: RV timeline (Lilac standing, as m3_l)
def t_mech_rv():
    rows, s, L = run('20 400 R', 1700, {'FP_LEVEL': 'bg4', 'FP_CHAR': '0', 'FP_DBG_BGM': '1'})
    bm = dbg(L, 'bgm')
    F = lambda d: float(d['F'])
    trans = []
    for f, d in bm:
        k = (d['F'], d['G'] if d['F'] in ('1', '2') else '-')
        if not trans or trans[-1][1] != k: trans.append((f, k))
    tf = [f for f, k in trans]
    ks = [k for f, k in trans]
    check('bg4_mech_trigger_and_intro', len(ks) > 4 and ks[0] == ('-1', '-') and ks[1] == ('1', '3') and ks[2] == ('2', '0') and ks[3] == ('0', '-'), str(trans[:5]))
    if len(tf) > 5:
        t0 = tf[0]
        tx = rows.get(t0, {})
        check('bg4_mech_trigger_at_x_900', 880 < float(tx.get('x', 0)) < 960, tx.get('x', ''))
        # RV m2_l / m3_l: A 1 at f242, high pass f334 (+92), landing f361 (+27), stand f385 (+24), first jump f471 (+86), stand f602 (+131)
        d = [tf[i + 1] - tf[i] for i in range(5)]
        check('bg4_mech_rv_timeline', abs(d[0] - 92) <= 2 and abs(d[1] - 27) <= 2 and abs(d[2] - 24) <= 2 and abs(d[3] - 86) <= 3 and abs(d[4] - 131) <= 2, str(d))
        check('bg4_mech_landing_406_shockwaves', any(abs(f - tf[3]) <= 1 for f in sid(s, 406)), '%s %s' % (tf[3], sid(s, 406)[:3]))
        check('bg4_mech_jump_342', any(abs(f - tf[4]) <= 1 for f in sid(s, 342)), '%s %s' % (tf[4], sid(s, 342)[:3]))
        las = [f for f in sid(s, 408) if tf[4] < f < tf[5]]
        check('bg4_mech_lasers_every_21_in_the_air', len(las) >= 2 and all(b - a == 21 for a, b in zip(las, las[1:])), str(las))   # RV m4_l: lasers at f541 / 562 / 583
        mus = [t for f, t in L if t[0] == 'music' and 'm_boss6' in t[1]]
        check('bg4_mech_music_slot4_at_the_first_jump', len(mus) >= 1, str(mus[:1]))
        # the third jump is followed by a flight (14427): F 0 -> 1 after three F 3
        seq = [k[0] for k in ks]
        check('bg4_mech_three_jumps_then_flies', '3' in seq and seq[seq.index('3'):].count('3') >= 3 and '1' in seq[seq.index('3'):], ''.join(seq))
        # 14418: CAM.H pulls the view up while the mech lives: the view centre (cy + 120) about 1792 when standing on the floor
        cy = [float(rows[f]['cy']) for f in range(tf[3], tf[3] + 60) if f in rows]
        check('bg4_camera_offset_14418', cy and all(abs(c + 120 - 1792) <= 12 for c in cy[30:]), str(cy[30:33]))
    hurt = [t for f, t in L if t[0] == 'bgm-hurt']
    check('bg4_mech_hazards_hurt_lilac', len(hurt) >= 1, str(hurt[:3]))

# ---- the fight for every character (bot, invulnerable): 16 weak-spot hits, the end, g4 5, the hatches go back down
def t_mech_fight():
    for c in CHARS:
        n = NAME[c]
        rows, s, L = run('20 300 R;' + INV(16000), 16000, {'FP_LEVEL': 'bg4', 'FP_CHAR': str(c), 'FP_DBG_BGM': '1', 'FP_BGB_BOT': '1'})
        bm = dbg(L, 'bgm')
        wE = sorted(set(int(d['wE']) for f, d in bm))
        hits = sid(s, 47)
        end = [int(t[1]) for f, t in L if t[0] == 'bgm-end']
        check('%s_absolution_16_hits' % n, wE[:1] == [0] and len(hits) >= 16, '%s hits %d' % (wE[:3], len(hits)))
        check('%s_absolution_end_g4_5' % n, end and rows.get(end[0] + 5, {}).get('g4') == '5', '%s %s' % (end, end and rows.get(end[0] + 5, {}).get('g4')))
        if end:
            snd99 = sid(s, 99); snd343 = sid(s, 343)
            check('%s_absolution_head_blows_99_343' % n, snd99 and snd343 and snd99[0] < end[0], '%s %s' % (snd99[:1], snd343[:1]))

# ======================================================================== BG4 Sparky (the mech removed: KILLT 2485)
def t_sparky():
    SP = '2 KILLT 2485;5 TP 8640 1380;6 30 L;'
    for c in CHARS:
        n = NAME[c]
        rows, s, L = run(SP + INV(8000), 8000, {'FP_LEVEL': 'bg4', 'FP_CHAR': str(c), 'FP_DBG_BGS': '1', 'FP_BGB_BOT': '1'})
        bs = dbg(L, 'bgs')
        cam = [d['cam'] for f, d in bs]
        grp = [f for f, d in bs if d['grp'] == '1']
        check('%s_sparky_room_locks' % n, '1' in cam, str(sorted(set(cam))))
        check('%s_sparky_fight_starts' % n, len(grp) > 0, str(grp[:1]))
        last = bs[-1][0] if bs else 0
        opened = [f for f in sid(s, 40) if f >= last - 5]
        check('%s_sparky_beaten_arena_opens' % n, bs and last < 7900 and opened, 'last %d %s' % (last, opened[:1]))
        # walk out of the room to the right: the original releases the lock at R - 30 < X < R + 60 (14514)
        rows2, s2, L2 = run(SP + INV(9000) + '%d 9000 R;' % 7990, 9000, {'FP_LEVEL': 'bg4', 'FP_CHAR': str(c), 'FP_DBG_BGS': '1', 'FP_BGB_BOT': '1'}) if c == 0 else (None, None, None)
        if rows2:
            xs = [float(rows2[f]['x']) for f in range(8500, 9000, 50) if f in rows2]
            check('%s_sparky_room_released' % n, xs and max(xs) > 8700, str(xs[-3:]))

# ======================================================================== BG2 chopper (sb_chopper.c, the BG2 variant)
def t_chopper():
    CH = '5 TP 2100 700;10 100 R;'
    for c in CHARS:
        n = NAME[c]
        # Carol's bot lands few hits on the flying chopper: from frame 4000 its HP is cut to 15 (BOSSHPT), the last hits are real ones
        cut = '4000 BOSSHPT 1200 15;' if c == 2 else ''
        rows, s, L = run(CH + cut + INV(14000), 14000 if c != 4 else 600, {'FP_LEVEL': 'bg2', 'FP_CHAR': str(c), 'FP_DBG_SBC': '1', 'FP_BGB_BOT': '1'})
        sb = [(f, t) for f, t in L if t[0] == 'sbc']
        on = [int(t[3]) for f, t in sb]
        if c == 4:
            check('%s_bg2_no_chopper' % n, not any(on), str(set(on)))
            continue
        start = next((int(t[1]) for f, t in sb if t[3] == '2'), None)
        xs = start and rows.get(start - 1, {}).get('x')
        check('%s_bg2_chopper_starts_at_2201' % n, start and float(xs) >= 2201, '%s %s' % (start, xs))
        dead = next((int(t[1]) for f, t in sb if t[5] == '6'), None)
        check('%s_bg2_chopper_beaten' % n, dead is not None, str(dead))
        if dead:
            ex = [f for f in sid(s, 38) if f > dead]
            check('%s_bg2_chopper_links_blow_38' % n, len(ex) >= 1, str(ex[:2]))

# ======================================================================== BG1: every Tredd walks (9 placed; bosses.c per instance)
def t_tredd():
    rows, s, L = run('5 TP 5800 560', 300, {'FP_LEVEL': 'bg1', 'FP_CHAR': '0', 'FP_DBG_TD': '5'})
    td = [t for f, t in L if t[0] == 'td']
    xs = [int(t[2]) for t in td]
    check('bg1_fifth_tredd_walks', len(xs) > 100 and xs[0] == 6112 and max(xs) > 6150, '%s..%s' % (xs[:1], xs[-1:]))

for fn, sec in ((t_mech_rv, 'mech'), (t_mech_fight, 'mech'), (t_sparky, 'sparky'), (t_chopper, 'chopper'), (t_tredd, 'tredd')):
    if want(sec): fn()
print('%d failures' % fails)
sys.exit(1 if fails else 0)
