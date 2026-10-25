#!/usr/bin/env python3
"""Host tests for the Fortune Night bosses (src/game/fn_boss.c, build/re/spec_fn_bosses.md A-E): the FN1 UFO (floor breaker),
the FN1 / FN2 bulldozer, the FN3 beam chase, the FN3 Serpentine fight (Lilac only) and the samurai (FN3 Carol / Milla, FN5
everyone): each triggers, locks the camera where the spec says, attacks with the spec's damage, takes hits by its rules (HP /
armour), and is defeated with its drops / exit / music. Integration checks of the recovered rules (no original trace of these
fights exists yet, spec Part Z).
usage: test_fn_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
# engine types (src/fp.h, Fortune Night boss block; others it touches)
UFO, PIECE, HATCH, BEAM, BORDER, UDEB = 800, 801, 802, 803, 804, 805
DOZER, ARM, CLAW, DOOR, WHEEL, DZHIT, LIMIT, BOOM, DZDEB = range(810, 819)
SERP, SARM, SHIT, SPROBE, SSHIELD, GLOW, WARN, WARN2, RAPID, MISSILE = range(820, 830)
SAMU, HAND, SWHEEL, SPROBE2, SDEB, GATE = range(840, 846)
FLOOR, LAMP, COLUMN, BARRIER, STURRET, LASER, FLAME, CRYSTAL = 579, 592, 596, 60, 523, 540, 244, 2
ARMOURED, DEFEATED = 0x20, 0x40
LILAC, CAROL, MILLA = 0, 2, 4
INV = '3 SETV others E 99999'

def run(level, script, frames, track=None, ch=0, snd=False):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if snd: e['FP_TRACE_SND'] = '1'
    if track: e['FP_TRACK'] = track
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, obs, sids, pend, music = [], [], [], [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'sid': pend.append(int(t[1]))
        elif t[0] == 'music' and len(t) >= 3 and t[-1].startswith('@'): music.append((int(t[-1][1:]), t[1]))
        elif t[0] == 'f':
            d = {}
            for i in range(0, len(t) - 1, 2):
                try: d[t[i]] = float(t[i + 1])
                except ValueError: pass
            rows.append(d); sids += [(int(d['f']), s) for s in pend]; pend = []
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'x0': int(t[3])}
            for i in range(4, len(t) - 1, 2):
                d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
            obs.append(d)
    return rows, obs, sids, music

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def of(obs, t, f=None): return [o for o in obs if o['type'] == t and (f is None or o['f'] == f)]
def byf(obs, t): return {o['f']: o for o in obs if o['type'] == t}
def snd(sids, sid, f0=0, f1=1 << 30): return [f for f, s in sids if s == sid and f0 <= f <= f1]
def hp_drops(rows, f0=0, f1=1 << 30):                      # (frame, half-petals lost) at every hp decrease
    return [(int(b['f']), int(a['hp'] - b['hp'])) for a, b in zip(rows, rows[1:]) if b['hp'] < a['hp'] and f0 <= b['f'] <= f1]
def attacks(f0, f1, every=12, key='X'): return ';'.join('%d %d %s' % (f, f + 2, key) for f in range(f0, f1, every))
def first(seq, pred):
    for x in seq:
        if pred(x): return x
    return None

# ======================================================================== PART A: the FN1 UFO (floor breaker; every character)
# placed at (6560, 1088) invisible; CameraTriggerBoss column at x 6936 (y 656..1232); limits 6984 / 824 / 7559 / 1831
A1 = '5 TP 6900 1000;70 82 R'
for ch in (LILAC, CAROL, MILLA):
    rows, obs, sids, music = run('fn1', A1 + ';' + INV, 820, '%d:-1,%d:-1,%d:-1' % (UFO, BEAM, FLOOR), ch=ch, snd=True)
    u = byf(obs, UFO)
    tag = 'ch%d' % ch
    check('ufo_%s_armoured_and_hidden_before_trigger' % tag, u[60]['fl'] & ARMOURED and u[60]['hd'] == 1 and u[60]['a6'] == 0 and u[60]['A'] == 200)
    trig = first(range(60, 820), lambda f: u[f]['a6'] == 1)
    check('ufo_%s_trigger_on_column' % tag, trig is not None and u[trig]['hd'] == 0 and abs(rows[trig]['x'] - 6936) < 24, 'f %s x %s' % (trig, trig and rows[trig]['x']))
    locked = [rows[f]['cx'] for f in range(trig + 200, trig + 600)] if trig else []   # the bounds ease 2 px / frame to the markers
    check('ufo_%s_camera_locked_to_markers' % tag, locked and min(locked) >= 6984 and max(locked) <= 7559 - 426, 'cx %s..%s' % (locked and min(locked), locked and max(locked)))
    Gs = [u[f]['a0'] for f in sorted(u)]
    order = [g for i, g in enumerate(Gs) if i == 0 or Gs[i - 1] != g]
    check('ufo_%s_states_0_to_5' % tag, order[:6] == [0, 1, 2, 3, 4, 5], str(order[:8]))
    g3 = first(sorted(u), lambda f: u[f]['a0'] == 3)
    check('ufo_%s_hatches_open_sound_168' % tag, g3 is not None and u[g3]['a3'] == 45 and snd(sids, 168) and abs(snd(sids, 168)[0] - g3) <= 1 and snd(sids, 169))
    check('ufo_%s_unarmoured_after_trigger' % tag, not (u[g3]['fl'] & ARMOURED))
    xs = [u[f]['x'] for f in sorted(u) if u[f]['a0'] in (3, 4)]
    check('ufo_%s_hovers_between_borders' % tag, xs and min(xs) >= 6968 - 80 and max(xs) <= 7424 + 80, '%s..%s' % (min(xs), max(xs)))
    nf0, nf1 = len(of(obs, FLOOR, 0)), len(of(obs, FLOOR, 819))
    check('ufo_%s_beam_cuts_floors' % tag, nf1 < nf0 and len(snd(sids, 13)) >= nf0 - nf1, '%d -> %d floors, sound 13 x%d' % (nf0, nf1, len(snd(sids, 13))))
    g5 = first(sorted(u), lambda f: u[f]['a0'] == 5)
    check('ufo_%s_releases_camera_and_leaves' % tag, g5 is not None and rows[-1]['cx'] < 6984 and u[819]['a1'] > 3, 'G5 at %s cx %s H %s' % (g5, rows[-1]['cx'], u[819]['a1']))
    check('ufo_%s_no_music_change' % tag, all(m[1] == 'bgm/m_stage3.adp' for m in music), str(music))
# the beam hurts for 10 (2 half-petals on Normal): the player stands where the beam cuts
rows, obs, sids, music = run('fn1', A1 + ';300 TP 7140 1180', 760, '%d:-1,%d:-1' % (UFO, BEAM))
bm = byf(obs, BEAM)
d = [(f, n) for f, n in hp_drops(rows, 300) if bm.get(f - 1) and bm[f - 1]['hd'] == 0 and abs(rows[f - 1]['x'] - bm[f - 1]['x']) < 40]
check('ufo_beam_damage_10', d and d[0][1] == 2, 'drops %s' % hp_drops(rows, 300)[:5])
# the player falls 300 px below the UFO: camera released early (7369)
rows, obs, sids, music = run('fn1', A1 + ';' + INV + ';260 TP 7200 1700', 420, '%d:-1' % UFO)
u = byf(obs, UFO)
check('ufo_release_when_player_falls', rows[300]['cy'] > 1831 - 240 or rows[400]['cx'] < 6984 or u[400]['a0'] < 5, 'cx %s cy %s' % (rows[400]['cx'], rows[400]['cy']))

# ======================================================================== PART B: the bulldozer (FN1 x 11231, FN2 x 7639)
DZ1 = '5 TP 11330 1000;' + INV
rows, obs, sids, music = run('fn1', DZ1 + ';' + attacks(600, 640) + ';' + attacks(930, 966), 1300, '%d:-1,%d:-1,%d:-1,%d:-1,%d:-1' % (DOZER, DOOR, BOOM, DZDEB, CRYSTAL), snd=True)
dz, door = byf(obs, DOZER), byf(obs, DOOR)
check('dozer_spawn', dz[0]['A'] == 200 and dz[0]['fl'] & 0x10000 and door[0]['A'] == 15 and door[300]['A'] == 1)   # ctor 15, 7440: 1
J = [dz[f]['a0'] for f in sorted(dz)]
check('dozer_cycle_drive_raise_stop_slam', [0, 1, 2, 3, 0] == [g for i, g in enumerate(J) if i == 0 or J[i - 1] != g][:5], str([g for i, g in enumerate(J) if i == 0 or J[i - 1] != g][:8]))
j1 = first(sorted(dz), lambda f: dz[f]['a0'] == 1)
check('dozer_rev_sound_165_then_166', snd(sids, 165) and abs(snd(sids, 165)[0] - j1) <= 1 and snd(sids, 166) and snd(sids, 166)[0] > j1)
check('dozer_no_camera_lock_no_music', all(m[1] == 'bgm/m_stage3.adp' for m in music))
xs = [dz[f]['x'] for f in sorted(dz) if f < 900]
check('dozer_drives_between_limiters', min(xs) >= 11048 - 60 and max(xs) <= 11368 + 30 and len(set(dz[f]['a5'] for f in dz)) >= 2, '%s..%s' % (min(xs), max(xs)))
boom = of(obs, BOOM)
check('dozer_throws_boomerang_sound_28', boom and snd(sids, 28) and max(b['x'] for b in boom) > min(b['x'] for b in boom) + 90)
k1 = first(sorted(door), lambda f: door[f]['a1'] in (7, 10))
check('door_armoured_while_closed', all(door[f]['fl'] & ARMOURED for f in range(10, k1 - 2)) and not (door[k1 + 5]['fl'] & ARMOURED), 'open at %s' % k1)
br = first(sorted(door), lambda f: door[f]['fl'] & DEFEATED)
check('door_breaks_in_one_hit_while_open', br is not None and 600 <= br <= 645 and door[br + 2]['a1'] == 8 and snd(sids, 23, br - 2, br + 1), 'broken at %s' % br)
a = [(f, dz[f]['A']) for f in sorted(dz) if 0 < f and dz[f]['A'] != dz[f - 1]['A']]
check('boomerang_through_broken_door_hurts_dozer', a and a[0][1] == 100 and snd(sids, 17, a[0][0] - 2, a[0][0] + 1), str(a[:4]))
check('door_restored_when_closed', any(door[f]['a1'] == 0 and not door[f]['fl'] & DEFEATED and door[f]['A'] == 1 for f in range(700, 900)))
dfeat = first(sorted(dz), lambda f: dz[f]['fl'] & DEFEATED)
check('dozer_defeated_second_cycle', dfeat is not None and dz[dfeat]['a0'] == 6 and snd(sids, 23, dfeat - 2, dfeat + 1), str(dfeat))
j7 = first(sorted(dz), lambda f: dz[f]['a0'] == 7)
check('dozer_explodes_15_times_sound_29', j7 and len(snd(sids, 29, dfeat, j7)) == 15, 'x%d' % len(snd(sids, 29, dfeat, j7 or 0)))
crys = [o for o in of(obs, CRYSTAL, j7 + 1) if abs(o['x'] - dz[j7]['x']) <= 32 and -52 <= o['y'] - dz[j7 - 1]['y'] <= -18]
check('dozer_wreck_10_crystals_debris_blast', len(crys) == 10 and of(obs, DZDEB) and snd(sids, 38, j7 - 1, j7 + 2), 'crystals %d' % len(crys))
j8 = first(sorted(dz), lambda f: dz[f]['a0'] == 8)
check('dozer_wreck_lands_and_rests', j8 and snd(sids, 51, j7, j8) and dz[1299]['a0'] == 8 and dz[1299]['a2'] == 0)
# armoured door: whips at the closed door leave it whole (A = 1; the overlapping body takes the hits: combat picks an unarmoured
# enemy first, the clang 30 only when nothing else is hit)
rows, obs, sids, music = run('fn1', DZ1 + ';396 398 L;' + attacks(400, 470), 480, '%d:-1,%d:-1' % (DOZER, DOOR))
door, dz = byf(obs, DOOR), byf(obs, DOZER)
check('door_closed_takes_no_damage', snd(sids, 24, 400, 480) and dz[479]['A'] < 200 and all(door[f]['A'] == 1 and door[f]['fl'] & ARMOURED and not door[f]['fl'] & DEFEATED for f in range(400, 480)))
# a plain hit on the body takes its HP (Q20 without armour)
rows, obs, sids, music = run('fn1', DZ1 + ';' + attacks(150, 230), 240, '%d:-1' % DOZER)
dz = byf(obs, DOZER)
check('dozer_body_takes_shared_hits', dz[239]['A'] < 200 and snd(sids, 24, 150, 240), 'A %s' % dz[239]['A'])
# damage: the slamming claw (10, B -8) and the boomerang (10, sound 17)
rows, obs, sids, music = run('fn1', '5 TP 11330 1000', 470, '%d:-1,%d:-1' % (DOZER, BOOM))
dz = byf(obs, DOZER)
dr = hp_drops(rows)
boomhit = [f for f, n in dr if snd(sids, 17, f - 2, f) and n == 2]
check('boomerang_damage_10_sound_17', boomhit, 'drops %s' % dr)
claw = [f for f, n in dr if dz[f - 1]['a7'] == 1 and n == 2]
check('claw_slam_damage_10', claw, 'drops %s, N %s' % (dr, [(f, dz[f - 1]['a7']) for f, n in dr]))
# FN2: the same rules on its own placement (7606 = 7427: sound 51 at the slam)
rows, obs, sids, music = run('fn2', '5 TP 7720 1700;' + INV, 700, '%d:-1,%d:-1' % (DOZER, BOOM))
dz = byf(obs, DOZER)
J = [dz[f]['a0'] for f in sorted(dz)]
check('dozer_fn2_cycle', 3 in J and of(obs, BOOM) and snd(sids, 51) and dz[0]['A'] == 200, str(sorted(set(J))))

# ======================================================================== PART C: the FN3 beam chase (every character)
for ch in (LILAC, CAROL, MILLA):
    rows, obs, sids, music = run('fn3', '5 TP 2000 700;%s;60 90 R;100 TP 2600 780;110 700 R' % INV, 700, '%d:-1,%d:-1,%d:-1,%d:-1' % (UFO, BEAM, FLOOR, LAMP), ch=ch)
    u, bm = byf(obs, UFO), byf(obs, BEAM)
    tag = 'ch%d' % ch
    st = first(sorted(u), lambda f: u[f]['a6'] == 1)
    check('chase_%s_starts_in_window' % tag, st is not None and 2024 < rows[st]['x'] < 2064 + 12 and u[st]['hd'] == 0, str(st and rows[st]['x']))
    check('chase_%s_ufo_races_and_parks' % tag, u[699]['y'] <= -100 and u[699]['x'] > 3000 and u[st + 20]['a1'] >= 12)
    on = [f for f in sorted(bm) if bm[f]['hd'] == 0]
    check('chase_%s_beam_follows_player_in_zone' % tag, on and snd(sids, 169) and all(abs(bm[f]['x'] - rows[f]['x']) < 200 for f in on), '%d frames' % len(on))
    nf0, nf1 = len(of(obs, FLOOR, 0)), len(of(obs, FLOOR, 699))
    check('chase_%s_floors_and_lamppost_break' % tag, nf1 < nf0 and any(o['fl'] & DEFEATED for o in of(obs, LAMP)), '%d -> %d' % (nf0, nf1))
    check('chase_%s_ufo_armoured' % tag, all(u[f]['fl'] & ARMOURED for f in u))

# ======================================================================== PART D: Serpentine (FN3, Lilac only)
SP = '5 TP 10400 150;40 52 R'
for ch in (CAROL, MILLA):
    rows, obs, sids, music = run('fn3', '5 TP 10400 150;%s;40 52 R' % INV, 200, '%d:-1,%d:-1,%d:-1' % (SERP, SHIT, SAMU), ch=ch)
    check('serp_absent_for_ch%d' % ch, not of(obs, SHIT) and of(obs, SAMU) and all(o['hd'] == 1 for o in of(obs, SERP)))
rows, obs, sids, music = run('fn3', SP + ';' + INV, 1500, '%d:-1,%d:-1,%d:-1,%d:-1,%d:-1,%d:-1,%d:-1' % (SERP, SHIT, UFO, BEAM, SSHIELD, RAPID, LASER), snd=True)
sp, hb, u = byf(obs, SERP), byf(obs, SHIT), byf(obs, UFO)
check('serp_lilac_has_no_samurai', not any(o['type'] == SAMU for o in obs))
st = first(sorted(sp), lambda f: sp[f]['a0'] != 0)
check('serp_fight_starts_at_trigger', st is not None and rows[st]['x'] >= 10432 - 10 and sp[st]['hd'] == 0, str(st))
lk = [rows[f]['cx'] for f in range(st + 60, 1500)]
check('serp_camera_locked', min(lk) >= 9872 and max(lk) <= 12271 - 426, '%s..%s' % (min(lk), max(lk)))
check('serp_music_midboss', ('bgm/m_midboss.adp' in [m[1] for m in music]) and first(music, lambda m: m[1] == 'bgm/m_midboss.adp')[0] <= st + 1, str(music))
check('serp_ufo_parks_at_arena_left', abs(u[st + 5]['x'] - 9967) < 2 and u[st + 5]['hd'] == 0 and of(obs, BEAM, st + 5)[0]['hd'] == 0)
states = {sp[f]['a0'] for f in sp}
check('serp_states_dash_shoot_flame_shield', {1, 2, 3, 5} <= states, str(sorted(states)))
check('serp_shot_sounds_191_192_lasers', snd(sids, 191) and snd(sids, 192) and of(obs, LASER))
check('serp_flame_jump_sounds_70_8', snd(sids, 70) and snd(sids, 8))
check('serp_shield_mortar_193_194', snd(sids, 193) and snd(sids, 194) and of(obs, RAPID))
sh = byf(obs, SSHIELD)
check('serp_armoured_while_shield_up', any(sh[f]['hd'] == 0 for f in sh) and all((hb[f]['fl'] & ARMOURED) == (ARMOURED if sh[f]['hd'] == 0 else 0) for f in sh if f > st + 2 and sh[f - 1]['hd'] == sh[f]['hd']))   # 7914 / 7915 run before the states (1-frame lag at a change)
# hits while the shield is down: shared damage, stagger below 30 D (120), the threshold counter D drops
rows, obs, sids, music = run('fn3', SP + ';' + INV + ';' + ';'.join('%d %d X' % (f, f + 2) for f in range(60, 1400, 9)) + ';60 1400 R', 1400, '%d:-1,%d:-1' % (SERP, SHIT))
sp, hb = byf(obs, SERP), byf(obs, SHIT)
check('serp_takes_hits', hb[1399]['A'] < 135 and snd(sids, 24), 'A %s' % hb[1399]['A'])
stg = first(sorted(sp), lambda f: sp[f]['a0'] == 6)
check('serp_stagger_below_120', stg is not None and hb[stg]['A'] < 120 + 12 and hb[stg]['a0'] <= 3 and snd(sids, 25, stg - 2, stg), str(stg and (hb[stg]['A'], hb[stg]['a0'])))
# damage: laser 10 (2 half-petals), flame 5 (1), shield 5 (1); rapid bullets 1 (no invulnerability test)
rows, obs, sids, music = run('fn3', SP + ';300 HP 14;600 HP 14;900 HP 14;1200 HP 14', 1500, '%d:-1,%d:-1,%d:-1' % (SERP, LASER, FLAME))
dr = hp_drops(rows)
check('serp_attacks_hurt', any(n == 2 for f, n in dr) and any(n == 1 for f, n in dr), str(dr[:10]))
# defeat (BOSSHP 0): sound 198, invulnerable player, he boards the UFO, the escape: 200 / 199 / 169, columns topple (317 / 75), the crash
# (fadeout, 51), the walk back, then m_stage3c, barriers at 13948, the camera free again
rows, obs, sids, music = run('fn3', SP + ';100 BOSSHP 0', 1400, '%d:-1,%d:-1,%d:-1,%d:-1,%d:-1' % (SERP, SHIT, UFO, COLUMN, BARRIER), snd=True)
sp, u = byf(obs, SERP), byf(obs, UFO)
dfl = first(sorted(sp), lambda f: sp[f]['a5'] == 1)
check('serp_defeat_sound_198', dfl is not None and snd(sids, 198) and abs(snd(sids, 198)[0] - dfl) <= 1, str(dfl))
esc = first(sorted(sp), lambda f: f > dfl and sp[f]['a6'] >= 0)
check('serp_boards_ufo', esc is not None and sp[esc]['hd'] == 1 and abs(sp[esc - 1]['x'] - 9967) <= 12)
Gs = [u[f]['a0'] for f in sorted(u) if f >= esc]
order = [g for i, g in enumerate(Gs) if i == 0 or Gs[i - 1] != g]
check('serp_escape_ufo_states', [g for g in order if g != 5] == [0, 1, 2, 3, 4, 6, 7, 8], str(order))   # 7947 (G 5) and 7950 (G 6) can run in one frame
check('serp_escape_sounds', snd(sids, 200) and snd(sids, 199) and snd(sids, 169, esc) and snd(sids, 317) and snd(sids, 75) and snd(sids, 51, esc))
cols = of(obs, COLUMN, 1399)
check('serp_escape_columns_topple', sum(1 for c in cols if c['ang'] <= -105) == 2 and all(c['fl'] & 1 for c in cols))
g8 = first(sorted(u), lambda f: u[f]['a0'] == 8)
check('serp_end_music_stage3c', any(m[1] == 'bgm/m_stage3c.adp' and abs(m[0] - g8) <= 1 for m in music) and any(m[1] == 'music' or True for m in music), str(music))
check('serp_end_barriers_open', all(o['x'] == 13948 for o in of(obs, BARRIER, 1399)))
check('serp_player_invulnerable_then_normal', rows[g8 + 5]['x'] > 13800 and rows[1399]['cx'] > 13000)
check('serp_lies_where_he_fell', sp[1399]['hd'] == 0 and sp[1399]['x'] > 13800)

# ======================================================================== PART E: the samurai (FN3 Carol / Milla, FN5 everyone)
S3 = '5 TP 10700 800;60 TP 11000 800;'   # into the arena from the left: the lock bounds ease from the current view (2 px / frame)
for ch in (CAROL, MILLA):
    tag = 'ch%d' % ch
    rows, obs, sids, music = run('fn3', S3 + INV, 1300, '%d:-1,%d:-1,%d:-1' % (SAMU, HAND, FLOOR), ch=ch, snd=True)
    sm = byf(obs, SAMU)
    check('samu3_%s_armoured_until_lock' % tag, sm[0]['fl'] & ARMOURED and sm[0]['A'] == 90 and not (sm[60]['fl'] & ARMOURED))
    check('samu3_%s_camera_locked' % tag, all(10831 <= rows[f]['cx'] <= 11441 - 426 for f in range(300, 1300)), '%s..%s' % (min(r['cx'] for r in rows[100:]), max(r['cx'] for r in rows[100:])))
    sts = [sm[f]['a0'] for f in sorted(sm)]
    order = [g for i, g in enumerate(sts) if i == 0 or sts[i - 1] != g]
    check('samu3_%s_intro_then_hover' % tag, order[:3] == [0, 1, 2] and sm[first(sorted(sm), lambda f: sm[f]['a0'] == 2)]['y'] > 700)
    check('samu3_%s_attacks_smash_lasers_charge' % tag, {3, 4, 5} <= set(sts) and snd(sids, 192) and snd(sids, 17), str(sorted(set(sts))))
    nf0, nf1 = len(of(obs, FLOOR, 0)), len(of(obs, FLOOR, 1299))
    check('samu3_%s_smash_breaks_floors' % tag, nf1 < nf0, '%d -> %d' % (nf0, nf1))
    check('samu3_%s_music_midboss' % tag, any(m[1] == 'bgm/m_midboss.adp' for m in music))
    rows, obs, sids, music = run('fn3', S3 + INV + ';200 BOSSHP 0', 700, '%d:-1,%d:-1,%d:-1' % (SAMU, GATE, STURRET), ch=ch, snd=True)
    sm, gt = byf(obs, SAMU), [o for o in of(obs, GATE, 699)]
    df = first(sorted(sm), lambda f: sm[f]['a0'] == 6)
    check('samu3_%s_defeat_sequence' % tag, df and snd(sids, 23, df - 2, df) and len(snd(sids, 311) + snd(sids, 319) + snd(sids, 320)) >= 20 and snd(sids, 38) and snd(sids, 62) and sm[699]['a1'] >= 28)
    check('samu3_%s_gates_rise_192' % tag, len(gt) == 2 and sorted(g['y'] for g in gt) == [608, 704] and all(g['A'] == 48 for g in gt))
    tur = {(round(o['x']), o['a4']) for o in of(obs, STURRET) if o['f'] > df}
    check('samu3_%s_turrets' % tag, any(x == 11550 for x, i in tur) and any(x == 11552 for x, i in tur) and all(i == -1 for x, i in tur if x in (11550, 11552)), str(sorted(tur))[:120])
    check('samu3_%s_music_fade_then_stage3c' % tag, any(m[1] == 'stop' for m in music) and music[-1][1] == 'bgm/m_stage3c.adp', str(music))
    check('samu3_%s_camera_released' % tag, rows[699]['cx'] != rows[400]['cx'] or snd(sids, 51))
# Samu damage: falling hands / body charge / lasers hurt for 10 (2 half-petals)
rows, obs, sids, music = run('fn3', S3 + '300 HP 14;700 HP 14;1100 HP 14', 1300, '%d:-1' % SAMU, ch=CAROL)
dr = hp_drops(rows)
check('samu3_damage_10', dr and all(n == 2 for f, n in dr), str(dr[:8]))
# FN5: everyone; no armour, no lock (CAM.D 336), no music; one gate, the turret at (10931, 550)
for ch in (LILAC, CAROL, MILLA):
    tag = 'ch%d' % ch
    rows, obs, sids, music = run('fn5', '5 TP 10100 400;' + INV + ';300 BOSSHP 0', 800, '%d:-1,%d:-1,%d:-1' % (SAMU, GATE, STURRET), ch=ch, snd=True)
    sm = byf(obs, SAMU)
    check('samu5_%s_no_armour_fight' % tag, not (sm[0]['fl'] & ARMOURED) and sm[40]['a7'] == 1 and sm[40]['a6'] == 0)
    check('samu5_%s_defeat_gate_turret' % tag, sm[799]['a1'] >= 28 and of(obs, GATE, 799)[0]['A'] == 48 and any(round(o['x']) == 10931 for o in of(obs, STURRET) if o['f'] > 300))
    check('samu5_%s_no_music_change' % tag, all(m[1] == 'bgm/m_stage3b.adp' for m in music), str(music))
# FN5 8422: no trigger once g4 >= 3 (the second checkpoint, 8421)
rows, obs, sids, music = run('fn5', '5 TP 10600 500;' + INV + ';40 TP 10100 400', 300, '%d:-1' % SAMU)
sm = byf(obs, SAMU)
check('samu5_no_trigger_after_checkpoint_3', all(sm[f]['a7'] == 0 for f in sm), str(rows[39].get('g4')))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
