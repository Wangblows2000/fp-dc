#!/usr/bin/env python3
"""Host tests for the Fortune Night enemies (src/game/fn_enemy.c, build/re/spec_fn_enemies.md): every class spawns, attacks
on its timeline, damages the player with the spec's damage (and its shield immunity), takes hits through the shared rules
(combat.c), and defeats with its drops; Milla's cube / shield reflects lasers and pellets back into enemies. These are
integration checks of the recovered rules (no original trace of Fortune Night exists yet, spec 14 T1-T3).
usage: test_fn_enemies.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
# engine types (src/fp.h, Fortune Night enemy block)
COIL, HEAD, P1, P2, P3, P4, FLIP, POLE, L1, L2, L3, STURRET, FCANNON, ROLLER, SHELL, DUSTER, ARM2, ARM1, MOUTH, BALLOON, PIGEON = range(512, 533)
LASER, FLARE, WAVE = 540, 541, 542
OT_PELLET, OT_DEBRIS, OT_KEYSTONE, OT_ITEM_FLY, OT_MON_RINGS = 101, 107, 146, 102, 18
MON_FIRE, MON_METAL = 153, 154
DEFEATED, NOFLY = 0x40, 0x10000

def run(level, script, frames, track=None, ch=0):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs, sids, pend = [], [], [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'sid': pend.append(int(t[1]))
        elif t[0] == 'f':
            d = {t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)}
            rows.append(d); sids += [(int(d['f']), s) for s in pend]; pend = []
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            for i in range(4, len(t) - 1, 2):
                d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
            obs.append(d)
    return rows, obs, sids

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def of(obs, t, x0=None): return [o for o in obs if o['type'] == t and (x0 is None or o['x0'] == x0)]
def snd(sids, sid, f0=0, f1=1 << 30): return [f for f, s in sids if s == sid and f0 <= f <= f1]
def hp_at(rows, f): return [r['hp'] for r in rows if r['f'] == f][0]
def hp_drops(rows):                                        # (frame, petals lost) at every hp decrease
    return [(int(b['f']), int(a['hp'] - b['hp'])) for a, b in zip(rows, rows[1:]) if b['hp'] < a['hp']]
def attacks(f0, f1, every=24, key='X'):                    # repeated attack presses
    return ';'.join('%d %d %s' % (f, f + 2, key) for f in range(f0, f1, every))
INV = '3 SETV others E 99999'

# ==== 1. Coil (FN1 coil at (912, 536), head 40 px above; limiters at 821 / 845 and 1261 / 1285): walks by 75-px flips near
# the view, charges (anim 8) and zaps (anim 7, sound 167) every second flip, lands with sound 164
rows, obs, sids = run('fn1', '5 TP 760 522;' + INV, 420, '%d:912,%d:912,%d:912' % (COIL, HEAD, P1))
coil, head, p1 = of(obs, COIL), of(obs, HEAD), of(obs, P1)
check('coil_spawns_with_head_and_pieces', bool(coil) and bool(head) and bool(p1) and head[0]['A'] == 3 and coil[0]['a3'] == 1)
xs = sorted({o['x'] for o in coil})
check('coil_flips_75px', len(xs) >= 3 and all(abs((b - a) - 75) < 1 for a, b in zip(xs, xs[1:])), 'x %s' % xs[:6])
check('coil_head_follows_base', all(abs(h['x'] - c['x']) < 50 and abs(h['y'] - c['y']) < 60 for h, c in zip(head, coil)))
check('coil_pieces_follow_base', all(p['x'] == c['x'] and p['y'] == c['y'] for p, c in zip(p1, coil)))
check('coil_lands_sound_164', len(snd(sids, 164)) >= 2, str(snd(sids, 164)[:6]))
zaps = snd(sids, 167)
check('coil_zaps_sound_167', len(zaps) >= 1 and zaps[0] < 150, str(zaps))
# the zap hurts (R-hurt 10 = 2 half-petals on Normal) a player under the head after the first flip, sound 371
rows, obs, sids = run('fn1', '5 TP 1062 522', 200, '%d:912' % HEAD)
d = hp_drops(rows)
check('coil_zap_damage_10', bool(d) and d[0][1] == 2 and snd(sids, 371) and abs(snd(sids, 371)[0] - d[0][0]) <= 1, 'drops %s sid371 %s' % (d, snd(sids, 371)))
# no hits while zapping (C = 5, 7283): whips during the zap leave A = 3, the next one after it defeats the head
rows, obs, sids = run('fn1', '5 TP 1062 522;' + INV + ';' + attacks(92, 128, 12) + ';170 172 X', 220, '%d:912' % HEAD)
head = of(obs, HEAD)
check('coil_immune_while_zapping', all(o['A'] == 3 for o in head if o['f'] < 135) and any(o['fl'] & DEFEATED for o in head if o['f'] > 170),
      'A at 134 %s, defeated after 170 %s' % ([o['A'] for o in head if o['f'] == 134], any(o['fl'] & DEFEATED for o in head)))
# metal shield (OTHERS.B 3): immune to the zap
rows, obs, sids = run('fn1', '3 TP 300 522;20 MON %d;22 24 X;150 TP 1062 522' % MON_METAL, 360)
check('coil_zap_metal_shield_immune', rows[149]['sh'] == 3 and snd(sids, 167, 150) and not hp_drops(rows[150:]) and not snd(sids, 371, 150),
      'sh %s zap %s drops %s' % (rows[149]['sh'], snd(sids, 167, 150), hp_drops(rows[150:])))
# a whip hits the head (HP 3) through the shared rules: defeat (sound 23), fly-away, the crash: keystone (direction 2),
# explosion, sound 29, 7 debris; the head goes invisible (flag 7) and stays; pieces 1 / 3 hide, the base freezes (G = 0)
rows, obs, sids = run('fn1', '5 TP 880 522;8 10 X', 160, '%d:912,%d:912,%d:912,%d:-1,%d:-1' % (HEAD, COIL, P1, OT_KEYSTONE, OT_DEBRIS))
head = of(obs, HEAD); dfr = [o['f'] for o in head if o['fl'] & DEFEATED]
check('coil_head_defeated_by_whip', bool(dfr) and snd(sids, 23) and snd(sids, 24), 'defeat %s' % (dfr[:1]))
crash = snd(sids, 29)
ks = [o for o in of(obs, OT_KEYSTONE) if crash and o['f'] == crash[0]]
deb = [o for o in of(obs, OT_DEBRIS) if crash and o['f'] == crash[0]]
check('coil_head_crash_keystone_7_debris', bool(crash) and len(ks) == 1 and len(deb) == 7, 'crash %s ks %d debris %d' % (crash[:1], len(ks), len(deb)))
check('coil_head_kept_hidden', bool(head) and head[-1]['fl'] & 0x80 and head[-1]['A'] == -1, 'last fl %x A %s' % (head[-1]['fl'], head[-1]['A']))
cl = of(obs, COIL)
check('coil_base_freezes', cl[-1]['a3'] == 0 and cl[-1]['a1'] == 99)
# FN1 7346 (level data): the coil below y 1500 turns upside down (H = 180) and the CoilFlip markers go
rows, obs, sids = run('fn1', '5 TP 6100 1600;' + INV, 30, '%d:6248,%d:-1' % (COIL, FLIP))
cl = of(obs, COIL)
check('coil_fn1_flip_line', bool(cl) and cl[-1]['a4'] == 180 and of(obs, COIL)[0]['f'] <= 1, 'H %s' % (cl[-1]['a4'] if cl else None))
# FN2: the coil at the CoilFlip marker (2424, 1188) flips by overlap (7273)
rows, obs, sids = run('fn2', '5 TP 2300 1100;' + INV, 20, '%d:2424,%d:2240' % (COIL, COIL))
check('coil_fn2_flip_marker', of(obs, COIL, 2424)[-1]['a4'] == 180 and of(obs, COIL, 2240)[-1]['a4'] == 0)

# ==== 2. Trafficle (FN1 pole + lights at x 9420, y 755 / 779 / 803; floor 877): lights activate within 240 px, sweep their
# aim, fire 8-px/frame lasers (sound 171) right then left, one colour per light (laser direction 0 / 1 / 2)
rows, obs, sids = run('fn1', '5 TP 9520 840;' + INV, 420, '%d:9420,%d:9420,%d:-1' % (L1, POLE, LASER))
lz = of(obs, LASER)
check('trafficle_lights_activate', all(o['fl'] & 0x2 for o in of(obs, L1) if o['f'] > 6) and of(obs, L1)[-1]['y'] != 755, 'bob y %s' % of(obs, L1)[-1]['y'])
check('trafficle_pole_bobs', len({o['y'] for o in of(obs, POLE)}) > 10)
check('trafficle_fires_sound_171', len(snd(sids, 171)) >= 4, str(snd(sids, 171)))
check('trafficle_laser_colours', {o['dir'] for o in lz} == {0, 1, 2})
at = {}
for o in lz: at.setdefault((o['f'], o['dir'], o['y']), []).append(o['x'])
speeds = {min(abs(x1 - x0) for x0 in at.get((f - 1, d, y), [1e9])) for (f, d, y), xs in at.items() for x1 in xs if (f - 1, d, y) in at}
check('trafficle_laser_8px', 8.0 in speeds and speeds <= {0.0, 8.0}, str(speeds))
check('trafficle_fires_both_ways', any(o['a1'] == 0 for o in lz) and any(o['a1'] == -180 for o in lz))
# a laser hits a standing Lilac for 10 (2 half-petals) and is destroyed
rows, obs, sids = run('fn1', '5 TP 9520 840', 200, '%d:-1' % LASER)
d = hp_drops(rows)
hit = [o for o in of(obs, LASER) if d and d[0][0] - 4 <= o['f'] < d[0][0] and o['dir'] == 2 and 9480 < o['x'] < 9530]
check('trafficle_laser_damage_10', bool(d) and d[0][1] == 2 and hit and not any(o['f'] >= d[0][0] and o['dir'] == 2 and o['a1'] == 0 for o in of(obs, LASER)), str(d))
# fire shield (OTHERS.B 2): the same low green laser passes without damage (activation 360 frames later: the same bob phase)
rows, obs, sids = run('fn1', '3 TP 9800 840;20 MON %d;22 24 X;365 TP 9520 840' % MON_FIRE, 520, '%d:-1' % LASER)
crossed = [o for o in of(obs, LASER) if o['f'] > 367 and abs(o['x'] - 9520) <= 8 and o['y'] > 815]
check('trafficle_fire_shield_immune', rows[364]['sh'] == 2 and crossed and not hp_drops(rows[365:]), 'sh %s crossed %d drops %s' % (rows[364]['sh'], len(crossed), hp_drops(rows[365:])))
# a jump attack hits the red light (HP 5) through the shared rules: defeat, fly-away, crash (shade + explosion, sound 50)
rows, obs, sids = run('fn1', '5 TP 9385 840;' + INV + ';20 30 J;28 30 X', 260, '%d:9420' % L1)
l1 = of(obs, L1)
check('trafficle_light_defeated', any(o['fl'] & DEFEATED for o in l1) and snd(sids, 24) and snd(sids, 23))
check('trafficle_light_crash_destroyed', snd(sids, 50) and l1[-1]['f'] < 259 and not (l1[-1]['fl'] & NOFLY), 'last seen %s' % l1[-1]['f'])
# Milla: her carried cube reflects the green laser (B += 180, col_13_29); the reflected laser hits the light for 10 (7344)
rows, obs, sids = run('fn1', '5 TP 9520 840;30 300 X', 200, '%d:9420,%d:-1' % (L3, LASER), ch=4)
refl = [o for o in of(obs, LASER) if o['fl'] & 0x4]
l3 = of(obs, L3)
check('trafficle_milla_cube_reflects_laser', refl and refl[0]['a1'] == 180 and not hp_drops(rows), 'reflected %d' % len(refl))
check('trafficle_reflected_laser_defeats_light', any(o['fl'] & DEFEATED for o in l3), 'A %s' % [o['A'] for o in l3][-1:])

# ==== 3. SuperTurretus (FN2 turret at (443, 1400), I = 1): parked above the map (Y -128) until its X is just off the view's
# left edge and the player is within 150 px of its home Y; flies in (sound 127), brakes, shoots 3 pellets (sound 28), leaves
# and parks again. The player hovers at the turret's height (TP every frame).
def hover(x, y, f0, f1): return ';'.join('%d TP %d %d' % (f, x, y) for f in range(f0, f1))
rows, obs, sids = run('fn2', hover(760, 1395, 5, 160) + ';' + INV, 400, '%d:443,%d:-1' % (STURRET, OT_PELLET))
st = of(obs, STURRET)
D = lambda f: [o['a0'] for o in st if o['f'] == f][0]
fly = [o['f'] for o in st if o['a0'] == 1]
check('sturret_parked_above_map', st[0]['y'] == -128 and st[0]['a0'] == 0)
check('sturret_flies_in', fly and snd(sids, 127) and [o['y'] for o in st if o['a0'] == 1][0] == 1400 and snd(sids, 127)[0] == fly[0], 'fly-in %s' % fly[:1])
shoot = [o['f'] for o in st if o['a0'] == 3]
pel = snd(sids, 28, shoot[0], shoot[-1] + 1) if shoot else []
check('sturret_brakes_then_shoots_3_pellets', any(o['a0'] == 2 for o in st) and len(pel) == 3, 'pellets %s' % pel)
check('sturret_pellets_fly_left', of(obs, OT_PELLET) and of(obs, OT_PELLET)[3]['x'] < of(obs, OT_PELLET)[0]['x'])
check('sturret_leaves_and_parks', any(o['a0'] == 4 for o in st) and st[-1]['a0'] == 0 and st[-1]['y'] == -128, 'last D %s y %s' % (st[-1]['a0'], st[-1]['y']))
# a pellet hits the player for 10 (no shield exception)
rows, obs, sids = run('fn2', hover(760, 1395, 5, 96) + ';' + hover(880, 1395, 96, 112), 160)
d = hp_drops(rows)
check('sturret_pellet_damage_10', bool(d) and d[0][1] == 2 and 95 < d[0][0] < 105, str(d))
# whips through the shared rules: defeat (sound 23), fly-away, the crash: keystone (direction 4), sound 29, 4 debris, destroyed
rows, obs, sids = run('fn2', hover(760, 1395, 5, 60) + ';' + hover(880, 1395, 60, 200) + ';' + INV + ';66 68 X', 300, '%d:443,%d:-1,%d:-1' % (STURRET, OT_KEYSTONE, OT_DEBRIS))
st = of(obs, STURRET)
crash = snd(sids, 29)
check('sturret_defeated', any(o['fl'] & DEFEATED for o in st) and snd(sids, 23))
check('sturret_crash_keystone_4_debris', crash and len([o for o in of(obs, OT_KEYSTONE) if o['f'] == crash[0]]) == 1 and
      len([o for o in of(obs, OT_DEBRIS) if o['f'] == crash[0]]) == 4 and st[-1]['f'] == crash[0] - 1, 'crash %s last %s' % (crash[:1], st[-1]['f']))
# Milla's shield (barrier in front of her) reflects a pellet (bounce, sound 219); the reflected pellet hits the turret for 10
rows, obs, sids = run('fn2', '5 TP 7677 660;30 400 B;' + hover(7770, 668, 88, 140), 200, '%d:7387,%d:-1' % (STURRET, OT_PELLET), ch=4)
pe = of(obs, OT_PELLET)
check('sturret_milla_reflects_pellet', any(o['fl'] & 0x4 for o in pe) and snd(sids, 219) and not hp_drops(rows))
check('sturret_reflected_pellet_defeats_it', any(o['fl'] & DEFEATED for o in of(obs, STURRET)) and snd(sids, 17))

# ==== 4. FlareCannon (FN2 cannon at (300, 1304), flag 16): counts while the player is within 199 px; fires 31 frames after,
# then every 231 (anim 7 forced by E / 4; the flare at E = 64, sound 46) an arcing flare toward the player's side
rows, obs, sids = run('fn2', '5 TP 180 1250;' + INV, 360, '%d:300,%d:-1' % (FCANNON, FLARE))
fc, fl_ = of(obs, FCANNON), of(obs, FLARE)
check('fcannon_noflag16_spawn', fc[0]['A'] == 25 and fc[0]['fl'] & NOFLY)
fires = snd(sids, 46)
check('fcannon_fires_every_231', len(fires) == 2 and fires[1] - fires[0] == 231 and fires[0] < 120, str(fires))
check('fcannon_flare_arcs_left', fl_ and fl_[0]['a2'] == -1.5 and fl_[0]['a4'] == 1 and fl_[5]['x'] < fl_[0]['x'] and min(o['y'] for o in fl_) < fl_[0]['y'] - 50)
check('fcannon_fn2_flares_pass_floors', not snd(sids, 29) and max(o['y'] for o in fl_) > 1400, 'max y %s' % max(o['y'] for o in fl_))   # FN2: no col_24_30 (level data)
# the flare detonates on the player (col_14_30: sound 29, puffs sound 430) and the blast hurts for 10
rows, obs, sids = run('fn2', '5 TP 245 1250', 220, '%d:-1' % FLARE)
d = hp_drops(rows)
check('fcannon_flare_detonates_on_player', snd(sids, 29) and snd(sids, 430) and any(o['a4'] == 2 for o in of(obs, FLARE)))
check('fcannon_flare_blast_damage_10', bool(d) and d[0][1] == 2, str(d))
# fire shield: the blast does not hurt (and the shield detonates flares, col_19_30)
rows, obs, sids = run('fn2', '3 TP 245 1250;20 MON %d;22 24 X' % MON_FIRE, 360, '%d:-1' % FLARE)
check('fcannon_fire_shield_immune', rows[200]['sh'] == 2 and snd(sids, 29, 100) and not hp_drops(rows), 'sh %s drops %s' % (rows[200]['sh'], hp_drops(rows)))
# FN3 (flares stop at walls, col_24_30): the arcing flare detonates on the floor
rows, obs, sids = run('fn3', '5 TP 6150 1330;' + INV, 200, '%d:-1' % FLARE)
fl_ = of(obs, FLARE)
blast = [o for o in fl_ if o['a4'] == 2]
check('fcannon_fn3_flare_hits_floor', snd(sids, 46) and blast and blast[0]['y'] > 1300 and abs(blast[0]['x'] - 6150) > 30 and max(o['y'] for o in fl_) < 1360,
      'fire %s blast at %s' % (snd(sids, 46), [(o['x'], o['y']) for o in blast[:1]]))
# whips defeat it (HP 25): keystone (direction 3, B -3), sound 29, 4 debris; no fly-away, never destroyed
rows, obs, sids = run('fn2', '5 TP 270 1250;' + INV + ';' + attacks(20, 400, 14), 420, '%d:300,%d:-1,%d:-1' % (FCANNON, OT_KEYSTONE, OT_DEBRIS))
fc = of(obs, FCANNON)
dfr = [o['f'] for o in fc if o['fl'] & DEFEATED]
ks = [o for o in of(obs, OT_KEYSTONE) if dfr and o['f'] == dfr[0]]
check('fcannon_defeated_by_whips', bool(dfr) and len(snd(sids, 24)) >= 6, 'hits %d' % len(snd(sids, 24)))
check('fcannon_wreck_keystone_debris', ks and ks[0]['a2'] == -3 and len([o for o in of(obs, OT_DEBRIS) if o['f'] == dfr[0]]) == 4 and snd(sids, 29, dfr[0], dfr[0]))
check('fcannon_stays_in_place', fc[-1]['f'] == 419 and fc[-1]['x'] == 300 and fc[-1]['y'] == 1304)

# ==== 5. Roller (FN4 roller at (495, 1224), start direction 16): wakes near the view, rolls toward the player (up to 5 px a
# frame), hops (sound 8), drops a pellet at the apex (sound 28, straight down), lands (sound 164)
rows, obs, sids = run('fn4', '5 TP 300 1200;' + INV, 200, '%d:495,%d:-1' % (ROLLER, OT_PELLET))
ro, pe = of(obs, ROLLER), of(obs, OT_PELLET)
check('roller_spawns_asleep', ro[0]['A'] == 5 and ro[0]['dir'] == 16 and ro[0]['x'] == 495)
check('roller_hops_and_lands', snd(sids, 8) and snd(sids, 164) and snd(sids, 8)[0] < snd(sids, 164)[0] and min(o['y'] for o in ro) < 1170)
check('roller_drops_pellet_at_apex', snd(sids, 28) and pe and pe[0]['x'] == pe[5]['x'] and pe[5]['y'] > pe[0]['y'], 'pellets %d' % len(pe))
xs = [o['x'] for o in ro]
check('roller_rolls_toward_player', max(abs(b - a) for a, b in zip(xs, xs[1:])) == 5 and all(b < a for a, b in zip(xs[80:110], xs[81:111])), 'x %s' % xs[80:110:10])
# its pellet hurts for 10
rows, obs, sids = run('fn4', '5 TP 488 1220', 120)
d = hp_drops(rows)
check('roller_pellet_damage_10', bool(d) and d[0][1] == 2, str(d))
# whips defeat it (HP 5); fly-away, crash (shade + explosion, sound 50), destroyed
rows, obs, sids = run('fn4', '5 TP 300 1200;' + INV + ';' + attacks(96, 130, 6), 300, '%d:495' % ROLLER)
ro = of(obs, ROLLER)
check('roller_defeated', any(o['fl'] & DEFEATED for o in ro) and snd(sids, 23))
check('roller_crash_destroyed', snd(sids, 50) and ro[-1]['f'] < 299 and ro[-1]['f'] + 1 in snd(sids, 50), 'last %s sid50 %s' % (ro[-1]['f'], snd(sids, 50)))

# ==== 6. GhettoShell (FN4 shell at (1263, 1232), flag 16): sways +-40 px, faces the player, blasts every 121 near-view frames
# (anim 10, sound 231 at frame 9) with 5 sound waves (one every 3 frames, 6 px sideways / 3 px up, 17 frames)
rows, obs, sids = run('fn4', '5 TP 1220 1200;' + INV, 300, '%d:1263,%d:-1' % (SHELL, WAVE))
sh = of(obs, SHELL); wv = [o for o in of(obs, WAVE) if o['dir'] == 16]
check('shell_sways', max(o['x'] for o in sh) - min(o['x'] for o in sh) > 30 and sh[0]['fl'] & NOFLY)
bl = sorted(set(snd(sids, 231)))   # (both FN4 shells near the start blast together)
busy = len([o for o in sh if o['a0'] == 1 and len(bl) > 1 and bl[0] < o['f'] <= bl[1]])   # F counts only while idle (D 0): 121 + the blast animation
check('shell_blasts_every_121', len(bl) >= 2 and abs(bl[1] - bl[0] - 121 - busy) <= 1, '%s busy %d' % (bl, busy))
first = [o for o in wv if o['f'] <= bl[0] + 16] if bl else []
check('shell_5_waves', bl and 4 <= len({o['f'] for o in first if o['a0'] == 0}) <= 5, str(sorted({o['f'] for o in first if o['a0'] == 0})))   # 5 unless the animation ends first (8287 H = 0; GT phase)
w0 = [o for o in wv if o['a0'] <= 16][:2]
check('shell_wave_moves', len(w0) == 2 and w0[1]['x'] - w0[0]['x'] == -6 and w0[1]['y'] - w0[0]['y'] == -3)
# a wave hurts for 10 (fire shield immune)
rows, obs, sids = run('fn4', '5 TP 1290 1200', 120)
d = hp_drops(rows)
check('shell_wave_damage_10', bool(d) and d[0][1] == 2, str(d))
rows, obs, sids = run('fn4', '3 TP 700 1200;20 MON %d;22 24 X;150 TP 1290 1200' % MON_FIRE, 330, '%d:-1' % WAVE)
check('shell_wave_fire_shield_immune', rows[149]['sh'] == 2 and of(obs, WAVE) and not hp_drops(rows), 'sh %s drops %s' % (rows[149]['sh'], hp_drops(rows)))
# whips defeat it (HP 25): no fly-away; 10 explosions (every 7 frames, sounds 311 / 319 / 320), then the keystone, sound 38,
# 4 debris (frames 60-57) and it goes
rows, obs, sids = run('fn4', '5 TP 1290 1200;' + INV + ';' + attacks(10, 300, 12), 420, '%d:1263,%d:-1,%d:-1' % (SHELL, OT_KEYSTONE, OT_DEBRIS))
sh = of(obs, SHELL)
dfr = [o['f'] for o in sh if o['fl'] & DEFEATED]
ex = [f for f, s in sids if s in (311, 319, 320) and dfr and f >= dfr[0]]
gone = sh[-1]['f'] + 1
check('shell_defeated_no_flyaway', bool(dfr) and abs(sh[-1]['y'] - 1232) < 1, 'defeat %s' % dfr[:1])
check('shell_10_explosions', len(ex) == 10 and all(b - a == 7 for a, b in zip(ex, ex[1:])), str(ex))
check('shell_keystone_sound38_debris_destroyed', snd(sids, 38, gone, gone) and [o for o in of(obs, OT_KEYSTONE) if o['f'] == gone] and
      len([o for o in of(obs, OT_DEBRIS) if o['f'] == gone]) == 4, 'gone %s sid38 %s' % (gone, snd(sids, 38)))

# ==== 7. Duster (FN4 duster at (3868, 1232), limiters at 3837 / 4181; arms and mouth chained through action points)
rows, obs, sids = run('fn4', '5 TP 3700 1200;' + INV, 700, '%d:3868,%d:3913,%d:3883' % (DUSTER, MOUTH, ARM2))
du, mo, a2 = of(obs, DUSTER), of(obs, MOUTH), of(obs, ARM2)
at72 = [o['x'] for o in du if o['f'] == 72]
check('duster_strides_137px', at72 and abs(at72[0] - (3868 + 137.5)) < 2, 'x at 72 %s' % at72)
check('duster_mouth_on_arm_chain', abs(mo[1]['x'] - 3913) < 2 and abs(mo[1]['y'] - 1230) < 2 and abs(a2[1]['x'] - 3881) < 3, 'mouth %s,%s arm2 %s' % (mo[1]['x'], mo[1]['y'], a2[1]['x']))
check('duster_nozzle_spares_itself', all(o['A'] == 30 for o in du))   # 8276 hits other Q20 enemies, never its own body here
check('duster_turns_at_limiter', max(o['x'] for o in du) < 4181 + 60 and any(o['a2'] == -1 for o in du) and du[-1]['x'] < max(o['x'] for o in du), 'max x %s' % max(o['x'] for o in du))
# the nozzle hurts for 5 (1 half-petal) a player right of mouth.x + 8 that is not moving right; metal shield immune
rows, obs, sids = run('fn4', '5 TP 3940 1220', 60)
d = hp_drops(rows)
check('duster_nozzle_damage_5', bool(d) and d[0][1] == 1, str(d))
rows, obs, sids = run('fn4', '5 TP 3800 1220;' + INV + ';12 40 R', 60)
check('duster_nozzle_not_from_behind', not hp_drops(rows))
rows, obs, sids = run('fn4', '3 TP 3300 1200;20 MON %d;22 24 X;150 TP 3940 1220' % MON_METAL, 240)
check('duster_nozzle_metal_shield_immune', rows[149]['sh'] == 3 and not hp_drops(rows), 'sh %s drops %s' % (rows[149]['sh'], hp_drops(rows)))
# whips defeat it (HP 30): sound 29, explosion, the crystal shower (MonitorIcon frame 2 / 2636), 4 more explosions; it stops
# (F = 0) and is never destroyed
rows, obs, sids = run('fn4', '5 TP 3830 1220;' + INV + ';' + attacks(8, 400, 12), 460, '%d:3868,%d:-1' % (DUSTER, OT_ITEM_FLY))
du = of(obs, DUSTER)
dfr = [o['f'] for o in du if o['fl'] & DEFEATED]
check('duster_defeated', bool(dfr) and snd(sids, 29, dfr[0], dfr[0] + 1), 'defeat %s' % dfr[:1])
check('duster_crystal_shower', dfr and len([o for o in of(obs, OT_ITEM_FLY) if o['f'] == dfr[0]]) >= 7)
check('duster_wreck_stays', du[-1]['f'] == 459 and du[-1]['a2'] == 0 and du[-1]['x'] == [o['x'] for o in du if o['f'] == dfr[0] + 2][0])

# ==== 8. Balloon (FN4 (1800, 584), FN5 (3608, 344); HP 1, flag 16): figure-eight drift; any hit pops it (never destroyed);
# FN4 drops a petal monitor (MonitorRings, falling: A = 1), FN5 gives an extra life (level data, 8168 / 8349)
rows, obs, sids = run('fn4', hover(1770, 600, 5, 40) + ';8 10 X', 400, '%d:1800,%d:-1' % (BALLOON, OT_MON_RINGS))
bl_ = of(obs, BALLOON)
check('balloon_figure_eight', max(o['x'] for o in bl_) - min(o['x'] for o in bl_) > 25 and max(o['y'] for o in bl_) - min(o['y'] for o in bl_) > 25)
pop = [o['f'] for o in bl_ if o['fl'] & DEFEATED]
mons = [o for o in of(obs, OT_MON_RINGS) if pop and o['f'] == pop[0] and abs(o['x'] - (bl_[0]['x'] - 16)) < 40]
check('balloon_pops_fn4_monitor', pop and snd(sids, 23) and mons and mons[0]['a0'] == 1, 'pop %s monitors %d' % (pop[:1], len(mons)))
check('balloon_stays_popped', bl_[-1]['f'] == 399 and bl_[-1]['fl'] & DEFEATED and bl_[-1]['an'] != bl_[0]['an'])
rows, obs, sids = run('fn5', hover(3578, 360, 5, 40) + ';8 10 X', 60, '%d:3608,%d:-1' % (BALLOON, OT_MON_RINGS))
pop = [o['f'] for o in of(obs, BALLOON) if o['fl'] & DEFEATED]
check('balloon_pops_fn5_extra_life', pop and rows[pop[0] + 1]['lv'] == rows[0]['lv'] + 1 and not [o for o in of(obs, OT_MON_RINGS) if o['f'] >= pop[0] and abs(o['x'] - 3592) < 80 and abs(o['y'] - 344) < 80], 'lives %s -> %s' % (rows[0]['lv'], rows[-1]['lv']))

# ==== 9. Pigeon (FN1 decor at (3188, 1032)): flies off when the player comes within 59 px (sound 243), climbs, goes home unseen
rows, obs, sids = run('fn1', '5 TP 3150 1020', 900, '%d:3188' % PIGEON)
pg = of(obs, PIGEON)
check('pigeon_scared_flies', snd(sids, 243) and pg[10]['x'] > 3188 and pg[10]['y'] < 1032 and pg[10]['a3'] == 1)
check('pigeon_returns_home', pg[-1]['x'] == 3188 and pg[-1]['y'] == 1032, 'last %s,%s D %s' % (pg[-1]['x'], pg[-1]['y'], pg[-1]['a3']))
check('pigeon_never_hurts', not hp_drops(rows))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
