#!/usr/bin/env python3
"""Host tests for the Trap Hideout enemies (session 21b; src/game/th_enemy.c, src/game/enemy_kit.h, build/re/spec_th_enemies.md)
against the original's traces (RV, build/orig_traces/th_enemies: session1.sh / session2.sh, the same input scripts as below;
frame numbers are the trace's: a sound at the trace's .snd frame, an object state at its .csv row = the host's 'ob' frame + 2).
Carol (2) and Milla (4) for the timed behaviours (the original gives both the same enemy timelines):
metal pup (walk 2 px, bites 59 frames with contact damage 5 from its frame 3, 30-frame cooldown, the limiter turn), shock puppet
(sound 167 every 242 frames), blade orbiter (sound 151 every 158 frames, the blades' swing positions), rocket puncher (the
30 / 40 / 17 / 15 / 71-frame cycle, the 200-px punch, its hand breaking totems), totems (Carol's claw, sound 314, removed the next
frame), the bike ninja (th3: star 28 frames after the jump, skid sound every 6 frames, the star bursts on the floor), hurts
(the pup's bite 5, a blade 10 with MV.B -2), Carol's hits (pup / puppet defeats and their landings, the idle puncher's deflects).
usage: test_th_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
HOFF = 2                                                     # trace row = host 'ob' frame + HOFF (in-view / camera phase)

def run(script, frames, env):
    """sids: (frame, id) in the trace's numbering; obs: (frame, type, x, y, fields); rows: host player rows by frame"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    fr, sids, obs, rows = 0, [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
        elif t[0] == 'sid': sids.append((fr + 1, int(t[1])))        # a sound belongs to the next 'f' row (trace frame numbering)
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]) + HOFF, int(t[2]), float(d['x']), float(d['y']), d))
    return sids, obs, rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return sorted(set(f for f, i in sids if i == n))
def gaps(v): return [b - a for a, b in zip(v, v[1:])]
def near(a, b, tol): return a is not None and b is not None and abs(a - b) <= tol
def changes(obs, key):
    """(frame, value) whenever the field changes"""
    out, prev = [], None
    for f, t, x, y, d in obs:
        if d[key] != prev: out.append((f, float(d[key]))); prev = d[key]
    return out
def first(ch, v, after=0):
    return next((f for f, val in ch if val == v and f > after), None)
INV = '3 SETV others E 99999\n'
VUL = '3 SETV others E 0\n'
CH = (2, 4)

