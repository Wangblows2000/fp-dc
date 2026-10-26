#!/usr/bin/env python3
"""Host tests for the Fortune Night Panther (session 17; src/game/fn_panther.c; build/re/spec_fn_bosses.md F / G / X,
spec_fn_flow 5.3 / 5.4): the FN5 intro / chase (trigger, input lock, Ship2 timeline, control back, Milla's branch, the exit
to FN6), the FN6 fight per character (floor mode Lilac / Carol: the auto-run and treadmill; plane mode Milla), every attack
with its damage (plasma 5, laser 10, the rider's rapid bullets 1, the back cannon's missile blasts 10, the rider's shield 5),
the HP pools and the jaw exposure, mode 2, the defeat (timer stop, music stop), the post-boss scene and the results, and an
FN6 death back to FN5. Reads the module's host trace (FP_DBG_FP: 'fp' state rows, 'fpdmg' damage, 'fpev' events).
usage: test_fn_panther.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')


def num(v):
    try: return float(v)
    except ValueError: return v


def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_DBG_FP': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    R = {'fpp': {}, 'f': [], 'fp': [], 'dmg': [], 'ev': [], 'sid': [], 'load': [], 'music': [], 'out': r.stdout + r.stderr}
    for line in R['out'].splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': R['f'].append({t[i]: num(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'fp': d = {t[i]: num(t[i + 1]) for i in range(2, len(t) - 1, 2)}; d['f'] = int(t[1]); R['fp'].append(d)
        elif t[0] == 'fpp': R['fpp'][int(t[1])] = {int(k): tuple(map(float, v.split(','))) for k, v in (w.split(':') for w in t[2:]) if k.isdigit()}
        elif t[0] == 'fpdmg': R['dmg'].append((int(t[1]), t[2], int(t[3])))
        elif t[0] == 'fpev': R['ev'].append((int(t[1]), t[2], float(t[3]), float(t[4])))
        elif t[0] == 'sid': R['sid'].append((len(R['f']), int(t[1])))
        elif t[0].startswith('load['): R['load'].append(t[0][5:-1])
        elif t[0] == 'music': R['music'].append(line)
    return R


fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1


def first(rows, pred):
    return next((r for r in rows if pred(r)), None)


def hp_refill(extra=(), until=3000):   # keep the player alive (HP 14 every 50 frames) + extra script lines
    return ''.join('%d HP 14\n' % f for f in range(50, until, 50)) + ''.join(l + '\n' for l in extra)


def tp_each(f0, f1, x, y):             # the floor's treadmill moves the player: re-teleport every frame
    return ['%d TP %d %d' % (f, x, y) for f in range(f0, f1 + 1)]


def kill_script(start=300, step=70, n=30):   # the hitbox's HP to 0 every 70 frames (the pools and the exposed hits)
    return '5 SETV others E 99999\n' + ''.join('%d BOSSHP 0\n' % (start + k * step) for k in range(n))


# ---------------------------------------------------------------- FN5: the intro / chase (8370-8419, RV T-F1)
R = run('5 TP 12000 210\n20 1500 R\n', 1300, {'FP_LEVEL': 'fn5'})
a5 = [r for r in R['fp'] if r['act'] == 5]
trig = first(a5, lambda r: r['chase'] == 1)
check('fn5_chase_trigger_x', trig is not None and 12480 <= trig['px'] <= 12600, str(trig and trig['px']))   # RV 12545
check('fn5_ship2_b_jumps_270', trig is not None and trig['s2B'] >= 270, str(trig and trig['s2B']))           # RV: B 270 (not Milla)
lock = [r for r in a5 if r['chase'] == 1 and 280 <= r['s2B'] < 480]
check('fn5_input_locked', lock and all(abs(r['B']) < 0.5 for r in lock[20:]), str(lock[-1:] and lock[-1]['B']))
k440 = first(a5, lambda r: r['chase'] == 1 and r['s2B'] >= 440)
check('fn5_panther_lands_k_l', k440 is not None and abs(k440['K'] - 12475) <= 24 and abs(k440['L'] - 106) <= 2,   # RV K 12475 L 106
      str(k440 and (k440['K'], k440['L'])))
back = first(a5, lambda r: r['chase'] == 1 and r['s2B'] >= 491)
check('fn5_control_back_lilac_speed_12', back is not None and back['B'] >= 11, str(back and back['B']))       # 8409: Lilac B 12
sids = [s for _, s in R['sid']]
check('fn5_chase_sounds', 13 in sids and 193 in sids and 126 in sids, str(sorted(set(sids) & {9, 13, 126, 193})))
run_on = [r for r in a5 if r['chase'] == 1 and r['s2B'] >= 530]
check('fn5_panther_runs_off', len(run_on) > 10 and run_on[-1]['K'] > run_on[0]['K'] + 100)
check('fn5_exit_to_fn6', R['load'][-1:] == ['fn6'], str(R['load']))
# Milla (8373 / 8376-8378): the panther placed at once (K = view + 426 + 1000), Milla moved to K + 200
R = run('5 TP 12000 210\n20 400 R\n', 420, {'FP_LEVEL': 'fn5', 'FP_CHAR': '4'})
a5 = [r for r in R['fp'] if r['act'] == 5 and r['chase'] == 1]
check('fn5_milla_k_13817', a5 and abs(a5[0]['K'] - 13817) <= 16, str(a5[:1] and a5[0]['K']))   # RV 13817
mv = first(a5, lambda r: r['px'] > 13000)
check('fn5_milla_moved_to_k_plus_200', mv is not None and abs(mv['px'] - (a5[0]['K'] + 200)) < 2, str(mv and mv['px']))

# ---------------------------------------------------------------- FN6: floor mode (Lilac / Carol), plane mode (Milla)
for ch, nm in (('0', 'lilac'), ('2', 'carol')):
    R = run('5 SETV others E 99999\n', 400, {'FP_LEVEL': 'fn6', 'FP_CHAR': ch})
    fp = R['fp']
    check('%s_fn6_floor_mode' % nm, fp and fp[0]['pl'] == 0 and abs(fp[0]['K'] - 216) < 2 and fp[0]['hA'] == 60, str(fp[:1] and (fp[0]['pl'], fp[0]['K'])))
    walk = [r['B'] for r in fp[:30]]
    check('%s_fn6_walkin_autorun' % nm, walk[5] > walk[1] and max(walk) > 8, str(walk[:8]))   # CTRL.D during the walk-in (8516 / 8517)
    late = [r for r in fp if 250 <= r['f'] <= 380]
    check('%s_fn6_treadmill_holds_10' % nm, late and all(9.9 <= r['B'] <= 10.3 for r in late) and max(r['px'] for r in late) - min(r['px'] for r in late) < 6,
          str(late[:1] and (late[0]['B'], late[0]['px'], late[-1]['px'])))   # RV T-G1: B 10.08, X creeps
R = run('5 SETV others E 99999\n', 400, {'FP_LEVEL': 'fn6', 'FP_CHAR': '4'})
fp = R['fp']
check('milla_fn6_plane_mode', fp and all(r['pl'] == 1 for r in fp), str(fp[:1] and fp[0]['pl']))
check('milla_fn6_on_the_plane_x520', fp and all(abs(r['px'] - 520) < 4 and 160 <= r['py'] <= 200 for r in fp[5:]),
      str(sorted({(round(r['px']), round(r['py'])) for r in fp[5:]})[:4]))   # RV T-G3: X 520, Y 170-183
R = run('5 SETV others E 99999\n20 120 U\n', 140, {'FP_LEVEL': 'fn6', 'FP_CHAR': '4'})
check('milla_fn6_plane_up', min(r['py'] for r in R['fp'][100:]) < 120, str(min(r['py'] for r in R['fp'][100:])))   # 8526: CTRL.A raises the plane

# ---------------------------------------------------------------- attacks and their damage
R = run(hp_refill(), 1400, {'FP_LEVEL': 'fn6'})
fp = R['fp']
v1 = first(fp, lambda r: r['fcD'] == 1)
check('fn6_first_plasma_volley', v1 is not None and 100 <= v1['f'] <= 140, str(v1 and v1['f']))   # RV T-G1 f117
check('fn6_plasma_damage_5', any(s == 'plasma' and d == 5 for _, s, d in R['dmg']), str(R['dmg'][:3]))
check('fn6_plasma_sounds', {191, 436} <= {s for _, s in R['sid']})
check('fn6_rider_phase', any(r['rd'] == 1 for r in fp) and 193 in {s for _, s in R['sid']} and 194 in {s for _, s in R['sid']})
rf = [e for e in R['ev'] if e[1] == 'rfall']
check('fn6_rapid_bullets_30', 20 <= len(rf) <= 32, str(len(rf)))    # RV: 30 bullets, one every 3 frames
bl = [e for e in R['ev'] if e[1] == 'blast']
check('fn6_back_cannon_missiles', len(bl) >= 8 and 195 in {s for _, s in R['sid']} and 258 in {s for _, s in R['sid']}, str(len(bl)))   # 11 x 195, Lilac's voice 258
if rf:
    f0, x0 = rf[0][0], int(rf[0][2])
    R2 = run(hp_refill(tp_each(f0 + 20, f0 + 75, x0, 266)), f0 + 80, {'FP_LEVEL': 'fn6'})
    check('fn6_rapid_bullet_damage_1', any(s == 'rapid' and d == 1 for _, s, d in R2['dmg']), str(R2['dmg'][-3:]))
if bl:
    f0, x0 = bl[0][0], int(bl[0][2])
    R2 = run(hp_refill(tp_each(f0 - 2, f0 + 4, x0, 266)), f0 + 10, {'FP_LEVEL': 'fn6'})
    check('fn6_missile_blast_damage_10', any(s == 'missile' and d == 10 for _, s, d in R2['dmg']), str(R2['dmg'][-3:]))
R = run(hp_refill(), 800, {'FP_LEVEL': 'fn6', 'FP_DBG_FP': '2'})   # the rider's ShieldSerpentine (5): the player put on the rider (part 4)
sh = [r['f'] for r in R['fp'] if r['shv'] == 1]
check('fn6_rider_shield_shown', len(sh) > 60, str(sh[:1] + sh[-1:]))
f0 = sh[0] + 10 if sh else 600
R2 = run(hp_refill(['%d TP %d %d' % (f, R['fpp'][f][4][0], R['fpp'][f][4][1]) for f in range(f0, f0 + 20)]), f0 + 25, {'FP_LEVEL': 'fn6'})
check('fn6_rider_shield_damage_5', any(s == 'shield' and d == 5 for _, s, d in R2['dmg']), str(R2['dmg'][-3:]))
R = run(hp_refill(['300 BOSSHP 0', '370 BOSSHP 0']), 700, {'FP_LEVEL': 'fn6'})
lz = first(R['fp'], lambda r: r['fcD'] == 4)
check('fn6_laser_after_first_hit', lz is not None and {200, 169} <= {s for _, s in R['sid']}, str(lz and lz['f']))
check('fn6_laser_damage_10', any(s == 'laser' and d == 10 for _, s, d in R['dmg']), str(R['dmg']))
lzoff = first(R['fp'], lambda r: lz and r['f'] > lz['f'] and r['fcD'] == 5)
check('fn6_laser_200_frames', lz and lzoff and 195 <= lzoff['f'] - lz['f'] <= 205, str(lz and lzoff and lzoff['f'] - lz['f']))   # 8583

# ---------------------------------------------------------------- HP pools, the jaw, mode 2, defeat, results (BOSSHP, RV T-G2)
R = run(kill_script(), 2900, {'FP_LEVEL': 'fn6'})
fp = R['fp']
pools = []
for r in fp:
    if r['hC'] == 100 and r['hA'] > 1 and (not pools or pools[-1] != r['hA']): pools.append(r['hA'])
check('fn6_hp_pools', pools[:3] == [60, 70, 80] and 90 in pools, str(pools))   # 60 (ctor), 60, 70, 80, 90
ex = first(fp, lambda r: r['hA'] == 1 and r['hC'] == 30)
op = first(fp, lambda r: r['hA'] == 0 and r['jaw'] < 0)
check('fn6_jaw_opens_then_exposed', ex is not None and op is not None and ex['jaw'] == -60 and 14 <= ex['f'] - op['f'] <= 16,
      str(ex and op and (ex['f'] - op['f'], ex['jaw'])))   # 8638 / 8639: 15 frames, A 1, C 30
m2 = first(fp, lambda r: r['m2'] == 1)
check('fn6_mode2_at_f3', m2 is not None and m2['hF'] == 3 and {165, 166} <= {s for _, s in R['sid']}, str(m2 and m2['hF']))
check('fn6_jaw_destroyed_f4', 257 in {s for _, s in R['sid']})
d7 = first(fp, lambda r: r['hF'] >= 7)
check('fn6_defeat_at_f7_post', d7 is not None and d7['post'] == 1, str(d7 and d7['f']))
tm = [r['tm'] for r in R['f'] if d7 and r['act'] == 6 and d7['f'] + 5 <= r['f'] <= d7['f'] + 60]
check('fn6_defeat_timer_stops', tm and len(set(tm)) == 1, str(tm[:1] + tm[-1:]))
check('fn6_defeat_music_stop', any(l.startswith('music stop') for l in R['music']))
fl = first(fp, lambda r: r['hdC'] == 1)
res = first(fp, lambda r: r['res'] == 1)
check('fn6_white_flash', fl is not None and d7 is not None and 450 <= fl['f'] - d7['f'] <= 560, str(fl and d7 and fl['f'] - d7['f']))   # RV f2474 - f1961 = 513
check('fn6_results_after_defeat', res is not None and d7 is not None and abs(res['f'] - d7['f'] - 883) <= 20, str(res and d7 and res['f'] - d7['f']))   # RV 883
check('fn6_results_music_voice', any('m_clear' in l for l in R['music']) and 'voice teamlilac_madeit' in R['out'])
check('fn6_results_screen', any(r['res'] for r in R['f']))

# ---------------------------------------------------------------- the partner's assist (fn_assist.c; tests/test_fn_assist.py)
# Carol arrives at f1198 (I 1200); a projectile may knock her out on a pass (RV al_c0: the first pass, f1127 -> the strikes on
# the next 600 cycle), so the run covers two cycles
R = run('5 SETV others E 99999\n', 2100, {'FP_LEVEL': 'fn6'})
st = [r['hA'] for r in R['fp'] if r['f'] > 1150]
check('fn6_assist_strikes', st and min(st) < 60 and 137 in {s for _, s in R['sid']}, str(st and min(st)))   # 10 strikes of 3 + |B| / 10

# ---------------------------------------------------------------- an FN6 death goes back to FN5 (spec_fn_flow 5.4, death_g4)
R = run('100 HP 0\n100 HURT 10\n', 400, {'FP_LEVEL': 'fn6'})
check('fn6_death_to_fn5', R['load'][-1:] == ['fn5'], str(R['load']))

# ================================================================ session 18: the user's FN6 report (RV = original traces t1-t6 of
# session 18, RE_NOTES "Session 18: the Panther fight fixes"); 'fpo' rows (FP_DBG_FP=3): frame, slot, born, type, x, y, st, tm, hidden
PLASMA, MISSILE, RAPID, WARN, SLINE, DEBRIS = 892, 893, 894, 895, 896, 107
def tracks(out):
    tr = {}
    for line in out.splitlines():
        t = line.split()
        if t and t[0] == 'fpo':
            f, i, born, ty, x, y, st, tm, hd = map(int, t[1:])
            tr.setdefault((i, born, ty), []).append((f, x, y, st, tm, hd))
    return tr
def of_type(tr, ty): return sorted((L for (i, b, t), L in tr.items() if t == ty), key=lambda L: L[0][0])
def peak(tr, ty):
    c = {}
    for (i, b, t), L in tr.items():
        if t == ty:
            for e in L: c[e[0]] = c.get(e[0], 0) + 1
    return max(c.values()) if c else 0

# ---- 1. projectiles (Lilac, idle, invulnerable): spawn, movement, lifetime, counts against RV t2 / t3
R = run('5 SETV others E 99999\n', 1500, {'FP_LEVEL': 'fn6', 'FP_DBG_FP': '3'})
tr = tracks(R['out'])
pl = of_type(tr, PLASMA)
vol = []
for L in pl:
    if not vol or L[0][0] - vol[-1][-1] > 30: vol.append([])
    vol[-1].append(L[0][0])
check('s18_plasma_12_per_volley_every_5', vol and all(len(v) == 12 and all(b - a == 5 for a, b in zip(v, v[1:])) for v in vol if v[0] < 1400), str([len(v) for v in vol]))   # RV T-G1 / t2
check('s18_plasma_lifetime_rv', pl and all(3 <= len(L) <= 16 for L in pl if L[-1][0] < 1490), str(sorted({len(L) for L in pl})))   # RV t2: 8-13 frames (floor), sooner on the player (col_18_34)
v0 = [(L[1][1] - L[0][1], L[1][2] - L[0][2]) for L in pl[:5]]
check('s18_plasma_speed_aim_rv', v0 == [(10, 6), (11, 4), (11, 4), (10, 6), (10, 6)], str(v0))   # RV t2 f24-44: identical steps
fp = {r['f']: r for r in R['fp']}
ends = [(L[-1][0], L[-1][1], L[-1][2]) for L in pl if L[-1][0] < 1390]
pf = {int(r['f']): r for r in R['f']}
bad = [(f, x, y) for f, x, y in ends if not (abs(x - (fp[f]['K'] + 260)) < 140 and (y >= 265 or (abs(x - pf[f]['x']) <= 40 and y >= pf[f]['y'] - 14 - 70)))]
check('s18_plasma_homes_on_target_floor', ends and not bad, str(bad[:3]))   # 8657 / 8658: re-aimed at (K + 260, L + 140), bursts on the floor / the player
ra = of_type(tr, RAPID)
check('s18_rapid_30_rise_fall_16', len(ra) == 30 and all(L[1][2] - L[0][2] == -16 and L[-1][2] - L[-2][2] == 16 for L in ra), str(len(ra)))
off = [next((L[k][1] - L[k - 1][1] for k in range(1, len(L)) if L[k][1] != L[k - 1][1]), 0) for L in ra]   # the x shift at the fall (8652)
check('s18_rapid_land_offsets_C_E_plus_2', off == [16 * (k + 2) * (1 if k % 2 else -1) for k in range(30)], str(off[:6]))   # RE disasm 0x11f73ad, RV t2
check('s18_rapid_lifetime', all(100 <= len(L) <= 125 for L in ra), str(sorted({len(L) for L in ra})[:3]))   # RV t2 111-117 (floor) ; off-floor ones removed below the level
wa = of_type(tr, WARN)
check('s18_warnings_31_frames', len(wa) == 30 and all(len(L) == 31 for L in wa) and peak(tr, WARN) <= 11, '%d %s %d' % (len(wa), sorted({len(L) for L in wa}), peak(tr, WARN)))   # RE 8658 / 8061, RV t2
mi = of_type(tr, MISSILE)
check('s18_missiles_11_one_per_10', len(mi) == 11 and [L[0][0] - mi[0][0][0] for L in mi] == list(range(0, 110, 10)), str([L[0][0] for L in mi]))
mv0 = [(L[1][1] - L[0][1], L[1][2] - L[0][2]) for L in mi[:6]]
check('s18_missile_directions_rv', mv0 == [(7, -5), (6, -7), (4, -8), (3, -8), (1, -9), (7, -4)], str(mv0))   # RV t3: dir 3 first (8663 at birth)
bl = [L for L in mi if any(e[3] == 1 for e in L)]
drift = [e for L in bl for a, e in zip(L, L[1:]) if a[3] == 1 and e[3] == 1 and e[1] - a[1] != -10]
check('s18_missile_blast_rides_the_belt', bl and not drift and all(e[5] == 1 for L in bl for e in L if e[3] == 1), str(drift[:2]))   # foreach_m_2041: x - 10, hidden
blt = [sum(1 for e in L if e[3] == 1) for L in bl if L[-1][0] < 1490]
check('s18_missile_blast_45_frames', blt and all(44 <= n <= 46 for n in blt if n), str(blt))
check('s18_runtime_counts_capped', peak(tr, PLASMA) <= 3 and peak(tr, RAPID) <= 30 and peak(tr, MISSILE) <= 11, '%d %d %d' % (peak(tr, PLASMA), peak(tr, RAPID), peak(tr, MISSILE)))

# Milla (plane): the shots home on the mirrored target and burst on her or on the target (col_18_34 / col_16_34), none circle
R = run('5 SETV others E 99999\n', 400, {'FP_LEVEL': 'fn6', 'FP_CHAR': '4', 'FP_DBG_FP': '3'})
tr = tracks(R['out']); pl = of_type(tr, PLASMA)
check('s18_milla_plasma_bursts_rv_t3', len(pl) >= 12 and all(len(L) <= 30 for L in pl) and sum(1 for L in pl if len(L) <= 13) >= 8 and peak(tr, PLASMA) <= 5,
      str(sorted(len(L) for L in pl)[:14]))   # RV t3: n 11-12 (9 of 12), the others leave at x ~674 (n 24)
R = run('5 SETV others E 99999\n100 380 L\n', 400, {'FP_LEVEL': 'fn6', 'FP_CHAR': '4'})
check('milla_s18_left_edge', min(r['x'] for r in R['f'] if r['f'] > 50) >= 214 + 16, str(min(r['x'] for r in R['f'] if r['f'] > 50)))

# ---- 2. the standing pose (mode 2) against RV t4: I 90 -> head level at L - 45, cannon / jaw unrotated, hip (wheel) on the floor,
#         foot FR at 333, K follows the player at 2 px (8620 / 8621)
R = run(kill_script(), 1000, {'FP_LEVEL': 'fn6', 'FP_DBG_FP': '2'})
fp = {r['f']: r for r in R['fp']}
m2f = [f for f in sorted(fp) if fp[f]['m2'] == 1 and fp[f]['I'] == 90]
check('s18_mode2_reached', len(m2f) > 20, str(len(m2f)))
if m2f:
    f = m2f[10]; P = R['fpp'][f]; r = fp[f]
    hd, jw, fc, sh, ff, hp = P[0], P[1], P[2], P[5], P[12], P[7]
    check('s18_mode2_head_level_at_L_minus_45', hd[1] == int(r['L'] - 45) and hd[2] == 0, str(hd))       # RV t4: y 75 at L 120.5, angle 0
    check('s18_mode2_cannon_jaw_unrotated', (fc[0] - hd[0], fc[1] - hd[1]) == (56, 48) and (jw[0] - hd[0], jw[1] - hd[1]) == (71, 21), str((fc, jw, hd)))   # RV t4
    check('s18_mode2_hip_on_floor', 240 <= hp[1] <= 242, str(hp))                                           # RV t4: 240-242
    check('s18_mode2_foot_fr_pose', (ff[0] - sh[0], ff[1] - sh[1]) in ((6, 69), (7, 70), (7, 69), (6, 70)) and abs((ff[2] % 360) - 333) < 0.5, str((ff, sh)))   # RV t4 (8613)
    wl = [l for l in R['out'].splitlines() if l.startswith('fpp %d ' % f)][0].split(' w:')[1].split(',')
    check('s18_mode2_wheel_at_hip', (float(wl[0]), float(wl[1])) == (hp[0], hp[1]) and wl[2] == '1' and float(wl[3]) == 1.0, str((wl, hp)))   # 8618; x scale 1 (RV)
    ks = [fp[g]['K'] for g in m2f]
    check('s18_mode2_K_follows_player_2px', all(abs(b - a) in (0, 2) for a, b in zip(ks, ks[1:])) and abs(ks[-1] - (fp[m2f[-1]]['px'] - 180)) <= 42, str(ks[:1] + ks[-1:]))

# ---- 3. stray objects: jaw / post-boss debris fall away, nothing hangs into the results; the wreck is on the floor
R = run(kill_script(), 2300, {'FP_LEVEL': 'fn6', 'FP_DBG_FP': '3', 'FP_TRACK': '%d:-1' % DEBRIS})
res = first(R['fp'], lambda r: r['res'] == 1)
deb = [l.split() for l in R['out'].splitlines() if l.startswith('ob ') and l.split()[2] == str(DEBRIS)]
late = [d for d in deb if res and int(d[1]) >= res['f']]
check('s18_debris_falls_and_is_removed', deb and not late, '%d rows, %d at the results' % (len(deb), len(late)))
tr = tracks(R['out'])
alive = [(t, L[-1][0]) for (i, b, t), L in tr.items() if res and L[-1][0] >= res['f'] + 60 and t in (PLASMA, MISSILE, RAPID, WARN, SLINE)]
check('s18_no_projectile_into_results', res is not None and not alive, str(alive[:3]))
P = R['fpp'] if R['fpp'] else {}
R2 = run(kill_script(), 2300, {'FP_LEVEL': 'fn6', 'FP_DBG_FP': '2'})
fin = max(R2['fpp']) if R2['fpp'] else None
if fin:
    W = R2['fpp'][fin]
    check('s18_wreck_on_floor', (W[12][0], W[12][1]) == (283, 280) and (W[7][0], W[7][1]) == (386, 282) and (W[10][0], W[10][1]) == (592, 282)
          and (W[9][0], W[9][1]) == (423, 281) and (W[6][0], W[6][1]) == (260, 281), str({k: W[k][:2] for k in (6, 7, 9, 10, 12)}))   # RE 8769

# ---- 4. the left edge: 8512 (M != 9: X >= view_x + 16) before the belt; RV t1: X holds 224.85 / B 4.41 walking left, then rises
for ch, nm in (('0', 'lilac'), ('2', 'carol')):
    R = run('5 SETV others E 99999\n200 800 L\n', 900, {'FP_LEVEL': 'fn6', 'FP_CHAR': ch})
    xs = [r['x'] for r in R['f'] if r['f'] >= 150]
    check('%s_s18_left_edge_holding_left' % nm, min(xs) >= 214 + 6, str(min(xs)))
    if ch == '0':
        r = {int(r['f']): r for r in R['f']}
        check('s18_left_edge_rv_t1', abs(r[300]['x'] - 224.85) < 0.05 and abs(r[300]['gsp'] - 4.4075) < 0.01 and abs(r[802]['x'] - 225.155) < 0.05,
              '%.3f %.4f %.3f' % (r[300]['x'], r[300]['gsp'], r[802]['x']))   # RV t1 f300 / f804 (input shift 2)
        check('s18_left_edge_released_moves_back', r[899]['x'] > r[801]['x'] + 5, '%.2f -> %.2f' % (r[801]['x'], r[899]['x']))
R = run(hp_refill(['200 900 L'] + ['%d HURT %d' % (f, 20 if k % 2 == 0 else -20) for k, f in enumerate(range(300, 860, 70))], 900), 900, {'FP_LEVEL': 'fn6'})
xs = [r['x'] for r in R['f'] if r['f'] >= 150 and r['act'] == 6]
check('s18_knockback_run_stays_in_fn6', R['load'][-1:] == ['fn6'], str(R['load']))
check('s18_left_edge_with_knockback', min(xs) >= 214 + 6, str(min(xs)))
R = run('5 SETV others E 99999\n100 400 R\n150 HURT -20\n', 400, {'FP_LEVEL': 'fn6'})
xs = {int(r['f']): r['x'] for r in R['f']}
check('s18_holding_right_runs_forward', xs[399] > 560 and min(xs[f] for f in range(150, 400)) >= 220, '%.1f' % xs[399])   # 8517: + 0.2 while right is held
# the walk-in from FN5 (RV t5): B + 0.2 a frame to 15 from 9.6-10.2, X ~ 500 when the belt speed settles at 10.08
R = run('5 TP 12000 210\n20 600 R\n', 760, {'FP_LEVEL': 'fn5'})
a6 = [r for r in R['f'] if r['act'] == 6]
if a6:
    b = [r['gsp'] for r in a6[:40]]
    check('s18_walkin_b_rises_to_15', 9.5 <= b[2] <= 10.6 and max(b) == 15 and abs(a6[60]['gsp'] - 10.081) < 0.01, str(b[:4] + [max(b), a6[60]['gsp']]))
    check('s18_walkin_x_like_rv', 490 <= a6[45]['x'] <= 515 and min(r['x'] for r in a6[1:]) >= 400, '%.1f' % a6[45]['x'])   # RV t5 498.6 (port + 9: INFERRED start frame)

# ---- 5 / 6. Normal vs Hard (global 12: fe_difficulty, host FP_DIFF): no FN6 event reads g12 (RE 8487-8840); only the shared damage
#             multiplier differs (2414: Hard 2 half-petals per 5). FN3: Serpentine's Hard-only missile (7828) when the player is 300+ away
hits = {}
for d in ('1', '0'):
    R = run('5 SETV others E 0\n', 200, {'FP_LEVEL': 'fn6', 'FP_DIFF': d})
    hp = [r['hp'] for r in R['f']]
    dm = [s for _, s, n in R['dmg'] if s == 'plasma']
    hits[d] = (dm[:1], 14 - min(hp))
    fpd = [(r['fcD'], r['fcE']) for r in R['fp'][:150]]
    hits[d + 'tl'] = fpd
check('s18_fn6_same_attacks_normal_hard', hits['1tl'] == hits['0tl'])
check('s18_fn6_plasma_hard_doubles_damage', hits['1'][0] == ['plasma'] and hits['0'][0] == ['plasma'] and hits['0'][1] == 2 * hits['1'][1], str((hits['1'], hits['0'])))
def fn3(script, frames, d):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE')]
    e.update({'FP_LEVEL': 'fn3', 'FP_DIFF': d, 'FP_TRACK': '820:-1,829:-1', 'FP_TRACE_SID': '1'})
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e).stdout
    os.unlink(name)
    serp, nmis, rows, sid = {}, 0, {}, []
    for line in out.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'ob':
            dd = {t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2) if t[i] != 'fl'}
            if t[2] == '820': serp[int(t[1])] = dd
            else: nmis += 1
        elif t[0] == 'f': rows[int(t[1])] = {t[i]: num(t[i + 1]) for i in range(0, len(t) - 1, 2)}
        elif t[0] == 'sid': sid.append(int(t[1]))
    return serp, nmis, rows, sid
base = '5 TP 10400 150;3 SETV others E 99999;40 52 R'
serp, nmis, rows, sid = fn3(base, 400, '1')
fs = next((f for f in sorted(serp) if serp[f]['a0'] == 2), None)            # the first shot (state 2): the player 350 px away then
if fs:
    sx = serp[fs]['x']; tx = sx - 350 if sx - 350 > 9950 else sx + 350
    tp = base + ';%d TP %d %d' % (fs - 3, tx, rows[fs]['y'] - 14)
    sn, nn, _, sidn = fn3(tp, fs + 120, '1')
    sh, nh, _, sidh = fn3(tp, fs + 120, '0')
    check('s18_fn3_hard_only_missile', any(s['a0'] == 4 for s in sh.values()) and nh > 0 and 195 in sidh and not any(s['a0'] == 4 for s in sn.values()) and nn == 0,
          'hard %d rows, normal %d' % (nh, nn))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
