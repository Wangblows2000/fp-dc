#!/usr/bin/env python3
"""Host gameplay tests for Milla (session 11): character select, the Puppy Float, the cube (charge, carry, throw,
burst), the shield, shield burst, super shield beam and its recoil, the dig state, hits, voices, HUD and VMU icon.
Frame-exact behaviour is checked against the original by tools/orig/cmp_milla.py; these tests guard the integration.
usage: test_milla.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ, FP_TRACE_SID='1'); e.pop('FP_LEVEL', None); e.update(env)
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e).stdout
    os.unlink(name)
    rows, sid, ens = [], [], {}
    for line in out.splitlines():
        t = line.split()
        if t and t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
            rows[-1]['sid'] = sid; sid = []
        elif t and t[0] == 'sid': sid.append(int(t[1]))
        elif t and t[0] == 'en': ens[int(t[1])] = {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2)}
    return rows, ens

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sounds(rows): return [s for r in rows for s in r['sid']]
def at(rows, f): return next(r for r in rows if r['f'] == f)

M = {'FP_CHAR': '4', 'FP_LEVEL': 'dv1'}

# character select: CLASSIC route, Down from Lilac to Milla (cursor 3), confirm: voice 186, she plays DV1
r, _ = run('40 42 S\n200 201 J\n230 231 J\n260 261 D\n280 281 J\n370 371 D\n380 381 J\n', 520, {})
play = [x for x in r if x['mode'] == 1]
check('select_milla_plays', bool(play) and play[-1]['chr'] == 4, 'chr %s' % (play[-1]['chr'] if play else None))
check('select_milla_voice_186', 186 in sounds(r))

# energy: +0.5 / frame (1664), the meter is hidden
r, _ = run('', 140, {**M, 'FP_ENERGY': '50'})
d = [b['en'] - a['en'] for a, b in zip(r[100:], r[101:])]
check('milla_energy_regen_0_5', all(abs(x - 0.5) < 1e-3 for x in d), str(sorted(set(round(x, 3) for x in d))))

# Puppy Float (M22): hold jump through the apex -> she rises 2 px/frame, sound 244 once, flaps 170, ends at E >= 100
r, _ = run('80 300 J\n', 300, M)
fl = [x for x in r if x['mm'] == 22]
check('float_starts_after_apex', bool(fl) and fl[0]['vy'] >= -0.01, 'first float vy %s' % (fl[0]['vy'] if fl else None))
rise = [b['y'] - a['y'] for a, b in zip(fl[20:60], fl[21:61])]
check('float_rises_2px', bool(rise) and all(abs(x + 2) < 0.01 for x in rise), str(sorted(set(round(x, 2) for x in rise))[:4]))
check('float_lasts_until_meter_full', 85 <= len(fl) <= 92 and fl[-1]['me'] >= 98, '%d frames, E %.1f' % (len(fl), fl[-1]['me'] if fl else -1))
check('float_sounds', sounds(r).count(244) == 1 and sounds(r).count(170) >= 8, str(sorted(set(sounds(r)))))
r2, _ = run('80 120 J\n', 160, M)
check('float_ends_on_release', any(x['mm'] == 22 for x in r2) and not any(x['mm'] == 22 for x in r2[125:]))
check('no_double_jump', not any(x['mm'] == 1 and x['vy'] < -6 for x in r2[84:]))

# Phantom Cube: attack held 40+ frames standing -> M30, cube appears (A = 1, sound 415) ~42 frames after the press
r, _ = run('100 160 X\n', 200, M)
a1 = next((x['f'] for x in r if x['rA'] == 1), None)
check('cube_appears_after_charge', a1 is not None and 140 <= a1 <= 144 and 415 in sounds(r) and 422 in sounds(r), 'A1 at %s' % a1)
check('cube_carry_state_m30', at(r, 180)['mm'] == 30)
r, _ = run('100 120 X\n', 160, M)
check('cube_cancel_early_release', not any(x['rA'] == 1 for x in r) and at(r, 140)['mm'] == 0)
# throw: A 1 -> 2 (sound 417), flies, bursts (A = 3, sound 416) for 21 frames, then parked (A = 0)
r, _ = run('100 160 X\n200 202 X\n', 320, M)
a2 = [x for x in r if x['rA'] == 2]; a3 = [x for x in r if x['rA'] == 3]
check('cube_throw_flies', len(a2) > 3 and a2[3]['rx'] > a2[0]['rx'] + 10 and 417 in sounds(r), '%d flight frames' % len(a2))
check('cube_burst_21_frames', len(a3) in (20, 21) and 416 in sounds(r) and r[-1]['rA'] == 0, '%d burst frames' % len(a3))

# Barrier Shield (special held): M31, A = 4, sound 420; release -> shield burst (A = 5, sound 418) -> cool-down -> M0
r, _ = run('80 120 B\n', 200, M)
check('shield_up', at(r, 100)['mm'] == 31 and at(r, 100)['rA'] == 4 and 420 in sounds(r))
check('shield_burst_on_release', any(x['rA'] == 5 for x in r) and 418 in sounds(r) and r[-1]['mm'] == 0 and r[-1]['rA'] == 0)
r2, _ = run('80 120 BR\n', 130, M)
check('shield_blocks_walking', abs(at(r2, 118)['x'] - at(r2, 90)['x']) < 0.01)

# Super Shield Burst: cube + special -> sound 421, release -> beam (419) and recoil B = -7 (facing right)
r, _ = run('100 160 X\n170 200 B\n', 260, M)
rel = next((x for x in r if 419 in x['sid']), None)
check('super_shield_beam', 421 in sounds(r) and rel is not None)
check('super_recoil_minus_7', rel is not None and abs(rel['gsp'] + 7) < 0.6, 'gsp %s' % (rel['gsp'] if rel else None))
r, _ = run('100 160 X\n170 200 B\n170 210 D\n', 260, M)   # the aim is read on the release frame: keep Down held (RV m_super_down)
k = next((i for i, x in enumerate(r) if 419 in x['sid']), None)
check('super_recoil_aimed_down_lifts', k is not None and r[k]['gnd'] == 0 and r[k]['vy'] <= -4.7 and min(x['y'] for x in r[k:k + 20]) < r[k - 1]['y'] - 10)
r, _ = run('100 160 X\n170 200 B\n170 210 U\n', 260, M)
k = next((i for i, x in enumerate(r) if 419 in x['sid']), None)
check('super_recoil_aimed_up_stays_down', k is not None and r[k + 2]['gnd'] == 1)
# against a wall (DV1 x 5447): the recoil into the wall stops at it (no clipping)
r, _ = run('60 TP 5430 800\n70 90 R\n100 160 X\n170 200 B\n180 182 L\n', 260, M)
check('super_recoil_into_wall_stops', max(x['x'] for x in r[200:]) < 5450, 'max x %.1f' % max(x['x'] for x in r[200:]))

# the cube burst damages an enemy (DV1 Turretus at (1233, 437), Milla invulnerable): A 8 -> 4 -> defeated (RV m_turr_throw)
r, ens = run('60 TP 1180 423\n61 SETV others E 9999\n100 160 X\n200 202 X\n', 300, {**M, 'FP_DBG_ENEMY': '1'})
check('cube_burst_hits_enemy', sounds(r).count(24) >= 2 and 23 in sounds(r), str([s for s in sounds(r) if s in (23, 24, 416)]))
# the plain shield burst hits it twice and defeats it (RV m_turr_burst: 418, 24, 24 + 23)
r, ens = run('60 TP 1200 423\n61 SETV others E 9999\n100 130 B\n', 200, {**M, 'FP_DBG_ENEMY': '1'})
check('shield_burst_hits_enemy', 418 in sounds(r) and sounds(r).count(24) == 2 and 23 in sounds(r), str([s for s in sounds(r) if s in (23, 24, 418)]))
# the cube burst and the beam break the DV1 rocks (RV m_rock_throw / m_rock_beam: sound 13)
r, _ = run('60 TP 6740 860\n61 SETV others E 9999\n100 160 X\n200 202 X\n', 260, M)
check('cube_breaks_rock', 13 in sounds(r))

# hurt voices 179-182 (Milla's set), cube dropped when hurt while carrying (2235)
r, _ = run('100 160 X\n180 HURT 20\n', 260, M)
check('milla_hurt_voice', any(s in (179, 180, 181, 182) for s in sounds(r)), str([s for s in sounds(r) if s >= 170]))
check('cube_dropped_when_hurt', any(x['rA'] in (2, 3) for x in r[180:]))

# dig (M27) only inside a dig zone (MILLA.D, other stages): DV1 crouch + attack does nothing; with a zone she digs
r, _ = run('80 200 D\n100 102 X\n', 200, M)
check('no_dig_outside_zones', not any(x['mm'] == 27 for x in r))
z = '61 DIGZONE 1\n80 260 D\n100 102 X\n' + ''.join('%d %d X\n' % (f, f + 2) for f in range(110, 250, 10))
r, _ = run(z, 280, M)
check('dig_in_zone', any(x['mm'] == 27 for x in r) and 184 in sounds(r) and 185 in sounds(r))

# HUD panel: B box shows the cube icon, the charging frame (8/9) while charging, lifting (4/5) while holding
r, _ = run('100 160 X\n', 200, M)
check('hud_frames', at(r, 120)['mh'] in (8, 9) and at(r, 180)['mh'] in (4, 5) and at(r, 90)['mh'] in (0, 1),
      '%d %d %d' % (at(r, 90)['mh'], at(r, 120)['mh'], at(r, 180)['mh']))

# Lilac and Carol are untouched
r, _ = run('10 12 J\n20 21 J\n', 60, {'FP_LEVEL': 'dv1'})
check('lilac_double_jump_is_cyclone', 288 in sounds(r) and r[-1]['chr'] == 0)
r, _ = run('10 12 J\n20 21 J\n', 60, {'FP_CHAR': '2', 'FP_LEVEL': 'dv1'})
check('carol_pounce_unchanged', 154 in sounds(r) and r[-1]['rA'] == -1)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