for c in CH:
    # ---- metal pup (th2 495, 1644; RV pup / pup_hurt_c4 (Milla, invulnerable): walk from row 62, bites 137-196 and 226-284
    # (x 343 / 283), the limiter turn 290-298 at x 271, E 1, the next bite 314)
    s, obs, _ = run(INV + '60 TP 300 1630\n', 400, {'FP_LEVEL': 'th2', 'FP_CHAR': str(c), 'FP_TRACK': '1561:-1'})
    pup = [o for o in obs if o[1] == 1561 and float(o[4]['a1']) != 0 and 200 < o[2] < 520 and 1600 < o[3] < 1700]
    D = changes(pup, 'a0')
    b1 = first(D, 2); e1 = first(D, 0, b1 or 0); b2 = first(D, 2, e1 or 0); e2 = first(D, 0, b2 or 0); t1 = first(D, 1); t0 = first(D, 0, t1 or 0)
    xs = {f: x for f, t, x, y, d in pup}
    walk = [xs[f] - xs[f + 1] for f in range(70, 120) if f in xs and f + 1 in xs]
    check('c%d_pup_walks_2px' % c, walk and all(w == 2 for w in walk), str(walk[:5]))
    check('c%d_pup_bite_137' % c, near(b1, 137, 3), '%s %s' % (b1, xs.get(b1)))
    check('c%d_pup_bite_59_frames' % c, b1 and e1 and abs(e1 - b1 - 59) <= 2, '%s %s' % (b1, e1))
    check('c%d_pup_cooldown_30' % c, e1 and b2 and abs(b2 - e1 - 30) <= 1, '%s %s' % (e1, b2))
    bite = [(int(d['fr']), int(d['bd'])) for f, t, x, y, d in pup if d['a0'] == '2.000']
    check('c%d_pup_bite_damage_from_frame3' % c, bite and all(bd == (5 if fr >= 3 else 0) for fr, bd in bite) and any(bd == 5 for fr, bd in bite), str(sorted(set(bite))))
    check('c%d_pup_limiter_turn_290' % c, near(t1, 290, 5) and t0 and abs(t0 - t1 - 8) <= 1 and near(xs.get(t1), 271, 2), '%s %s %s' % (t1, t0, xs.get(t1)))
    after = [float(d['a1']) for f, t, x, y, d in pup if t0 and f > t0 + 1]
    check('c%d_pup_turns_right' % c, after and all(a == 1 for a in after), str(after[:3]))

    # ---- shock puppet (th2 1151, 1136; RV puppet: sound 167 at 142 and 384; wind-up 92-132, shock 132-243)
    s, obs, _ = run(INV + '60 TP 1080 1110\n', 450, {'FP_LEVEL': 'th2', 'FP_CHAR': str(c), 'FP_TRACK': '1562:-1'})
    sh = sid(s, 167)
    check('c%d_puppet_shock_142_384' % c, len(sh) >= 2 and near(sh[0], 142, 2) and sh[1] - sh[0] == 242, str(sh))
    pp = [o for o in obs if o[1] == 1562 and abs(o[2] - 1151) < 4]
    D = changes(pp, 'a0')
    w = first(D, 1); k = first(D, 2); z = first(D, 0, k or 0)
    check('c%d_puppet_windup_40' % c, w and k and abs(k - w - 40) <= 1 and near(w, 92, 2), '%s %s' % (w, k))
    check('c%d_puppet_shock_111' % c, k and z and abs(z - k - 111) <= 1, '%s %s' % (k, z))

    # ---- blade orbiter (th2 704, 1576 + blades 684 / 723; RV orb: attack (sound 151) at 60, 218, 376; the blades (row: x, y))
    s, obs, _ = run(INV + '60 TP 620 1560\n', 420, {'FP_LEVEL': 'th2', 'FP_CHAR': str(c), 'FP_TRACK': '1563:-1,1564:-1,1565:-1'})
    at = sid(s, 151)
    check('c%d_orbiter_attacks_158' % c, len(at) >= 3 and near(at[0], 60, 2) and gaps(at[:3]) == [158, 158], str(at))
    orb = [o for o in obs if o[1] == 1563 and o[2] == 704]
    F = changes(orb, 'a1'); f1 = first(F, 1)
    bl = {f: (x, y) for f, t, x, y, d in obs if t == 1564 and 600 < x < 720 and 1450 < y < 1650}
    br = {f: (x, y) for f, t, x, y, d in obs if t == 1565 and 690 < x < 800 and 1450 < y < 1650}
    oy = {f: y for f, t, x, y, d in orb}
    if f1:
        k0 = f1 - 61                                         # RV: F 1 at row 61
        rv_l = {82: (669, 1524), 85: (661, 1517), 100: (661, 1594), 108: (684, 1552), 120: (684, 1611), 140: (684, 1620), 148: (684, 1564)}
        bad = [(r, bl.get(r + k0), v) for r, v in rv_l.items() if bl.get(r + k0) != v]
        check('c%d_orbiter_left_blade_path' % c, not bad, str(bad[:3]))
        rv_r = {90: (754, 1535), 97: (752, 1586), 105: (732, 1576), 120: (724, 1611)}
        bad = [(r, br.get(r + k0), v) for r, v in rv_r.items() if br.get(r + k0) != v]
        check('c%d_orbiter_right_blade_path' % c, not bad, str(bad[:3]))
        rv_o = {100: 1580, 112: 1572, 120: 1579, 160: 1573}
        bad = [(r, oy.get(r + k0), v) for r, v in rv_o.items() if oy.get(r + k0) != v]
        check('c%d_orbiter_bob' % c, not bad, str(bad[:3]))
        check('c%d_orbiter_swing_cycle' % c, [f - k0 for f, v in F[:4]] == [55 + 0, 61, 81, 148][:len(F)] or [f - k0 for f, v in F[1:4]] == [61, 81, 148], str([(f - k0, v) for f, v in F[:5]]))
    else:
        check('c%d_orbiter_attack_state' % c, False, str(F[:4]))

    # ---- rocket puncher (th2 6224, 1392; RV punch: D 1 at 92, 2 at 122, 3 at 162, 4 at 179, 0 at 194, 1 at 265; the hand at
    # X - 200 = 6024 at the turn; its fist breaks two totems: sounds 23 + 314 at 126 and 132)
    s, obs, _ = run(INV + '60 TP 6080 1380\n', 400, {'FP_LEVEL': 'th2', 'FP_CHAR': str(c), 'FP_TRACK': '1566:-1,1567:-1'})
    pu = [o for o in obs if o[1] == 1566 and o[2] == 6224]
    D = changes(pu, 'a0')
    seq = [(f, v) for f, v in D if f > 60][:6]
    want = [(92, 1), (122, 2), (162, 3), (179, 4), (194, 0), (265, 1)]
    check('c%d_puncher_cycle' % c, len(seq) == 6 and all(v == wv and abs(f - wf) <= 2 for (f, v), (wf, wv) in zip(seq, want)) and
          [b[0] - a[0] for a, b in zip(seq, seq[1:])] == [30, 40, 17, 15, 71], str(seq))
    hx = [x for f, t, x, y, d in obs if t == 1567 and 5900 < x < 6300 and 1380 < y < 1420]
    check('c%d_puncher_hand_200px' % c, hx and min(hx) == 6024 and max(hx) == 6248, '%s %s' % (min(hx) if hx else None, max(hx) if hx else None))
    tb = sid(s, 314)
    check('c%d_puncher_fist_breaks_totems' % c, tb[:2] and near(tb[0], 126, 2) and tb[1] - tb[0] == 6 and set(sid(s, 23)) >= set(tb[:2]), '%s %s' % (tb, sid(s, 23)))

