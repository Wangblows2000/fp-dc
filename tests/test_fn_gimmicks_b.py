#!/usr/bin/env python3
"""Host tests for the Fortune Night acts 4-5 gimmicks (src/game/fn_gimb.c, build/re/spec_fn_gimmicks_b.md): escalators and
their wall plate, clotheslines, trolleys, scissor lifts (RV fn4_lift), LevelMovingPlatform2_1976, turning springs, parasols,
water platforms, air balloons (RV fn4_balloon), the keyboard and the secret melody (RV fn4_keys), restrooms, the A5 disco
dance, pole pods and the carousel. Integration checks of the recovered rules (RV items name their original trace).
usage: test_fn_gimmicks_b.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
# engine types (src/fp.h, Fortune Night acts 4-5 block) and states
ESCFRONT, ESCSTEP, ROPE, TBAR, TSENS, LIFT, STILT, MPLAT2, TSPRING, AIRBALLOON, IBALLOON, KEY, DISCOLIGHT, RESTROOM, CAROUSEL, PODDOOR = \
    680, 681, 684, 686, 687, 690, 691, 692, 693, 694, 695, 696, 697, 700, 702, 705
WPLATE = 315
ST_HANG, ST_HELD, ST_SIT, ST_DANCE = 11, 28, 30, 31

def run(level, ch, script, frames, track=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs, sids, pend = [], [], {}, []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            d = {t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)}; rows.append(d)
            for s in pend: sids.setdefault(s, []).append(int(d['f']))
            pend = []
        elif t[0] == 'sid': pend.append(int(t[1]))
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            d.update({t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2) if t[i] != 'fl'})
            d['fl'] = int(t[t.index('fl') + 1], 16)
            obs.append(d)
    return rows, obs, sids

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def of(obs, t, x0=None, f=None): return [o for o in obs if o['type'] == t and (x0 is None or o['x0'] == x0) and (f is None or o['f'] == f)]
def at(rows, f): return [r for r in rows if r['f'] == f][0]
MY = lambda r: r['y'] - 14   # the original's MV.Y (reference point 14 px above the feet)

# ---- 2. escalators (8151-8157): standing still on the A4 flight she is carried down 1 px a frame (RV fn4_esc: X -1, Y +1)
rows, _, _ = run('fn4', 0, '55 TP 690 1040', 120)
grd = [r for r in rows if r['f'] >= 55 and r['gnd'] == 1]
f0 = int(grd[0]['f']) + 2
d = [(at(rows, f + 1)['x'] - at(rows, f)['x'], at(rows, f + 1)['y'] - at(rows, f)['y']) for f in range(f0, f0 + 20)]
check('escalator_carries_down_the_flight', all(v == (-1.0, 1.0) for v in d) and all(at(rows, f)['gsp'] == 0 for f in range(f0, f0 + 20)), str(d[:4]))
# the wall plate (5677, M 11 push) reverses every belt once (8157), click 49; the steps are not solid while she stands on backdrops (8152)
rows, obs, sids = run('fn4', 0, '55 TP 1610 1180;60 120 R', 120, '%d:-1,%d:612' % (WPLATE, ESCSTEP))
st = of(obs, ESCSTEP, 612)
check('escalator_plate_reverses_belts', st[0]['a0'] == -1 and st[-1]['a0'] == 1 and len(sids.get(49, [])) == 1 and any(r['mm'] == 11 for r in rows), 'D %s -> %s, 49 at %s' % (st[0]['a0'], st[-1]['a0'], sids.get(49)))
check('escalator_steps_off_on_backdrop', st[-1]['fl'] & 1 == 1)

# ---- 4. clotheslines (8128-8133): falling into a rope grabs it 32 px below (MV.Y = 692 + 32), shimmy 3 px a frame, end sensor stops
rows, _, sids = run('fn4', 0, '55 TP 1800 712;70 300 R', 320)
h = [r for r in rows if r['mm'] == 41]
check('rope_grab_y', h and MY(h[0]) == 724 and 33 in sids, 'MV.Y %s' % (MY(h[0]) if h else None))
dx = [at(rows, f + 1)['x'] - at(rows, f)['x'] for f in range(90, 110)]
check('rope_shimmy_3px', all(v == 3 for v in dx), str(dx[:5]))
check('rope_end_sensor_stops', rows[-1]['mm'] == 41 and 2120 < rows[-1]['x'] < 2160 and at(rows, 300)['x'] == rows[-1]['x'], 'x %.0f' % rows[-1]['x'])
rows, _, _ = run('fn4', 0, '55 TP 1800 712;100 102 J', 140)
check('rope_jump_releases', at(rows, 99)['mm'] == 41 and at(rows, 103)['mm'] != 41 and at(rows, 103)['vy'] < 0)

# ---- 5. trolleys (8134-8150): grab the grip, ride to the stopper (B to 15, input locked), swing 12 degrees a frame, launch, return
rows, obs, sids = run('fn4', 0, '55 TP 1617 770;60 120 L;200 204 J', 520, '%d:1616' % TBAR)
bar = of(obs, TBAR)
C = lambda f: [o['st'] for o in bar if o['f'] == f][0]
ride = [o for o in bar if o['st'] == 1]
check('trolley_ride_accelerates_to_15', ride and max(o['a1'] for o in ride) == 15 and all(r['mm'] == 41 for r in rows if 60 <= r['f'] <= 120 and C(int(r['f'])) == 1),
      'max B %s' % (max(o['a1'] for o in ride) if ride else None))
check('trolley_input_locked_on_the_ride', all(r['face'] == 1 for r in rows if C(int(r['f'])) == 1))
sw = [o for o in bar if o['st'] == 2]
check('trolley_stops_and_swings', sw and sw[0]['x'] == 2256 and all(abs(b['a2'] - a['a2'] - 12) < 1e-6 for a, b in zip(sw, sw[1:])), 'stop x %s' % (sw[0]['x'] if sw else None))
check('trolley_whoosh_per_turn', 216 in sids and len(sids[216]) >= 2)
l3 = [o for o in bar if o['st'] == 3][0]                       # 8143 the frame she let go (8144 already turned D on)
D = l3['a2'] - 12; r201 = at(rows, l3['f'])
check('trolley_launch_18_px', abs(r201['vx'] - 18 * math.cos(math.radians(D))) < 0.01 and abs(r201['vy'] + 18 * math.sin(math.radians(D))) < 0.01, 'D %.0f vx %.2f vy %.2f' % (D, r201['vx'], r201['vy']))
check('trolley_returns_home', C(519) == 0 and [o['x'] for o in bar if o['f'] == 519][0] == 1616 and 4 in {o['st'] for o in bar}, 'C %d' % C(519))
for ch, nm in ((2, 'carol'), (4, 'milla')):
    rows, obs, _ = run('fn4', ch, '55 TP 1617 770', 200, '%d:1616' % TBAR)
    check('trolley_swing_%s' % nm, any(o['st'] == 2 for o in of(obs, TBAR)) and rows[-1]['mm'] == 41)

# ---- 6. scissor lifts (8169-8177; RV fn4_lift: 55 TP 4528 1180 onto the lift at (4464, 1200))
rows, obs, _ = run('fn4', 0, '55 TP 4528 1180', 470, ','.join('%d:%d' % (LIFT, x) for x in (4464, 4592, 4720, 5232, 5616)) + ',%d:4472' % STILT)
lifts = lambda f: sorted((o for o in of(obs, LIFT, f=f)), key=lambda o: o['x0'])
fire = [f for f in range(50, 470) if all(o['a3'] == 1.5 for o in lifts(f))]
check('lift_fires_all_five', fire and [o['st'] for o in lifts(fire[0])] == [0, 1, 1, 1, 1], 'V %s' % ([o['st'] for o in lifts(fire[0])] if fire else None))
top = min(o['y'] for o in of(obs, LIFT, 4464))
check('lift_rises_224', top == 1200 - 224 and min(MY(r) for r in rows if r['f'] > 60) < 980, 'top %s' % top)
e = [o for o in of(obs, LIFT, 4464) if o['f'] == 100][0]['a1']
s1 = [o for o in of(obs, STILT, 4472) if o['f'] == 101 and o['an'] == [o2['an'] for o2 in of(obs, STILT, 4472) if o2['f'] == 50][0]]
check('lift_stilt_angle', s1 and abs(s1[0]['a0'] - 70 * math.sin(math.radians(e + 1.5))) < 0.05, 'A %s E %s' % (s1[0]['a0'] if s1 else None, e))
rst = [f for f in range(fire[0] + 100, 470) if all(o['st'] == 0 and o['a1'] == 0 for o in lifts(f))] if fire else []
check('lift_cycle_resets_together', rst and [o['y'] for o in lifts(rst[0])] == [1200, 1200, 1200, 1200, 912], 'reset at %s' % rst[:1])

# ---- 7. LevelMovingPlatform2_1976: y = J + 8 cos E, E += 1 a frame (360-frame bob), x fixed
_, obs, _ = run('fn4', 0, '', 400, '%d:4048' % MPLAT2)
ys = [o['y'] for o in of(obs, MPLAT2)]
check('mplat2_bob_8px', min(ys) == 200 and max(ys) == 216 and of(obs, MPLAT2)[0]['x'] == 4048, '%s..%s' % (min(ys), max(ys)))

# ---- 8 / 12. turning spring (120-frame cycle, 30-frame flip, bounce G -11 face up), parasol (G -7, sound 212)
rows, obs, sids = run('fn4', 0, '90 TP 2848 790', 400, '%d:2848' % TSPRING)
sp = of(obs, TSPRING)
turns = [o['f'] for a, o in zip(sp, sp[1:]) if o['st'] != a['st']]
check('turning_spring_cycle', len(turns) >= 4 and turns[2] - turns[0] == 120 and turns[1] - turns[0] in (29, 30, 31), str(turns[:5]))
check('turning_spring_flip_180', any(o['st'] == 2 and o['dir'] in (0, 8) for o in sp) and max(abs(o['a0']) for o in sp) <= 90)
b = sids.get(5, [])
check('turning_spring_bounce_up', b and at(rows, b[0])['vy'] == -11, 'sound 5 at %s' % b[:1])
rows, _, sids = run('fn4', 0, '55 TP 1728 790', 120)
check('parasol_bounce', 212 in sids and at(rows, sids[212][0])['vy'] == -7)

# ---- 3. water platforms: one splash sound 208 per landing (8165)
rows, _, sids = run('fn4', 0, '55 TP 160 1120', 150)
check('water_platform_splash', len(sids.get(208, [])) == 1 and rows[-1]['gnd'] == 1)

# ---- 9. air balloons: +-8 px bob, 1.5 degrees a frame (RV fn4_balloon); the basket handle rides it up, rising to 4 px a frame
_, obs, _ = run('fn4', 0, '55 TP 10600 900', 300, '%d:10656' % AIRBALLOON)
ys = [o['y'] for o in of(obs, AIRBALLOON)]
check('balloon_bob', max(ys) - min(ys) in (15, 16) and abs((max(ys) + min(ys)) / 2 - 896) <= 1, '%s..%s' % (min(ys), max(ys)))
rows, obs, _ = run('fn4', 0, '55 TP 10657 952', 200, '%d:10656' % AIRBALLOON)
bl = of(obs, AIRBALLOON)
check('balloon_ride_rises', rows[-1]['mm'] == 41 and bl[-1]['y'] < 500 and min(o['a0'] for o in bl) == -4, 'y %s' % bl[-1]['y'])

# ---- 10. keyboard: notes 223 + A, A = 0..7 left to right (RV fn4_keys); the tune 1 3 4 5 3 1 8 gives sound 428 and a 1-up
mel = '55 TP 9336 1210;80 TP 9432 1210;105 TP 9480 1210;130 TP 9528 1210;155 TP 9432 1210;180 TP 9336 1210;205 TP 9672 1210'
rows, _, sids = run('fn4', 0, mel, 320)
notes = sorted((f, s) for s in range(223, 231) for f in sids.get(s, []))
check('keys_notes_left_to_right', [s - 223 for f, s in notes] == [0, 2, 3, 4, 2, 0, 7], str(notes))
check('melody_reward', 428 in sids and rows[-1]['lv'] == rows[0]['lv'] + 1, 'lives %s -> %s' % (rows[0]['lv'], rows[-1]['lv']))
rows, _, sids = run('fn4', 0, mel.replace('9528', '9576'), 320)
check('melody_wrong_note_no_reward', 428 not in sids and rows[-1]['lv'] == rows[0]['lv'])

# ---- 13. restrooms (8192-8205, 8102): the heroines are thrown out of the men's room (sound 217 at 10, B = 10 after 30)
rows, obs, sids = run('fn4', 0, '55 TP 2100 1020', 140, '%d:2104' % RESTROOM)
m39 = [r['f'] for r in rows if r['mm'] == 39]
check('restroom_men_enter', m39 and 217 in sids and max(o['fr'] for o in of(obs, RESTROOM)) >= 9, 'M39 from %s' % m39[:1])
ej = [r for r in rows if r['f'] > m39[-1]][0] if m39 else None
check('restroom_men_ejects', ej and ej['gsp'] == 10 and 5 in sids and sids[5][0] == ej['f'], str(ej and ej['gsp']))
rows, _, sids = run('fn4', 0, '55 TP 2228 1020', 200)
check('restroom_women_stays', all(r['mm'] == 39 for r in rows[80:]) and 218 not in sids)
rows, _, _ = run('fn4', 0, '55 TP 2228 1020;150 152 J', 200)
check('restroom_leave_with_jump', at(rows, 149)['mm'] == 39 and at(rows, 153)['mm'] != 39)

# ---- 11. disco light (A5 8298-8310): near the disco balls the light fades in and an idle heroine dances (M50); A4 never
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, obs, _ = run('fn5', ch, '55 TP 2950 250', 240, '%d:-1' % DISCOLIGHT)
    dl = of(obs, DISCOLIGHT)
    check('disco_dance_%s' % nm, any(r['mm'] == 50 for r in rows) and max(o['a0'] for o in dl) == 64)
rows, _, _ = run('fn5', 0, '55 TP 2950 250;200 230 R', 240)
check('disco_dance_exit', at(rows, 199)['mm'] == 50 and at(rows, 230)['mm'] != 50)
# session 18 (RE 8306-8308 + RV disco_c0 / c2 / c4, set_frame 33, TP (2950, 250) idle): the dance waits for frame 11 of Lilac's
# skin idle (12 frames at speed 14), the same skin for every character: M 50 exactly 79 frames after the landing for Lilac, Carol
# and Milla (RV f19 -> f98), the light at 64 first (RV 29 frames before); the pose is skin anim 40 direction 8; moving or
# jumping ends it, standing still again restarts it; Carol on the bike never dances (no MV.A = 3 event); a walk-in also dances
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, obs, _ = run('fn5', ch, '5 TP 2950 250', 300, '%d:-1' % DISCOLIGHT)
    land = [r['f'] for r in rows if r['f'] > 6 and r['gnd']][0]
    m50 = [r['f'] for r in rows if r['mm'] == 50]
    a64 = [o['f'] for o in of(obs, DISCOLIGHT) if o['a0'] == 64]
    check('disco_dance_rv_timing_%s' % nm, m50 and m50[0] - land == 79 and a64 and a64[0] < m50[0] and all(r['mm'] == 50 for r in rows if r['f'] >= m50[0]),
          'land %s M50 %s A64 %s' % (land, m50[:1], a64[:1]))
    rows, _, _ = run('fn5', ch, '5 TP 2950 250;150 156 R;300 302 J', 420)
    m50 = [r['f'] for r in rows if r['mm'] == 50]
    again = [f for f in m50 if 157 < f < 300]
    check('disco_dance_cancel_and_restart_%s' % nm, m50 and m50[0] < 150 and at(rows, 157)['mm'] != 50 and again and at(rows, 299)['mm'] == 50 and at(rows, 303)['mm'] != 50,
          'M50 %s, again %s' % (m50[:1], again[:1]))
    rows, _, _ = run('fn5', ch, '5 TP 2760 250;60 100 R', 300)
    check('disco_dance_after_walk_in_%s' % nm, any(r['mm'] == 50 for r in rows if r['f'] > 100))
rows, _, _ = run('fn5', 3, '5 TP 2950 250', 300)
check('disco_no_dance_on_the_bike', all(r['mm'] != 50 for r in rows) and rows[-1]['bike'] == 1)
rows, obs, _ = run('fn4', 0, '55 TP 3297 900', 300, '%d:-1' % DISCOLIGHT)
check('disco_none_in_a4', all(r['mm'] != 50 for r in rows) and all(o['a0'] == 0 for o in of(obs, DISCOLIGHT)))

# ---- 16. pole pods (A5 8350-8367): in from the left, 2.5 turns / 450 px down, released with half the pod's speed, the pod returns
rows, obs, sids = run('fn5', 0, '55 TP 4000 150;70 90 R', 420, '%d:4056' % PODDOOR)
held = [r for r in rows if r['st'] == ST_HELD]
pod = of(obs, PODDOOR)
check('pod_takes_player', held and 249 in sids and sids[249][0] == held[0]['f'], 'M43 at %s' % (held[0]['f'] if held else None))
rel = [r for r in rows if held and r['f'] == held[-1]['f'] + 1]
check('pod_descends_450', pod and abs(max(o['y'] for o in pod) - 168 - 450) <= 8 and rel and rel[0]['gsp'] > 4, 'max y %s' % max(o['y'] for o in pod))
check('pod_returns_top', pod[-1]['y'] == 168 and pod[-1]['st'] == 0)

# ---- 15. carousel (A5 8347): every on-map piece (222 of 228), drawn by fn_gimb.c; the platforms are one-way floors
_, obs, _ = run('fn5', 0, '', 2, '%d:-1' % CAROUSEL)
check('carousel_pieces', len(of(obs, CAROUSEL, f=1)) == 222, str(len(of(obs, CAROUSEL, f=1))))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
