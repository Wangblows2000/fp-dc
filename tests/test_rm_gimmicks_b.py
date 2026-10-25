#!/usr/bin/env python3
"""Host tests for the Relic Maze acts 3-5 gimmicks (src/game/relic_gimb.c, build/re/spec_rm_gimmicks_b.md): Milla's hand-car
ride, the character route split, the hanging switch and the moving pillars, the rising swing, the pickaxe, the spinny cup,
the elevator, the rolling cave platform, the drill platform, the RM4 door return, the RM4 gateway ending the act into RM5,
the RM5 light stones, the spike ceiling, the chair and the RM jump-pad trigger sets. These are integration checks of the
recovered rules (no original trace of these acts exists yet).
usage: test_rm_gimmicks_b.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
# engine types (src/fp.h, Relic Maze acts 3-5 block)
PILLAR, IDLESW, RSWING, PICKAXE, CUP, ELEV2, CART, TSWITCH, WPLATE, ALTPATH, RCAVE, DRILL, RUINS, GATE2, LSBASE, LSDOOR, SCEIL = \
    300, 301, 303, 305, 308, 310, 311, 314, 315, 320, 321, 323, 324, 327, 331, 333, 334
OT_DOOR, OT_SPIKES, OT_HANG, OT_RSPRING_U, OT_JUMPPAD = 96, 7, 17, 174, 132
ST_HELD, ST_CART, ST_SIT, ST_PAD = 28, 29, 30, 20

def run(level, ch, script, frames, track=None, extra=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch)})
    if track: e['FP_TRACK'] = track
    if extra: e.update(extra)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs, loads = [], [], []
    for line in (r.stdout + '\n' + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            d.update({t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2) if t[i] not in ('fl',)})
            d['fl'] = int(t[t.index('fl') + 1], 16)
            obs.append(d)
        elif t[0].startswith('load['): loads.append(t[0][5:t[0].index(']')])
    return rows, obs, loads, r.stdout

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def obs_of(obs, t, x0=None): return [o for o in obs if o['type'] == t and (x0 is None or o['x0'] == x0)]
INV = '60 SETV others E 99999\n'

# ---- 7. Milla's hand-car (RM3, Milla only, 6131-6166): mount at the handle (M90), pump right, the cart rides the track
rows, obs, _, _ = run('rm3', 4, '60 TP 950 1630\n' + INV + '100 800 R\n', 800, '%d:984' % CART)
mm = [r for r in rows if r['mm'] == 90]
cart = obs_of(obs, CART)
check('rm3_milla_mounts_handcar', bool(mm) and 55 < mm[0]['f'] < 110 and all(r['st'] == ST_CART for r in mm[:50]), 'first M90 frame %s' % (mm[0]['f'] if mm else None))
check('rm3_handcar_rides_track', bool(cart) and cart[-1]['x'] > 4500 and max(o['a1'] for o in cart) > 4, 'cart x %.0f, max B %.2f' % (cart[-1]['x'], max(o['a1'] for o in cart)) if cart else 'no cart')
at = {int(r['f']): r for r in rows}; c400 = [o for o in cart if o['f'] == 400]
check('rm3_milla_rides_with_cart', bool(c400) and abs(at[400]['x'] - (c400[0]['x'] - 35)) < 24 and abs(at[400]['y'] - c400[0]['y']) < 60, 'milla %.0f,%.0f cart %.0f,%.0f' % (at[400]['x'], at[400]['y'], c400[0]['x'], c400[0]['y']) if c400 else '')
check('rm3_handcar_tilts_on_slopes', bool(cart) and max(abs(o['a2']) for o in cart) > 10, 'max |A| %.1f' % max(abs(o['a2']) for o in cart) if cart else '')
# the bumper on the wall plate (5600, 1624) toggles both track switches (6145, 6153-6155)
_, obs, _, _ = run('rm3', 4, '60 TP 950 1630\n' + INV + '100 900 R\n', 900, '%d:-1' % TSWITCH)
ts = obs_of(obs, TSWITCH)
check('rm3_cart_plate_turns_track_switches', ts and max(o['a0'] for o in ts) > 5, 'max rail angle %s' % (max(o['a0'] for o in ts) if ts else None))
# jumping off (6136) leaves M90
rows, _, _, _ = run('rm3', 4, '60 TP 950 1630\n' + INV + '100 200 R\n200 202 J\n', 260)
check('rm3_jump_off_handcar', any(r['mm'] == 90 for r in rows[150:199]) and all(r['mm'] != 90 for r in rows[205:]) and min(r['vy'] for r in rows[200:215]) < -3,
      'vy after jump %.2f' % min(r['vy'] for r in rows[200:215]))
# 6374 / 6203 / 6204: the cart and switches exist only for Milla; the alternate paths split by character
for ch, nm in ((0, 'lilac'), (2, 'carol')):
    _, obs, _, _ = run('rm3', ch, '', 3, '%d:-1,%d:-1,%d:-1' % (CART, TSWITCH, ALTPATH))
    check('rm3_no_handcar_for_%s' % nm, not obs_of(obs, CART) and not obs_of(obs, TSWITCH), str(len(obs_of(obs, CART))))
    check('rm3_altpath_kept_for_%s' % nm, [o['x'] for o in obs_of(obs, ALTPATH) if o['f'] == 1] == [832], str([o['x'] for o in obs_of(obs, ALTPATH) if o['f'] == 1]))
_, obs, _, _ = run('rm3', 4, '', 3, '%d:-1' % ALTPATH)
check('rm3_altpath2_for_milla', sorted(o['x'] for o in obs_of(obs, ALTPATH) if o['f'] == 1) == [1064, 1112, 1176], str([o['x'] for o in obs_of(obs, ALTPATH) if o['f'] == 1]))

# ---- 2. hanging switch -> pillars (RM3 5866-5868, 6353-6363): hang on the bar, it sinks 8 px, the left block moves 232 px down
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, obs, _, _ = run('rm3', ch, '60 TP 10793 2548\n' + INV, 200, '%d:10816,%d:10793' % (PILLAR, IDLESW))
    sw = obs_of(obs, IDLESW); pil = [o for o in obs_of(obs, PILLAR, 10816) if abs(o['y'] - 2416) < 300 and o['f'] in (40, 199)]
    ys = sorted({o['y'] for o in obs_of(obs, PILLAR, 10816) if o['f'] == 199})
    check('rm3_hang_switch_sinks_%s' % nm, sw and max(o['y'] for o in sw) == 2528 and any(r['st'] == 11 for r in rows), 'switch y %s' % sorted({o['y'] for o in sw})[-3:])
    check('rm3_pillars_move_%s' % nm, ys[:1] == [2648.0], 'column x 10816 y %s' % ys)
# letting go and pulling again moves them back (C negated, 6007 re-arm)
_, obs, _, _ = run('rm3', 0, '60 TP 10793 2548\n' + INV + '200 202 J\n260 TP 10793 2548\n', 460, '%d:10816' % PILLAR)
ys = sorted({o['y'] for o in obs_of(obs, PILLAR, 10816) if o['f'] == 459})
check('rm3_pillars_move_back', ys[:1] == [2416.0], str(ys))

# ---- 3. rising swing (RM3 (12006, 2950), 6123-6130): standing on it, it swings over the pivot and returns once left
rows, obs, _, _ = run('rm3', 0, '60 TP 12006 3020\n' + INV, 300, '%d:12006' % RSWING)
sw = obs_of(obs, RSWING)
check('rm3_rising_swing_rides_up', sw and max(o['a0'] for o in sw) > 170 and min(o['y'] for o in sw) < 2870, 'max A %.1f min y %.0f' % (max(o['a0'] for o in sw), min(o['y'] for o in sw)) if sw else '')
on = [r for r in rows if r['f'] > 100]
check('rm3_rising_swing_carries', on and min(r['y'] for r in on) < 2900, 'min player y %.0f' % min(r['y'] for r in on))

# ---- 4. pickaxe (RM3 (9136, 2784), 6106-6119): it swings and hammers; touching it hurts (10 = 2 petals)
_, obs, _, _ = run('rm3', 0, '', 400, '%d:9136' % PICKAXE)
px = obs_of(obs, PICKAXE)
check('rm3_pickaxe_swings_both_ways', px and max(o['a1'] for o in px) > 45 and min(o['a1'] for o in px) < -45, 'B %.1f .. %.1f' % (min(o['a1'] for o in px), max(o['a1'] for o in px)) if px else '')
rows, _, _, _ = run('rm3', 0, '106 TP 9136 2678\n', 140)
check('rm3_pickaxe_hurts', min(r['hp'] for r in rows[106:]) < 14, 'hp min %d' % min(r['hp'] for r in rows[106:]))

# ---- 5. spinny cup (RM3 (9697, 3072), 6120-6122): grab its top from the right, ride it over 90 degrees, a handlebar is made
rows, obs, _, _ = run('rm3', 0, '60 TP 9712 2955\n' + INV + '61 63 L\n', 260, '%d:9697,%d:-1' % (CUP, OT_HANG))
held = [r for r in rows if r['st'] == ST_HELD]; cup = obs_of(obs, CUP)
check('rm3_cup_holds_and_turns', held and cup and min(o['a0'] for o in cup) == -90, 'held frames %d, A min %s' % (len(held), min(o['a0'] for o in cup) if cup else None))
check('rm3_cup_makes_handlebar', any(o['x'] > 9780 for o in obs_of(obs, OT_HANG)) and any(r['st'] == 11 for r in rows[100:]), str(sorted({(o['x'], o['y']) for o in obs_of(obs, OT_HANG)})))

# ---- 6. elevator (RM3 (8064, 968), 6167-6172): it starts under her, descends, brakes from J 2300 and stops near 2749
rows, obs, _, _ = run('rm3', 0, '60 TP 8064 930\n' + INV, 700, '%d:8064' % ELEV2)
el = obs_of(obs, ELEV2)
check('rm3_elevator_descends_and_stops', el and 2700 < el[-1]['y'] < 2800 and el[-1]['y'] == obs_of(obs, ELEV2)[-20]['y'], 'y %.0f' % el[-1]['y'] if el else '')
check('rm3_elevator_carries', rows[-1]['y'] > 2600, 'player y %.0f' % rows[-1]['y'])

# ---- 8. rolling cave platform (RM4 (1720, 1190), 6406-6414): at rest on the left rim; standing on it rolls it to the right rim
rows, obs, _, _ = run('rm4', 0, '60 TP 1528 1150\n' + INV, 400, '%d:1720' % RCAVE)
cv = obs_of(obs, RCAVE)
check('rm4_cave_platform_rest', cv and cv[10]['x'] == 1528, 'x %.0f' % cv[10]['x'] if cv else '')
check('rm4_cave_platform_rolls', cv and max(o['x'] for o in cv) > 1880 and max(o['y'] for o in cv) > 1350, 'max x %.0f max y %.0f' % (max(o['x'] for o in cv), max(o['y'] for o in cv)) if cv else '')

# ---- 9. drill platform (RM4 (2760, 80), 6446 / 6387-6391): it digs 1440 px through the ruins and ends with two red springs
_, obs, _, _ = run('rm4', 0, '60 TP 2760 900\n' + INV, 400, '%d:2760,%d:-1' % (DRILL, OT_RSPRING_U))
dr = obs_of(obs, DRILL)
check('rm4_drill_digs_1440', dr and dr[-1]['y'] == 80 + 1440 and dr[-1]['a0'] >= 1440, 'y %.0f' % dr[-1]['y'] if dr else '')
check('rm4_drill_makes_springs', len({o['x'] for o in obs_of(obs, OT_RSPRING_U) if o['f'] == 399 and o['y'] > 1400}) == 2, str({(o['x'], o['y']) for o in obs_of(obs, OT_RSPRING_U) if o['f'] == 399}))

# ---- 15. RM4 door (level data door_rise 8, RE 6431): the block opens it; off the plate it closes 8 px a frame
_, obs, _, _ = run('rm4', 0, '60 PB 5070 1060\n' + INV + '60 TP 4900 1040\n200 PB 4700 1060\n', 260, '%d:5176' % OT_DOOR)
d = obs_of(obs, OT_DOOR)
ys = {o['f']: o['y'] for o in d}
check('rm4_door_opens_with_block', d and max(o['y'] for o in d) == 944 + 128, 'max y %s' % (max(o['y'] for o in d) if d else None))
fall = [ys[f] - ys[f + 1] for f in range(60, 259) if f in ys and f + 1 in ys and ys[f] > ys[f + 1]]
check('rm4_door_returns_8px', fall and set(fall) == {8}, str(fall[:6]))

# ---- 14. RM4 gateway (6432-6434, 6466-6468): push the plate, the leaf sinks; walk left into the gate: the act ends into RM5
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, obs, loads, _ = run('rm4', ch, '60 TP 8320 2500\n' + INV + '61 130 R\n140 400 L\n', 420, '%d:8200' % GATE2)
    lf = [o for o in obs_of(obs, GATE2) if o['f'] < 270]
    check('rm4_gate_leaf_opens_%s' % nm, lf and max(o['y'] for o in lf) == 2492 + 80, 'leaf max y %s' % (max(o['y'] for o in lf) if lf else None))
    check('rm4_gateway_ends_act_into_rm5_%s' % nm, loads[-1:] == ['rm5'] and rows[-1]['act'] == 5, str(loads))
# without the plate the gate does nothing
rows, _, loads, _ = run('rm4', 0, '60 TP 8250 2500\n' + INV + '61 200 L\n', 260)
check('rm4_gate_needs_plate', loads[-1:] == ['rm4'], str(loads))

# ---- 16. RM5 light stone 1 (plate (88, 2584), 6756-6759, 6774): ~115 frames of pushing light it and open door 1
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    rows, obs, _, _ = run('rm5', ch, '60 TP 110 2600\n' + INV + '61 300 L\n', 320, '%d:192,%d:552' % (LSBASE, LSDOOR))
    b = obs_of(obs, LSBASE, 192); dd = obs_of(obs, LSDOOR, 552)
    lit = [o['f'] for o in b if o['fl'] & 0x2000]
    check('rm5_light_stone_lights_%s' % nm, lit and lit[0] < 300, 'lit at %s' % (lit[0] if lit else None))
    check('rm5_light_stone_opens_door_%s' % nm, dd and dd[-1]['y'] == 680, 'door y %s' % (dd[-1]['y'] if dd else None))
# released early: the charge resets (6758)
_, obs, _, _ = run('rm5', 0, '60 TP 110 2600\n' + INV + '61 120 L\n', 200, '%d:192' % LSBASE)
b = obs_of(obs, LSBASE, 192)
check('rm5_light_stone_resets_on_release', b and max(o['a0'] for o in b) > 2 and b[-1]['a0'] == 0 and not (b[-1]['fl'] & 0x2000), 'A max %.2f end %.2f' % (max(o['a0'] for o in b), b[-1]['a0']) if b else '')
# ---- spike ceiling (6752-6755): in the basement (MV.Y > 1608) it slides down 1 px a frame, following the view in x
_, obs, _, _ = run('rm5', 0, '60 TP 110 2600\n' + INV, 200, '%d:0' % SCEIL)
sc = obs_of(obs, SCEIL)
check('rm5_spike_ceiling_descends', sc and sc[-1]['y'] - sc[0]['y'] > 100 and sc[-1]['st'] == 0, 'y %.0f -> %.0f' % (sc[0]['y'], sc[-1]['y']) if sc else '')
# ---- chair (RM5 (1085, 1392), 6585 / 3068-3072): stopping on it sits (M39); a direction stands up
rows, _, _, _ = run('rm5', 0, '60 TP 1070 1360\n' + INV + '150 160 R\n', 200)
sit = [r for r in rows if r['st'] == ST_SIT]
check('rm5_chair_sits', sit and any(r['mm'] == 39 and r['x'] == 1085 for r in sit), 'sit frames %d' % len(sit))
check('rm5_chair_stand_up', all(r['st'] != ST_SIT for r in rows[152:159]), str([r['st'] for r in rows[150:160]]))

# ---- 18. jump pads with RM handles (6092-6095): RM3 pad 9 (11256, 2984, lit): Lilac and the bike get the crystal pop,
# Carol on foot rides (M48)
for ch, nm, ride in ((0, 'lilac', 0), (2, 'carol', 1), (3, 'carol_bike', 0)):
    rows, _, _, _ = run('rm3', ch, '60 TP 11256 2960\n' + INV + '100 102 J\n', 160)
    pad = any(r['st'] == ST_PAD for r in rows[100:])
    check('rm3_pad_%s_%s' % ('ride' if ride else 'no_ride', nm), pad == bool(ride), 'pad state %s' % pad)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