# ---- totems: Carol's third claw breaks the 4744 totem (RV totem_c2: sounds 24 / 23 / 314 at 226, gone at row 227, one crystal)
s, obs, _ = run(INV + '60 TP 4700 1600\n100 140 X\n160 200 X\n220 260 X\n', 300, {'FP_LEVEL': 'th2', 'FP_CHAR': '2', 'FP_TRACK': '1560:-1'})
br = sid(s, 314)
check('c2_totem_claw_break_226', br and near(br[0], 226, 3) and br[0] in sid(s, 23) and br[0] in sid(s, 24), '%s %s' % (br, sid(s, 23)))
# (the claw's box meets the tops of two totems: one random one breaks (RV 4744, 1592; the host's random draw differs))
last = {}
for f, t, x, y, d in obs:
    if t == 1560 and 4600 < x < 4800 and 1550 < y < 1650: last[(x, y)] = f
gone = [(k, f) for k, f in last.items() if f < 290]
check('c2_totem_removed_next_frame', br and 1 <= len(gone) <= 2 and all(br[0] - 1 + HOFF <= f <= br[0] + HOFF for k, f in gone) and
      max(abs(a[0][0] - b[0][0]) + abs(a[0][1] - b[0][1]) for a in gone for b in gone) <= 6, '%s %s' % (br[:1], gone))   # (one totem: hit, flung 1 frame)

# ---- bike ninja (th3 2248, 992; RV bike: star (71) at 61 and 28 frames after the jump (232 at 94 -> 122), skid 335 every 6
# frames, the first star bursts on the floor (240) at 101)
for c in CH:
    s, obs, _ = run(INV + '60 TP 2150 980\n', 260, {'FP_LEVEL': 'th3', 'FP_CHAR': str(c), 'FP_TRACK': '1571:-1,1590:-1'})
    st, jp, sk, bu = sid(s, 71), sid(s, 232), sid(s, 335), sid(s, 240)
    check('c%d_bike_first_star' % c, st and near(st[0], 61, 2), str(st))
    check('c%d_bike_jump_94' % c, jp and near(jp[0], 94, 3), str(jp))
    st2 = [f for f in st if jp and f > jp[0]]
    check('c%d_bike_star_28_after_jump' % c, jp and st2 and st2[0] - jp[0] == 28, '%s %s' % (jp[:1], st2[:1]))
    check('c%d_bike_skid_6' % c, len(sk) >= 3 and gaps(sk[:3]) == [6, 6] and near(sk[0], 93, 3), str(sk))
    check('c%d_bike_star_bursts_101' % c, bu and near(bu[0], 101, 2), str(bu))
    sp = [(f, x, y) for f, t, x, y, d in obs if t == 1590 and 2150 < x < 2260 and 950 < y < 1010 and f <= 106]   # the first star (RV: 62-101)
    check('c%d_bike_star_at_action_point' % c, sp and abs(sp[0][1] - 2223) <= 1 and abs(sp[0][2] - 990) <= 1, str(sp[:1]))
    by = {}                                                  # every star by its creation frame (frame - B): its flight steps
    for f, t, x, y, d in obs:
        if t == 1590: by.setdefault(f - int(float(d['a1'])), []).append((f, x, y, float(d['a1']), float(d['a0'])))
    mv = [(b[1] - a[1], b[2] - a[2], a[4]) for v in by.values() for a, b in zip(v, v[1:]) if b[0] == a[0] + 1 and a[3] >= 41]
    check('c%d_bike_star_flies_7_7' % c, mv and all(m[1] == 7 and m[0] == m[2] and abs(m[0]) == 7 for m in mv), str(mv[:3]))

