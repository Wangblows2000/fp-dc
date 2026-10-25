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

# ---------------------------------------------------------------- the partner's assist (U-G3, reduced: INFERRED)
R = run('5 SETV others E 99999\n', 1300, {'FP_LEVEL': 'fn6'})
st = [r['hA'] for r in R['fp'] if r['f'] > 1150]
check('fn6_assist_strikes', st and min(st) < 60 and 137 in {s for _, s in R['sid']}, str(st and min(st)))   # I 1200 -> 10 strikes of 4

# ---------------------------------------------------------------- an FN6 death goes back to FN5 (spec_fn_flow 5.4, death_g4)
R = run('100 HP 0\n100 HURT 10\n', 400, {'FP_LEVEL': 'fn6'})
check('fn6_death_to_fn5', R['load'][-1:] == ['fn5'], str(R['load']))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
