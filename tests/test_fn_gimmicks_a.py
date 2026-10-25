#!/usr/bin/env python3
"""Host tests for the Fortune Night gimmicks of acts 1-3 (src/game/fn_gima.c, build/re/spec_fn_gimmicks_a.md): dragon tunnels,
ferris wheels, gold platforms, rolling arches, boost blocks, breakable walls / floors (per-character breakers, the bomb blast),
lantern bridges, clotheslines, vertical half-pipes (state M38), teeter-totters, the trolley car, gachapons, fireworks, signs,
lampposts, pigeons, lanterns, strobes, stars, the FN chair and the FN1 / FN3 jump pads. Integration checks of the recovered
rules (no original trace of Fortune Night exists yet); positions are the levels' own placements (frame data).
usage: test_fn_gimmicks_a.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
# engine types (src/fp.h, Fortune Night gimmicks block 560-605)
DSEG, DHEAD, DMASK, FCAR, FSPOKE, GOLD, ARCHBASE, ARCH, ARCHWHEEL, BOOST, BFLAME = 560, 562, 563, 565, 566, 567, 568, 569, 570, 571, 572
LBRIDGE, HPIPE, TEETER, BWALL, BWPIECE, BFLOOR, BFPIECE, FWLAUNCH, FWPART, CLOTHES = 574, 575, 576, 577, 578, 579, 580, 581, 582, 583
PLANTERN, FLANTERN, GWHEEL, GFRONT, SIGN, LAMP, PIGEON, STROBE, TROLLEY, TPLAT, TLIGHT, LOOP0, LOOP1, STAR = \
    586, 587, 588, 590, 591, 592, 593, 594, 598, 599, 600, 601, 602, 603
OT_KEYSTONE, OT_RM_IDLESW, OT_RM_CHAIR = 146, 301, 336
ST_HANG, ST_PAD, ST_SIT, ST_HPIPE = 11, 20, 30, 31
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))

def run(level, ch, script, frames, track=None, extra=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    if extra: e.update(extra)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs, snd = [], [], []
    for line in (r.stdout + '\n' + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)}); rows[-1]['snd'] = snd; snd = []
        elif t[0] == 'sid': snd.append(int(t[1]))
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            d.update({t[i]: float(t[i + 1]) for i in range(4, len(t) - 1, 2) if t[i] not in ('fl',)})
            d['fl'] = int(t[t.index('fl') + 1], 16)
            obs.append(d)
    return rows, obs, r.stdout

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def obs_of(obs, t, x0=None, f=None): return [o for o in obs if o['type'] == t and (x0 is None or o['x0'] == x0) and (f is None or o['f'] == f)]
def sounds(rows, f0=0, f1=99999): return [s for r in rows if f0 <= r['f'] < f1 for s in r['snd']]
INV = '60 SETV others E 99999\n'

# ---- registry (spec 0.3): qualifier groups of the new types (FP_DUMP_OBJDEFS: grp type solid obstacle noedge obstacle_all box defined)
r = subprocess.run([BIN, DATA, '1', '-'], capture_output=True, text=True, env={**os.environ, 'FP_DUMP_OBJDEFS': '1'})
grp = {int(t[1]): [int(v) for v in t[2:]] for t in (l.split() for l in r.stdout.splitlines()) if t and t[0] == 'grp'}
check('registry_q8_q9_platforms', all(grp[t][1] and grp[t][2] and not grp[t][0] for t in (GOLD, FCAR, DMASK, TPLAT, 561)), str({t: grp[t] for t in (GOLD, FCAR, DMASK, TPLAT, 561)}))
check('registry_q7_solids', all(grp[t][0] for t in (ARCHBASE, LBRIDGE, HPIPE, TEETER, BWALL, BFLOOR, LOOP0, LOOP1, 596, 597)), str({t: grp[t][0] for t in (ARCHBASE, LBRIDGE, HPIPE, TEETER, BWALL, BFLOOR)}))
check('registry_every_fn_type_defined', all(grp[t][5] for t in range(560, 606)), str([t for t in range(560, 606) if not grp[t][5]]))

# ---- 1. dragon tunnel (FN1 dragon at (5424, 1376), 7198-7204): segments x = I + Q, Q < 752 (wrap), masks solid only next to a
# segment, the head on segment R = 13 (s + 1) - 1, conveyor +4 a frame on the back of a Tunnel1 dragon (every character)
rows, obs, _ = run('fn1', 0, '60 TP 5600 1250\n' + INV, 200, '%d:5424,%d:-1,%d:5424,%d:-1' % (DSEG, DSEG, DHEAD, DMASK))
seg = [o for o in obs_of(obs, DSEG, 5424) if o['f'] >= 61]
check('dragon_segment_x_is_I_plus_Q', seg and all(abs((o['x'] - (5424 + o['a2'] - 4)) % 752) <= 1 or abs((o['x'] - (5424 + o['a2'] - 4)) % 752 - 752) <= 1 for o in seg) and max(o['a2'] for o in seg) < 752,
      'max Q %.0f' % max(o['a2'] for o in seg) if seg else '')
heads = obs_of(obs, DHEAD, 5424)
ok = all(any(abs(h['x'] - s['x']) <= 0 and abs(h['y'] - s['y']) <= 0 for s in obs_of(obs, DSEG, 5424, h['f'])) for h in heads if h['f'] >= 61)
check('dragon_head_rides_a_segment', heads and ok)
m150 = obs_of(obs, DMASK, None, 150)
en = [m for m in m150 if not (m['fl'] & 1)]
s150 = obs_of(obs, DSEG, -1, 150)
near = [m for m in en if any(abs(m['x'] - s['x']) <= 39 + 8 and abs(m['y'] - s['y']) <= 39 + 8 for s in s150)]   # (segments moved 4 px / wrapped after the test)
check('dragon_masks_enabled_near_segments', en and len(en) < len(m150) and len(near) >= len(en) * 9 // 10,
      '%d of %d masks solid' % (len(en), len(m150)))
for ch, nm in CHARS:
    rows, _, _ = run('fn1', ch, '60 TP 5600 1250\n' + INV, 140)
    gx = [rows[f + 1]['x'] - rows[f]['x'] for f in range(95, 135) if rows[f]['gnd'] and rows[f + 1]['gnd']]
    check('dragon_conveys_right_%s' % nm, gx and gx.count(4) >= len(gx) * 3 // 4, str(gx[:12]))

# ---- 2. ferris wheel (FN1 (3784, 896)): four cars on a 128-px circle, 1 degree a frame; the car carries her; spokes on 4 x 5 radii
rows, obs, _ = run('fn1', 0, '60 TP 3784 740\n' + INV, 400, '%d:3784,%d:3784' % (FCAR, FSPOKE))
cars = obs_of(obs, FCAR, 3784)
check('ferris_cars_on_circle', cars and all(abs(((o['x'] - 3784) ** 2 + (o['y'] - 896) ** 2) ** 0.5 - 128) <= 2 for o in cars), '')
a100, a280 = sorted({round(o['a0']) for o in cars if o['f'] == 100}), sorted({round(o['a0']) for o in cars if o['f'] == 280})
check('ferris_car_period_360', len(a100) == 4 and a280 == sorted((a + 180) % 360 for a in a100) and all((b - a) % 90 == 0 for a in a100 for b in a100), '%s -> %s' % (a100, a280))
on = [r for r in rows[100:380] if r['gnd']]
check('ferris_car_carries', on and max(r['x'] for r in on) - min(r['x'] for r in on) > 150 and max(r['y'] for r in on) - min(r['y'] for r in on) > 150,
      'x %.0f..%.0f y %.0f..%.0f' % (min(r['x'] for r in on), max(r['x'] for r in on), min(r['y'] for r in on), max(r['y'] for r in on)) if on else '')
sp = obs_of(obs, FSPOKE, 3784, 200)
d = sorted(round(((o['x'] - 3784) ** 2 + (o['y'] - 896) ** 2) ** 0.5) for o in sp)
check('ferris_spokes_four_by_five', len(sp) == 20 and all(abs(d[4 * k] - 22 * (k + 1)) <= 2 and abs(d[4 * k + 3] - 22 * (k + 1)) <= 2 for k in range(5)), str(d))

# ---- 3. gold platforms (Q8 Q9, FN1 GoldPlatform1 (2632, 1200)): stood on from above
rows, _, _ = run('fn1', 0, '60 TP 2660 1150\n' + INV, 120)
check('gold_platform_stood_on', rows[-1]['gnd'] and rows[-1]['y'] == 1200, 'y %.2f' % rows[-1]['y'])

# ---- 15. loop-layer pieces (registry OD_LAYER0 / 1): a layer-4 copy at the same spot is merged into the piece (drawn in front); on
# layer 0 the set-1 pieces are ignored by the sensors (flag 0), the set-0 pieces are solid
_, obs, _ = run('fn1', 0, '', 3, '%d:-1,%d:-1' % (LOOP0, LOOP1))
l0, l1 = obs_of(obs, LOOP0, None, 2), obs_of(obs, LOOP1, None, 2)
check('loop_pieces_layers', l0 and l1 and all(not (o['fl'] & 1) for o in l0) and all(o['fl'] & 1 for o in l1), '%d / %d' % (len(l0), len(l1)))
check('loop_layer4_copies_merged', len({(o['x'], o['y']) for o in l1}) == len(l1), str(sorted((o['x'], o['y']) for o in l1)))
_, obs, _ = run('fn3', 0, '', 40, '%d:-1' % STROBE)
st = obs_of(obs, STROBE)
s10 = {round(o['a0']) for o in st if o['f'] == 10}; s20 = {round(o['a0']) for o in st if o['f'] == 20}
check('strobe_lights_swing', len(s10) == 12 and s20 == {(a + 10) % 360 for a in s10}, '')

# ---- 4. rolling arch (FN1 (1896, 416) on base (1768, 288)): at rest A 180, the base not solid (flag 0), the wheel at the action point
_, obs, _ = run('fn1', 0, '', 70, '%d:1896,%d:1768,%d:1992' % (ARCH, ARCHBASE, ARCHWHEEL))
a = obs_of(obs, ARCH, 1896, 69); b = obs_of(obs, ARCHBASE, 1768, 69); w = obs_of(obs, ARCHWHEEL, 1992, 69)
check('arch_rests_at_180_base_open', a and a[0]['a0'] == 180 and b and (b[0]['fl'] & 1), '')
check('arch_wheel_at_action_point', w and abs(w[0]['x'] - (1896 - 97)) <= 1 and abs(w[0]['y'] - 416) <= 1, str((w[0]['x'], w[0]['y'])) if w else '')

# ---- 5. boost blocks (FN1 (1928, 936), turned down by its marker): in view it arms (flag 5), flies 5 px a frame, crashes on the
# floor (anim 7, sound 163), waits to D = 80 and flies back reversed; contact hurts (10 = 2 half-petals on Normal)
rows, obs, _ = run('fn1', 0, '60 TP 1960 1000\n' + INV, 260, '%d:1928' % BOOST)
bk = obs_of(obs, BOOST, 1928)
fly = [o for o in bk if o['st'] == 1 and o['f'] < 80]
check('boost_block_arms_and_flies_down', fly and all(o['fl'] & 0x20 for o in fly) and fly[1]['y'] - fly[0]['y'] == 5 and fly[0]['a1'] == 5, '')
crash = [o for o in bk if o['st'] >= 2]
check('boost_block_crash_sound_163', crash and 163 in sounds(rows), '')
back = [o for o in bk if o['st'] == 1 and o['f'] > 140]
check('boost_block_returns_reversed', back and back[0]['a1'] == -5 and back[1]['y'] < back[0]['y'], '')
for ch, nm in CHARS:
    rows, _, _ = run('fn1', ch, '60 TP 1928 990\n', 120)
    check('boost_block_contact_hurts_%s' % nm, min(r['hp'] for r in rows[60:]) == 12, 'hp min %d' % min(r['hp'] for r in rows[60:]))

# ---- 6. breakable wall (FN1 (1352, 1136), corridor floor y 1232): walking into it does not break it; Lilac's boost, Carol's roll,
# Carol's bike ram and Milla's cube burst do (sound 13, 8 pieces); the bomb blast opens Floor2 (FN1 (1512 / 1576, 1232))
WALL = '%d:1352,%d:-1' % (BWALL, BWPIECE)
def wall_gone(obs, f): return not [o for o in obs_of(obs, BWALL, 1352, f) if o['y'] == 1136]
rows, obs, _ = run('fn1', 0, '60 TP 1450 1210\n' + INV + '70 150 L\n', 160, WALL)
check('wall_blocks_walking', not wall_gone(obs, 159) and min(r['x'] for r in rows[100:]) > 1370, 'min x %.0f' % min(r['x'] for r in rows[100:]))
rows, obs, _ = run('fn1', 0, '60 TP 1450 1210\n' + INV + '60 EN 100\n70 150 L\n90 110 B\n', 160, WALL)
check('wall_breaks_lilac_boost', wall_gone(obs, 159) and 13 in sounds(rows) and len(obs_of(obs, BWPIECE, None, 130)) >= 8, '')
rows, obs, _ = run('fn1', 2, '60 TP 1480 1210\n' + INV + '62 140 L\n76 140 D\n', 160, WALL)
check('wall_breaks_carol_roll', wall_gone(obs, 159) and 7 in sounds(rows), '')
rows, obs, _ = run('fn1', 3, '60 TP 1480 1210\n' + INV + '62 140 L\n64 140 B\n', 160, WALL)
check('wall_breaks_carol_bike', wall_gone(obs, 159), '')
rows, obs, _ = run('fn1', 4, '60 TP 1480 1210\n' + INV + '62 64 L\n100 160 X\n200 202 X\n', 260, WALL)
check('wall_breaks_milla_cube', wall_gone(obs, 259) and 416 in sounds(rows), '')
rows, obs, _ = run('fn1', 0, '60 TP 1544 1170\n' + INV, 260, '%d:1512,%d:1576,%d:-1' % (BFLOOR, BFLOOR, BFPIECE))
fl = [o for o in obs_of(obs, BFLOOR, None, 259) if o['x0'] in (1512, 1576)]
check('bomb_blast_breaks_floor2', not fl and len(obs_of(obs, BFPIECE)) > 0 and 38 in sounds(rows) and 13 in sounds(rows), str(fl))

# ---- 7. lantern bridge (FN1 link1 (2792, 416), E 1: direction 0, curling at the start): A eases to 13 (link 8 at 91 degrees),
# uncurls once she is airborne, curls again when she comes within 87 px of the anchor
rows, obs, _ = run('fn1', 0, '60 TP 2600 380\n' + INV + '120 180 R\n', 260, '%d:2792,%d:3016' % (LBRIDGE, LBRIDGE))
l1 = obs_of(obs, LBRIDGE, 2792)
check('lantern_bridge_curls_to_13', l1 and max(o['a0'] for o in l1 if o['f'] < 60) > 12.9 and min(o['y'] for o in obs_of(obs, LBRIDGE, 3016) if o['f'] < 60) < 300, '')
check('lantern_bridge_uncurls_when_airborne', any(o['st'] == -1 for o in l1 if 60 < o['f'] < 120) and min(o['a0'] for o in l1 if 100 < o['f'] < 125) < 1, '')
check('lantern_bridge_recurls_near_anchor', any(o['st'] == 1 for o in l1 if o['f'] > 125), str([(o['f'], o['st']) for o in l1 if o['f'] in (130, 160, 200)]))

# ---- 8. clotheslines (FN1 lines (2832..2968, 1104), long sensor (2976, 1112)): grab (M41, sound 33, MV.Y = line y + 32), shimmy
# 3 px a frame with OTHERS.Y 42, stopped by the long sensor; every character, Carol on the bike too
for ch, nm in CHARS + ((3, 'carol_bike'),):
    rows, _, _ = run('fn1', ch, '60 TP 2880 1122\n' + INV + '80 140 R\n', 150)
    hang = [r for r in rows if r['st'] == ST_HANG]
    check('clothesline_grab_%s' % nm, hang and hang[0]['y'] == 1104 + 32 + 14 and 33 in sounds(rows), '')
    mv = [rows[f + 1]['x'] - rows[f]['x'] for f in range(85, 95)]
    check('clothesline_shimmy_%s' % nm, mv == [3] * 10 and rows[90]['oy'] == 42 and abs(rows[139]['x'] - 2958) < 1 and rows[139]['oy'] == 41, '%s end x %.0f' % (mv[:4], rows[139]['x']))
rows, _, _ = run('fn1', 0, '60 TP 2880 1122\n' + INV + '100 102 J\n', 130)
check('clothesline_jump_off', rows[99]['st'] == ST_HANG and rows[105]['st'] != ST_HANG and min(r['vy'] for r in rows[100:106]) < -3, '')

# ---- 9. vertical half-pipe (FN2 column x 3264, corridor floor 1040): running left into hp1 enters M38 (OTHERS.Y 45), the wall run
# (hp2, made solid by M38) sets 46; hp2 is not solid outside M38 (7535); every character
for ch, nm in CHARS:
    rows, obs, _ = run('fn2', ch, '60 TP 3560 1026\n' + INV + '62 220 L\n', 200, '%d:3264' % HPIPE)
    m38 = [r for r in rows if r['mm'] == 38]
    check('halfpipe_m38_entry_%s' % nm, m38 and m38[0]['oy'] == 45 and m38[0]['st'] == ST_HPIPE, '')
    wall = [r for r in m38 if r['oy'] == 46]
    check('halfpipe_wall_run_%s' % nm, wall and min(r['y'] for r in wall) < 760 and all(abs(r['ang'] - 270) < 1 for r in wall[5:]), 'min y %.0f' % min(r['y'] for r in wall) if wall else '')
    hp2 = obs_of(obs, HPIPE, 3264)
    solid_in = [o for o in hp2 if o['y'] in (560, 656, 752, 848) and o['f'] == int(wall[5]['f'])] if wall else []
    pre = [o for o in hp2 if o['y'] in (560, 656, 752, 848) and o['f'] == 100]
    check('halfpipe_wall_solid_only_in_m38_%s' % nm, pre and all(o['fl'] & 1 for o in pre) and solid_in and not any(o['fl'] & 1 for o in solid_in), '')
rows, _, _ = run('fn2', 0, '60 TP 3560 1026\n' + INV + '62 140 L\n', 400)
check('halfpipe_m38_ends', any(r['mm'] == 38 for r in rows) and rows[-1]['mm'] != 38, 'last M %d' % rows[-1]['mm'])

# ---- 10. teeter-totter (FN2 (2816, 1328)): ctor A 30; standing right of the pivot tilts it to -30, image direction 16, sound 143
rows, obs, _ = run('fn2', 0, '60 TP 2920 1200\n' + INV, 200, '%d:2816' % TEETER)
t = obs_of(obs, TEETER, 2816)
check('teeter_tilts_under_weight', t and t[0]['a0'] == 30 and t[-1]['a0'] == -30 and t[-1]['an'] != t[0]['an'] and 143 in sounds(rows), '')

# ---- 11. trolley car (FN2 (152, 1432)): starts at 6 px a frame once the camera has left it behind, its platforms follow; it carries
# her, jumping off keeps its speed (B += 6); the loop sound 260 starts
rows, obs, _ = run('fn2', 0, '60 238 R\n' + INV + '240 TP 700 1416\n300 302 J\n', 320, '%d:152,%d:-1' % (TROLLEY, TPLAT))
car = obs_of(obs, TROLLEY, 152)
mv = [o for o in car if o['a2'] == 6]
check('trolley_starts_and_runs', mv and mv[1]['x'] - mv[0]['x'] == 6 and 260 in sounds(rows), '')
pl = obs_of(obs, TPLAT, None, 250); c250 = obs_of(obs, TROLLEY, 152, 250)
check('trolley_platforms_follow', c250 and sorted(o['x'] - c250[0]['x'] for o in pl) == [16, 80], str([o['x'] for o in pl]))
check('trolley_carries', all(rows[f + 1]['x'] - rows[f]['x'] == 6 for f in range(250, 290)), '')
check('trolley_jump_keeps_speed', abs(rows[301]['vx'] - 6) < 0.6, 'vx %.2f' % rows[301]['vx'])

# ---- 12. gachapon (FN3 wheel (832, 1247), switch (913, 1168)): pulling the switch spins the wheel (187), it slows, stops on a sector
# (188) and pays out (sector 7: 8 keystones, sound 15), then goes dark; every character can pull it
for ch, nm in CHARS:
    rows, obs, _ = run('fn3', ch, '60 TP 912 1188\n' + INV, 420, '%d:832,%d:-1' % (GWHEEL, OT_KEYSTONE))
    wh = obs_of(obs, GWHEEL, 832)
    sts = [o['st'] for o in wh]
    check('gachapon_cycle_%s' % nm, all(s in sts for s in (1, 2, 3, 4)) and 187 in sounds(rows) and 188 in sounds(rows), str(sorted(set(sts))))
    e = next((o['a3'] for o in wh if o['st'] == 3), None)
    check('gachapon_prizes_%s' % nm, e == 7 and sounds(rows).count(15) == 8, 'E %s, sound 15 x %d' % (e, sounds(rows).count(15)))

# ---- 13. fireworks (FN1 launcher (3816, 825), layer 1): shown when below the view near her, rises 6 px a frame, after 40 frames
# bursts into 2 x 32 particles (sound 430) and is gone
rows, obs, _ = run('fn1', 0, '60 TP 3816 560\n' + INV, 140, '%d:3816,%d:-1' % (FWLAUNCH, FWPART))
fw = obs_of(obs, FWLAUNCH, 3816)
check('firework_rises_and_bursts', fw and any(o['y'] == 825 for o in fw) and min(o['y'] for o in fw) < 825 - 200 and 430 in sounds(rows) and not obs_of(obs, FWLAUNCH, 3816, 139), '')
check('firework_particles', len(obs_of(obs, FWPART, None, 110)) >= 64 and len(obs_of(obs, FWPART, None, 139)) < len(obs_of(obs, FWPART, None, 110)), '')

# ---- 14. decor: the sign spins when passed at |B| >= 5 (A = floor(10 |B|), decays to 20), the lamppost breaks (23 + 134) and goes,
# pigeons fly off (243), paper lanterns in sync, stars follow the sky, strobes swing
_, obs, _ = run('fn1', 0, '60 TP 2250 1110\n' + INV + '62 200 R\n', 200, '%d:2392' % SIGN)
sg = obs_of(obs, SIGN, 2392)
check('sign_spins_when_passed', sg and max(o['a0'] for o in sg) >= 49 and max(o['fr'] for o in sg) > 3, 'max A %.0f' % max(o['a0'] for o in sg) if sg else '')
for ch, nm, keys in ((0, 'lilac', '96 98 X\n'), (2, 'carol', '96 98 X\n'), (4, 'milla', '91 99 B\n')):   # an air attack / Milla's shield burst at the lamp (FN1 (4584, 1266))
    rows, obs, _ = run('fn1', ch, '60 TP 4570 1300\n' + INV + '62 64 R\n90 100 J\n' + keys, 300, '%d:4584' % LAMP)
    lp = obs_of(obs, LAMP, 4584)
    check('lamppost_breaks_%s' % nm, 134 in sounds(rows) and 23 in sounds(rows) and any(o['fl'] & 0x40 for o in lp) and not obs_of(obs, LAMP, 4584, 299), '')
rows, obs, _ = run('fn1', 0, '60 TP 1000 250\n' + INV, 120, '%d:1028' % PIGEON)
pg = [o for o in obs_of(obs, PIGEON, 1028) if o['f'] > 60]
check('pigeons_fly_off', pg and any(o['st'] == 1 for o in pg) and min(o['y'] for o in pg) < 200 and 243 in sounds(rows), '')
_, obs, _ = run('fn1', 0, '', 100, '%d:-1,%d:-1' % (PLANTERN, STAR))
fr = {o['fr'] for o in obs_of(obs, PLANTERN, None, 99)}
check('paper_lanterns_in_sync', len(fr) == 1, str(fr))
_, obs, _ = run('fn1', 0, '60 TP 3000 900\n', 100, '%d:-1' % STAR)
s1, s2 = obs_of(obs, STAR, None, 50), obs_of(obs, STAR, None, 99)
check('stars_follow_the_sky', len(s1) == 16 and s1[0]['x'] != s2[0]['x'], '')
_, obs, _ = run('fn2', 0, '60 TP 8100 900\n' + INV, 120, '%d:9064' % FLANTERN)
fl = obs_of(obs, FLANTERN, 9064)
on = [o for o in fl if o['a0'] > 0]
check('floating_lanterns_start_past_x', fl and fl[0]['a0'] == 0 and on and 0.5 <= on[0]['a0'] <= 0.95 and on[-1]['a2'] < on[0]['a2'], '')

# ---- 15. FN chair (ObjectChair_1797 = the RM chair type, 7272): stopping on it sits (M39)
for ch, nm in CHARS:
    rows, _, _ = run('fn1', ch, '60 TP 1040 990\n' + INV, 150)
    check('fn_chair_sits_%s' % nm, any(r['st'] == ST_SIT and r['mm'] == 39 and r['x'] == 1053 for r in rows), '')

# ---- 16. FN1 jump pads (level data PADS_FN1): pad A0 (3880, 1328, lit) is ridden by Carol (M48), not by Lilac
for ch, nm, ride in ((0, 'lilac', 0), (2, 'carol', 1)):
    rows, _, _ = run('fn1', ch, '60 TP 3880 1300\n' + INV + '100 102 J\n', 140)
    check('fn1_pad_%s_%s' % ('ride' if ride else 'no_ride', nm), any(r['st'] == ST_PAD for r in rows[100:]) == bool(ride), '')

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