# ---- hurts (OTHERS.E 0): the pup's bite (RV pup_hurt2_c2: Carol hurt at 237, 5 = one petal), a blade (RV orb_hurt2_c2: 118 / 261,
# R-hurt 10 and MV.B - 2), the puppet's shock never reaches Milla at 1110 (RV puppet_hurt2_c4)
s, _, rows = run(VUL + '60 TP 300 1630\n', 280, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
hu = sid(s, 25)
check('c2_pup_bite_hurts_237', hu and near(hu[0], 237, 4), str(hu))
s, _, rows = run(VUL + '60 TP 690 1560\n', 300, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
hu = sid(s, 25)
check('c2_blade_hurts_118_261', len(hu) >= 2 and near(hu[0], 118, 3) and near(hu[1], 261, 3), str(hu))
hp = [int(r['hp']) for f, r in sorted(rows.items()) if hu and f in (hu[0] - 3, hu[0] + 3)]
check('c2_blade_hurt_10', len(hp) == 2 and hp[0] - hp[1] == 2, str(hp))
s, _, _ = run(VUL + '60 TP 1110 1110\n', 300, {'FP_LEVEL': 'th2', 'FP_CHAR': '4'})
check('c4_puppet_shock_misses_1110', not sid(s, 25) and not sid(s, 371) and sid(s, 167), '%s %s' % (sid(s, 25), sid(s, 167)))

# ---- Carol's claw (RV pup_hit_c2: the pup is defeated at 208 (sound 23) and lands 25 frames later (sound 50); puppet_hit_c2:
# defeat 167, landing 203; punch_hit_c2: the idle puncher deflects (sound 30) at 196 and 211)
CL = ''.join('%d %d X\n' % (f, f + 3) for f in range(100, 301, 20))
s, _, _ = run(INV + '60 TP 300 1630\n' + CL, 300, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
d, l = sid(s, 23), sid(s, 50)
check('c2_pup_defeat_208', d and near(d[0], 208, 4), str(d))
check('c2_pup_lands_25_later', d and l and abs(l[0] - d[0] - 25) <= 3, '%s %s' % (d, l))
s, _, _ = run(INV + '60 TP 1110 1110\n' + CL, 260, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
d, l = sid(s, 23), sid(s, 50)
check('c2_puppet_defeat_167', d and near(d[0], 167, 4), str(d))
check('c2_puppet_lands_36_later', d and l and abs(l[0] - d[0] - 36) <= 4, '%s %s' % (d, l))
s, _, _ = run(INV + '60 TP 6150 1380\n80 83 R\n200 203 X\n', 260, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
dfl, cl = sid(s, 30), [f for f in sid(s, 412) + sid(s, 411) if f >= 200]
check('c2_puncher_idle_deflects', cl and dfl and dfl[0] == cl[0] and not [f for f in sid(s, 24) if f >= 200], '%s %s %s' % (cl, dfl, sid(s, 24)))

# ---- the TH2 totem wall (5896.., 1416-1576; RV wall_c2 / wall_c4): Carol's claw breaks the two bottom totems at 107 / 112 (and picks
# up their crystals: sound 27), Milla's thrown cube bursts on two at 182 / 183 and on more at 313+ (the passage opens under the rest)
s, _, _ = run(INV + '60 TP 5850 1560\n70 72 R\n' + ''.join('%d %d X\n' % (f, f + 3) for f in range(100, 391, 25)), 400, {'FP_LEVEL': 'th2', 'FP_CHAR': '2'})
br = sid(s, 314)
check('c2_wall_claw_breaks_two', len(br) == 2 and near(br[0], 107, 3) and br[1] - br[0] == 5, str(br))
s, _, _ = run(INV + '60 TP 5850 1560\n70 72 R\n100 160 X\n180 182 X\n230 290 X\n300 302 X\n', 400, {'FP_LEVEL': 'th2', 'FP_CHAR': '4'})
br = sid(s, 314)
check('c4_wall_cube_breaks', len(br) >= 4 and near(br[0], 182, 2) and br[1] - br[0] == 1 and any(near(f, 313, 3) for f in br[2:]), str(br))
# ---- Milla's held cube in front of a Rocket Puncher's fist sends it back at once (RE 13673; RV punch_cube_c4: D 1 -> 4 at 122, idle
# again at 137 instead of 194)
s, obs, _ = run(INV + '60 TP 6150 1380\n62 64 R\n66 112 X\n', 260, {'FP_LEVEL': 'th2', 'FP_CHAR': '4', 'FP_TRACK': '1566:-1'})
pu = [o for o in obs if o[1] == 1566 and o[2] == 6224]
D = changes(pu, 'a0'); d2 = first(D, 2); d4 = first(D, 4); d0 = first(D, 0, d4 or 0)
emax = max([float(d['a1']) for f, t, x, y, d in pu if d2 and d4 and d2 <= f <= d4] or [999])
check('c4_cube_sends_fist_back', d2 and d4 and d4 - d2 <= 2 and near(d4, 122, 2) and near(d0, 137, 2) and emax <= 48, '%s %s %s E %s' % (d2, d4, d0, emax))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
