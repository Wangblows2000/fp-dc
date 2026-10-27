#!/usr/bin/env python3
"""Host tests for the FN6 partner assist and the plane (session 18; src/game/fn_assist.c; RE events 8668-8721 / 8764-8785,
spec_fn_bosses G.10, RE_NOTES "Session 18: the partner assist and the plane"). RV numbers are the session-18 original traces
(tools/orig, FRAME=34) shifted by +118 frames to the port's frame count (the first plasma volley: tg1 f117 = port f~120).
  - the plane (Torque, Milla in the back) flies in, stops, Milla holds up and drops the life-petal box, which breaks on the
    treadmill (6 petals), the plane leaves, crosses the far background and comes back only when the player is low on petals;
  - Lilac: Carol on her bike drives in along the floor (stays on the ground), jumps at the hitbox, 10 strikes at the RV cadence,
    lands, drives out; 600-frame cycle; Carol: Lilac runs in, jumps (D -5, sound 288), invisible shields every 6 frames;
    Milla: Lilac first (I 60), then Carol, in turn; no deliveries (the plane is her platform);
  - after the defeat the plane keeps flying until the white flash, then it and the helpers are gone; the closing scene's cast
    (Lilac, Carol, Torque and Milla from the plane) at the RV spots; the results at defeat + 883.
usage: test_fn_assist.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')


def num(v):
    try: return float(v)
    except ValueError: return v


def run(script, frames, ch):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_DBG_FP': '1', 'FP_LEVEL': 'fn6', 'FP_CHAR': ch})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    R = {'fpa': [], 'ev': [], 'cast': [], 'fp': [], 'sid': [], 'out': r.stdout + r.stderr}
    fno = 0
    for line in R['out'].splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': fno = int(t[1]) if len(t) > 1 and t[1].lstrip('-').isdigit() else fno + 1
        elif t[0] == 'fpa':   # fpa F pl X Y G g dir d dA a dB b off dir X Y F f I i C c D d E e grp xy mon m
            R['fpa'].append({'f': int(t[1]), 'px': float(t[3]), 'py': float(t[4]), 'G': float(t[6]), 'pdir': int(t[8]), 'dA': float(t[10]),
                             'odir': int(t[14]), 'ox': float(t[15]), 'oy': float(t[16]), 'F': float(t[18]), 'I': float(t[20]),
                             'C': float(t[22]), 'D': float(t[24]), 'E': float(t[26]), 'grp': t[28], 'mon': int(t[30])})
        elif t[0] == 'fpas': R['ev'].append((int(t[1]), t[2], [num(v) for v in t[3:]]))
        elif t[0] == 'fpcast': R['cast'].append({'f': int(t[1]), 'D': int(t[3]), 'L': float(t[5]), 'C': float(t[8]), 'M': float(t[11]), 'T': float(t[14])})
        elif t[0] == 'fp': d = {t[i]: num(t[i + 1]) for i in range(2, len(t) - 1, 2)}; d['f'] = int(t[1]); R['fp'].append(d)
        elif t[0] == 'sid': R['sid'].append((fno, int(t[1])))
    return R


fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1


def first(rows, pred):
    return next((r for r in rows if pred(r)), None)


def evs(R, kind): return [e for e in R['ev'] if e[1] == kind]
def sids(R, s): return [f for f, i in R['sid'] if i == s]
def near(v, ref, tol): return v is not None and abs(v - ref) <= tol


INV = '5 SETV others E 99999\n'
# ---------------------------------------------------------------- Lilac: the plane's delivery, Carol on the bike
R = run(INV, 2700, '0')
fa = R['fpa']
check('lilac_plane_far_left_at_start', fa and fa[0]['px'] < -1900 and fa[0]['G'] == 0, str(fa[:1] and fa[0]['px']))   # A = view x - 2000 (8703)
stop = first(fa, lambda r: r['dA'] == 1)
check('lilac_milla_holds_up_the_box_f902', near(stop and stop['f'], 902, 6), str(stop and stop['f']))   # RV f784 + 118
check('lilac_plane_stopped_in_view', stop and 464 < stop['px'] < 640, str(stop and stop['px']))       # 8706: X > LimitLeft + 240
check('lilac_box_sound_241_f962', near((sids(R, 241) or [None])[0], 962, 4), str(sids(R, 241)))     # RV f844
dr = evs(R, 'drop')
check('lilac_box_dropped_f1024', len(dr) == 1 and near(dr[0][0], 1024, 4), str(dr))                   # RV f906
br = [f for f in sids(R, 12) if f > 1024]
check('lilac_box_breaks_on_the_floor_f1052', br and near(br[0], 1052, 6), str(br[:2]))                # RV f934
check('lilac_one_box_at_a_time', max(r['mon'] for r in fa) <= 1)
lv = first(fa, lambda r: r['f'] > 1024 and r['G'] == 1)
check('lilac_plane_leaves_f1065', near(lv and lv['f'], 1065, 6), str(lv and lv['f']))               # RV f947 (8716)
far = [r for r in fa if r['G'] == 2]
check('lilac_plane_far_pass_dir1_capped', far and all(r['pdir'] == 1 for r in far) and max(r['px'] for r in far) <= 464 + 33,
      str(far and max(r['px'] for r in far)))                                                          # 8709: A <= 464, direction 1
check('lilac_plane_stays_far_at_full_petals', far and far[-1]['f'] >= 2690)                         # 8710: petals 14
arr = evs(R, 'arrive')
check('lilac_carol_arrives_f1198', arr and arr[0][2][0] == 'carol' and near(arr[0][0], 1198, 6), str(arr[:1]))   # RV f1077 + 118 (I 1200)
check('lilac_carol_voice_once', len(sids(R, 126)) == 1 and near(sids(R, 126)[0], 1196, 6), str(sids(R, 126)))    # 8670
check('lilac_carol_600_cycle', len(arr) >= 2 and arr[1][0] - arr[0][0] == 598, str([a[0] for a in arr]))         # RV 1077 -> 1675
jp = evs(R, 'jump')
# a projectile knocks the helper back (8684: RV al_c0 f1127-1149, Carol's first pass: F 2, D -3.6 -> 5.2 in 22 frames, top y 248,
# then she drives out and comes back on the next 600 cycle; the jump is on a pass without a knock)
kn = evs(R, 'knocked')
if kn:
    k0 = kn[0][0]; ks = [r for r in fa if r['f'] >= k0 and r['F'] == 2]; ks = [r for r in ks if r['f'] - k0 < 40]
    back = first(fa, lambda r: r['f'] > k0 and r['F'] == 0)
    check('lilac_carol_knocked_rv', ks and abs(ks[0]['D'] + 3.6) < 0.01 and back and back['f'] - k0 == 22 and abs(back['D'] - 5.2) < 0.01
          and min(r['oy'] for r in ks) == 248, str(ks[:1] and back and (ks[0]['D'], back['f'] - k0, back['D'], min(r['oy'] for r in ks))))
if jp:
    j0 = jp[0][0]
    a0 = max(a[0] for a in arr if a[0] <= j0)                                                        # the pass that jumps
    drive = [r for r in fa if a0 < r['f'] <= j0]
    g0 = first(drive, lambda r: r['oy'] == 267)                                                     # (8672 B = Y(o): a knock just
    drive = [r for r in drive if g0 and r['f'] >= g0['f']]                                          #  before the arrival falls back, 8677)
    check('lilac_carol_drives_on_the_ground', drive and all(r['oy'] == 267 and r['odir'] == 1 for r in drive),
          str(sorted({r['oy'] for r in drive})))
    xs = [r['ox'] for r in drive if r['f'] > g0['f'] + 2]
    check('lilac_carol_drives_right_5', len(xs) > 5 and xs[3] - xs[2] == 5, str(xs[:5]))
    air = [r for r in fa if j0 <= r['f'] < j0 + 40]
    check('lilac_carol_jumps_d_minus_8', air and min(r['oy'] for r in air) < 200 and abs(air[0]['D'] - (-7.7)) < 0.01, str(air[:1] and air[0]['D']))   # 8678 -8, 8679 + 0.3 (RV -7.7)
    st = [e[0] for e in evs(R, 'strike') if j0 <= e[0] <= j0 + 60]
    gaps = [b - a for a, b in zip(st, st[1:])]
    check('lilac_carol_10_strikes_rv_cadence', len(st) == 10 and gaps == [6, 4, 6, 6, 4, 6, 6, 4, 6] and st[0] - j0 == 3, str((st[:1], gaps)))   # RV 1759 / 1762..1810
    ld = first(evs(R, 'land'), lambda e: e[0] > j0)
    check('lilac_carol_lands_53_frames', ld and ld[0] - j0 == 53, str(ld and ld[0] - j0))             # RV 1759 -> 1812
    back = first(fa, lambda r: ld and r['f'] == ld[0] + 1)
    check('lilac_carol_leaves_left', back and back['C'] < 0 and back['E'] >= 29, str(back and (back['C'], back['E'])))   # 8683 C -3 E 30 (or knocked -4)
# damage of a strike: the shared 2740 with the shield's A 1 (3 + |B| / 10; RV 4.019 with B 10.19)
hA = [(r['f'], r['hA']) for r in R['fp'] if 'hA' in r]
drops = [round(a - b, 3) for (f0, a), (f1, b) in zip(hA, hA[1:]) if a > b and any(abs(f1 - s) <= 3 for s in [e[0] for e in evs(R, 'strike')])]
check('lilac_carol_strike_damage_3_plus_b10', drops and all(3.0 <= d <= 4.2 for d in drops), str(drops[:4]))
# petals: the box's six petals ride the treadmill to the player (low petals so they can be collected)
R4 = run(INV + '1000 HP 6\n', 1250, '0')
p37 = [f for f in sids(R4, 37) if f > 1050]
check('lilac_box_six_petals_collected', len(p37) == 6, str(p37))

# the plane returns when the petals are low (8710 <= 6) and delivers again; never two planes / boxes
R = run(INV + ''.join('%d HP 6\n' % f for f in range(1100, 2700, 20)), 2700, '0')
dr = evs(R, 'drop')
g3 = first(R['fpa'], lambda r: r['G'] == 3)
check('lilac_plane_returns_when_low', g3 is not None and len(dr) >= 2, str((g3 and g3['f'], [d[0] for d in dr])))
check('lilac_plane_returns_from_the_left', g3 is not None and any(r['G'] == 0 and r['f'] > g3['f'] and r['px'] < 214 for r in R['fpa']))   # 8712: A = view - 200
check('lilac_one_box_at_a_time_2', max(r['mon'] for r in R['fpa']) <= 1 and len(sids(R, 241)) == len(dr), str((len(sids(R, 241)), len(dr))))

# ---------------------------------------------------------------- Carol: Lilac runs in, jumps (288), invisible shields
R = run(INV, 2700, '2')
arr = evs(R, 'arrive')
check('carol_lilac_arrives_f1198', arr and arr[0][2][0] == 'lilac' and near(arr[0][0], 1198, 6), str(arr[:1]))
check('carol_no_arrival_voice', not sids(R, 126), str(sids(R, 126)))                                  # 8670 / 8671: Lilac / Milla only
jp = evs(R, 'jump')
check('carol_lilac_jump_sound_288', jp and any(abs(f - jp[0][0]) <= 2 for f in sids(R, 288)), str((jp[:1], sids(R, 288)[:2])))
if jp:
    j0 = jp[0][0]
    a0 = max(a[0] for a in arr if a[0] <= j0)                                                        # the pass that jumps (knocks: 8698)
    pre = [r for r in R['fpa'] if a0 < r['f'] <= j0]
    check('carol_lilac_runs_on_the_ground', pre and all(r['oy'] == 267 and r['odir'] == 0 for r in pre))
    ld = first(evs(R, 'land'), lambda e: e[0] > j0)
    check('carol_lilac_floats_100_frames', ld and 95 <= ld[0] - j0 <= 102, str(ld and ld[0] - j0))   # D -5, + 0.1 (RV 2160 -> 2261)
hA = [(r['f'], r['hA']) for r in R['fp'] if 'hA' in r]
drops = [round(a - b, 3) for (f0, a), (f1, b) in zip(hA, hA[1:]) if a > b]
check('carol_lilac_shield_damage_2_plus_b10', drops and all(2.0 <= d <= 3.2 for d in drops[:3]), str(drops[:4]))   # RV 3.003 (A 2)
check('carol_lilac_rearrives_from_far_left', len(arr) >= 2 and arr[1][2][1] < -500, str(arr[1:2]))  # A = X(o): no -50 clamp (RV -2158)

# ---------------------------------------------------------------- Milla: Lilac first (I 60), then Carol; the plane is her platform
R = run(INV, 1000, '4')
arr = evs(R, 'arrive')
check('milla_lilac_first_f58', arr and arr[0][2][0] == 'lilac' and near(arr[0][0], 58, 4), str(arr[:1]))       # RV act 58
jp = evs(R, 'jump')
check('milla_lilac_jumps_f138', jp and near(jp[0][0], 138, 4), str(jp[:1]))                                   # RV 288 at act 137
check('milla_carol_second_f656', len(arr) >= 2 and arr[1][2][0] == 'carol' and near(arr[1][0], 656, 4), str(arr[1:2]))   # RV act 656
check('milla_carol_voice', len(sids(R, 126)) == 1 and near(sids(R, 126)[0], 654, 4), str(sids(R, 126)))      # 8671 (RV act 655)
check('milla_no_delivery', not sids(R, 241) and not evs(R, 'drop'))                                          # 8528: 0x227c0 / c1 off
check('milla_on_the_plane', R['fp'] and all(r['pl'] == 1 for r in R['fp']))

# ---------------------------------------------------------------- defeat: the plane until the flash, the cast, the results
for ch in ('0', '2', '4'):
    R = run(INV + ''.join('%d BOSSHP 0\n' % (300 + k * 70) for k in range(30)), 2050, ch)
    fp = R['fp']
    d7 = first(fp, lambda r: r['hF'] >= 7)
    fl = first(fp, lambda r: r['hdC'] == 1)
    after = [r for r in R['fpa'] if d7 and r['f'] > d7['f']]
    check('ch%s_plane_after_defeat_until_the_flash' % ch, d7 and fl and after and after[-1]['f'] >= fl['f'] and after[-1]['f'] <= fl['f'] + 30,
          str((d7 and d7['f'], fl and fl['f'], after[-1:] and after[-1]['f'])))
    c = R['cast']
    last = c[-1] if c else None
    check('ch%s_cast_lilac_carol_torque_milla_rv' % ch, last and last['L'] == 483 and last['C'] == 530 and last['T'] == 396 and last['M'] == 435,
          str(last))                                                                                  # RV 483 / 530 / 396 / 435
    walk = first(c, lambda r: r['D'] == 131)
    check('ch%s_crew_walks_in_after_lilac' % ch, walk and walk['L'] == 483 and walk['T'] <= 124 and walk['M'] <= 125, str(walk))   # from 120 (8769)
    res = first(fp, lambda r: r['res'] == 1)
    check('ch%s_results_after_defeat_883' % ch, res and d7 and abs(res['f'] - d7['f'] - 883) <= 20, str(res and d7 and res['f'] - d7['f']))

# ---------------------------------------------------------------- session 20: after the flash (user report: Milla, Serpentine and the camera)
# RV build/orig_traces/fn6_s20: m_kill (Milla, defeat in mode 1): CAM.B 186.6 (Milla on the plane's platform) -> 191 at MV.M 50 (8764:
# MV.Y = 260), the head at x -2; m2_kill (Lilac, defeat during the wheel stance, mode 2): 8749 keeps the front cannon's D at -2 every
# frame, so K stays put, then 8759 takes it to 0 (head x -29): the panther and its rider leave the view for good.
def run_post(ch):
    script = INV + ''.join('%d BOSSHP 0\n' % (300 + k * 70) for k in range(30))
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_DBG_FP': '1', 'FP_LEVEL': 'fn6', 'FP_CHAR': ch, 'FP_TRACK': '861:-1,865:-1'})
    r = subprocess.run([BIN, DATA, '2050', name], capture_output=True, text=True, env=e)
    os.unlink(name)
    cam, obj, hdc, m2 = {}, {}, {}, {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f' and len(t) > 2 and t[1].isdigit():
            d = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}; cam[int(t[1])] = (float(d['cx']), float(d['cy']))
        elif t[0] == 'ob': obj[(int(t[1]), int(t[2]))] = float(t[5])
        elif t[0] == 'fp': d = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}; hdc[int(t[1])] = int(float(d['hdC'])); m2[int(t[1])] = int(d['m2'])
    return cam, obj, hdc, m2

for ch in ('0', '2', '4'):
    cam, obj, hdc, m2 = run_post(ch)
    scene = [f for f in sorted(hdc) if hdc[f] == 2 and f in cam]
    check('ch%s_s20_closing_camera_on_the_floor' % ch, scene and all(cam[f][1] == 71 for f in scene[30:]),
          str(sorted(set(cam[f][1] for f in scene[30:]))))
    off = [f for f in scene if obj.get((f, 861), 1e9) > cam[f][0] - 60 or obj.get((f, 865), 1e9) > cam[f][0] - 60]
    check('ch%s_s20_panther_and_rider_gone_after_flash' % ch, scene and not off,
          'kill in mode 2: %d, visible frames %d (first %s)' % (max(m2.values()), len(off), off[:1]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
