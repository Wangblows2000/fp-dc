#!/usr/bin/env python3
"""Host gameplay tests for Carol (session 10): her moves, the fuel can / bike, parked bikes and the jump pads.
Frame-exact behaviour is checked against the original by tools/orig/cmp_carol.py; these tests guard the
integration (state changes, objects, character switches) and run without the original game.
usage: test_carol.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    out = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True,
                         env={**os.environ, 'FP_TRACE_SID': '1', **env}).stdout
    os.unlink(name)
    rows, sid, objs = [], [], {}
    for line in out.splitlines():
        t = line.split()
        if t and t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
            rows[-1]['sid'] = sid; sid = []
        elif t and t[0] == 'sid': sid.append(int(t[1]))
        elif t and t[0] == 'obj': objs.setdefault(int(t[3]), []).append((float(t[5]), float(t[7])))
    return rows, objs

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sounds(rows): return [s for r in rows for s in r['sid']]

C = {'FP_CHAR': '2', 'FP_LEVEL': 'dv1'}
B = {'FP_CHAR': '3', 'FP_LEVEL': 'dv1'}

# speed cap 12 on foot (1555), 15 on the bike; energy +0.25 (1663)
r, _ = run('', 140, {**C, 'FP_ENERGY': '50'})
d = [b['en'] - a['en'] for a, b in zip(r[100:], r[101:])]
check('carol_energy_regen_0_25', all(abs(x - 0.25) < 1e-3 for x in d), 'per frame %s' % sorted(set(round(x, 3) for x in d)))
check('carol_is_carol', r[-1]['chr'] == 2 and r[-1]['bike'] == 0)

# pounce: jump, then jump again in the air: B >= 6 held, OTHERS.Y 24, sounds 154 (+ voice 133)
r, _ = run('10 12 J\n20 21 J\n', 60, C)
p = [x for x in r if x['oy'] == 24]
check('pounce_dash', bool(p) and min(abs(x['vx']) for x in p[1:] or p) >= 5.9, '%d frames' % len(p))
check('pounce_sounds', 154 in sounds(r) and 133 in sounds(r), str(sounds(r)))

# Wild Claw: three presses -> combo index advances, shields made (claw sounds 410-412)
r, _ = run('10 12 X\n30 32 X\n50 52 X\n', 80, C)
check('claw_combo_sounds', sum(1 for s in sounds(r) if s in (410, 411, 412)) >= 2, str(sounds(r)))

# Wild Kick: special held with energy: M44 (state 21), energy drains 1.5/frame
r, _ = run('10 30 B\n', 60, {**C, 'FP_ENERGY': '100'})
k = [x for x in r if x['st'] == 18]   # ST_WILDKICK
check('wild_kick_drains_energy', bool(k) and min(x['en'] for x in r) <= 100 - 1.5 * 15, 'min en %.1f, %d kick frames' % (min(x['en'] for x in r), len(k)))

# roll: Down while running at |B| >= 2 (1748), sound 7
r, _ = run('5 80 R\n40 42 D\n40 80 D\n', 90, C)
check('roll_entry', any(x['st'] == 16 for x in r) and 7 in sounds(r))   # ST_ROLL

# wall cling at the DV1 wall x 1312 (RV c_wall_slide): jump toward it, cling (M47), slide to the floor
r, _ = run('60 TP 1290 450\n80 150 R\n82 85 J\n', 140, C)
w = [x for x in r if x['st'] == 19]  # ST_WALL
check('wall_cling', len(w) >= 20 and 33 in sounds(r), '%d frames on the wall' % len(w))
r2, _ = run('60 TP 1290 450\n80 96 R\n82 85 J\n95 96 J\n', 120, C)
check('wall_jump_sound', 117 in sounds(r2), str(sounds(r2)))

# fuel can (DV1 (1924, 1348)): Carol gets the bike, sound 126, jump (RV c_fuel_dv1)
r, _ = run('60 TP 1880 1320\n70 120 R\n', 130, C)
got = next((x for x in r if x['bike'] == 1), None)
check('fuel_can_gives_bike', got is not None and r[-1]['chr'] == 3 and 126 in sounds(r), 'f %s' % (got['f'] if got else None))

# on the bike: speed limits (C 8, D 15), jump sound 232, air jump M32 (sounds 232 + 409)
r, _ = run('10 12 J\n20 21 J\n', 80, B)
check('bike_air_jump', 409 in sounds(r) and sounds(r).count(232) >= 2, str(sounds(r)))
r, _ = run('10 300 R\n', 300, B)
check('bike_rides', max(x['gsp'] for x in r) > 3 and r[-1]['bike'] == 1, 'max B %.2f' % max(x['gsp'] for x in r))

# dismount (crouch + jump, 1828) leaves a parked bike; falling onto it remounts (2109, RV b_remount)
r, o = run('10 18 D\n16 17 J\n60 TP 70 400\n', 100, {**B, 'FP_DBG_OBJS': '40'})
check('dismount_parks_bike', r[45]['chr'] == 2 and 159 in o, 'chr %d objs %s' % (r[45]['chr'], o.get(159)))
check('remount_parked_bike', r[-1]['bike'] == 1 and r[-1]['chr'] == 3 and 126 in sounds(r))

# three hits wreck the bike (1285): Carol ends up on foot, wreck sound 29
r, _ = run('20 HURT 20\n100 HURT 20\n180 HURT 20\n', 260, {**B, 'FP_HP': '14'})
check('bike_wrecked_on_third_hit', r[150]['bike'] == 1 and r[-1]['bike'] == 0 and r[-1]['chr'] == 2 and 29 in sounds(r),
      'bc %d -> bike %d' % (r[150]['bc'], r[-1]['bike']))

# nitro: special held + direction on the ground: B >= 10, sound 387 (RV b_nitro)
r, _ = run('10 90 R\n40 60 B\n', 90, B)
check('bike_nitro', max(x['gsp'] for x in r) >= 10 and 387 in sounds(r), 'max B %.2f' % max(x['gsp'] for x in r))

# jump pads (DV2, RV c_jumppad_dv2): Carol hops pad to pad and launches from the last one
P = {'FP_CHAR': '2', 'FP_LEVEL': 'dv2'}
r, _ = run('60 TP 2736 1110\n80 84 J\n110 114 J\n135 139 J\n160 164 J\n', 200, P)
pad = [x for x in r if x['st'] == 20]   # ST_PAD
check('carol_jump_pad_chain', len(pad) > 60 and 61 in sounds(r) and any(abs(x['x'] - 3176) < 2 for x in pad), '%d frames riding' % len(pad))
# Lilac on the first pad: it goes dark and two crystals pop out (4944, 3275)
r, o = run('60 TP 2736 1110\n80 84 J\n', 110, {'FP_CHAR': '0', 'FP_LEVEL': 'dv2', 'FP_DBG_OBJS': '100'})
check('lilac_pad_crystals', not any(x['st'] == 20 for x in r) and len(o.get(1, [])) + len(o.get(102, [])) >= 1, str({k: v for k, v in o.items() if k in (1, 102)}))

# Lilac is untouched by Carol's code: no Carol state, no bike
r, _ = run('10 12 J\n20 21 J\n', 60, {'FP_LEVEL': 'dv1'})
check('lilac_double_jump_is_cyclone', 288 in sounds(r) and r[-1]['chr'] == 0, str(sounds(r)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
