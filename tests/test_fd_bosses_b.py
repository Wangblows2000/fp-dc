#!/usr/bin/env python3
"""Host tests for the Final Dreadnought bosses of rounds 3-4 (session 24; src/game/fd_boss_r34.c, fd_boss_r34_3b.c, fd_boss_r34_4b.c,
tools/import_frame.py FD_BOSS B; spec build/re/spec_fd_bosses_b.md; RV traces build/orig_traces/fd_bosses_b/):
  - FD3b: the MinibossBulldozer is FN's (fn_boss.c on the FN_BOSS types mapped in json 71); the Kingdom Stone scene (Lilac / Carol: stone
    D 0..8, RE 16237-16394; RV b2_0 frame counts), the mutated Milla fight (five lives of A 60: 16576 / 16577; RV b2_0), its defeat and the
    results -> FD4a; Milla's Classic path (16395-16399: no scene, CTRL.D for 90 frames, Sign.B 179 -> the results; RV b2_4).
  - FD4b: the intro (16830-16847: MV.Y <= 336, the walk to Brevon.x - 192, the mech at E 60, Brevon gone), the mech (BG4's Absolution with
    FD's numbers: hits on the weak spot, the defeat at E 0 -> checkpoint g4 2, F 50 and the cut scene 0x230f4), the suit (P lives, the
    head's hits), its defeat (g4 3, L 10 -> 11 -> 12) -> the results -> results_request_end -> the ending; for Lilac, Carol and Milla.
usage: test_fd_bosses_b.py [data_dir]   (default: converted2; the session-24 worktrees build Final Dreadnought into cv)
Host-only knobs of the module: FP_DBG_F3B / FP_DBG_F4B (state lines), FP_F3B_LIVES, FP_F4B_HITS, FP_F4B_LIVES (shorter fights)."""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = (('lilac', 0), ('carol', 2), ('milla', 4))
REF = 14                                                 # ORIG_REF_DY (port y = MV.Y + 14)
INV = ''.join('%d SETV others E 99999\n' % f for f in range(3, 9000, 200))
DOZER = 810

def attacks(f0, f1, every=12, key='X'): return ''.join('%d %d %s\n' % (f, f + 2, key) for f in range(f0, f1, every))

def run(level, ch, script, frames, env=None, track=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ)
    for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TRACK', 'FP_F3B_LIVES', 'FP_F4B_HITS', 'FP_F4B_LIVES', 'FP_SAVE', 'FP_FILE', 'FP_FILE_NEW'): e.pop(k, None)
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_DBG_F3B': '1', 'FP_DBG_F4B': '1', 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    out = r.stdout + r.stderr
    rows, b3, b4, obs, sids, loads = [], [], [], [], [], []
    for line in out.splitlines():
        t = line.split()
        if not t: continue
        if t[0] in ('f3b', 'f4b'):
            d = {'f': int(t[1])}
            for i in range(2, len(t) - 1, 2):
                try: d[t[i]] = float(t[i + 1])
                except ValueError: pass
            (b3 if t[0] == 'f3b' else b4).append(d)
        elif t[0] == 'f':
            d = {}
            for i in range(0, len(t) - 1, 2):
                try: d[t[i]] = float(t[i + 1])
                except ValueError: pass
            rows.append(d)
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2])}
            for i in range(4, len(t) - 1, 2):
                try: d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
                except ValueError: pass
            obs.append(d)
        elif t[0] == 'sid': sids.append((len(rows), int(t[1])))
        elif t[0].startswith('load['):
            lv = t[0][5:].split(']')[0]
            if lv not in ('boot', 'frontend', 'gal'): loads.append(lv)
    return dict(rows=rows, b3=b3, b4=b4, obs=obs, sids=sids, loads=loads, out=out)

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def first(seq, pred):
    for x in seq:
        if pred(x): return x
    return None
def changes(seq, key):
    out = []
    for d in seq:
        if not out or out[-1] != d[key]: out.append(d[key])
    return out

CONFIRM = lambda f0, f1: ''.join('%d %d J\n' % (f, f + 2) for f in range(f0, f1, 40))

# ======================================================================== FD3b
# ---- the bulldozer: FN's MinibossBulldozer object (same handles and events 7404-7464) on fn_boss.c (json 71 maps FN_BOSS's classes)
r = run('fd3b', 0, INV + '5 TP 1420 600\n' + attacks(120, 400), 600, track='%d:-1' % DOZER)
dz = [o for o in r['obs'] if o['type'] == DOZER]
check('fd3b_dozer_is_fn_bulldozer', dz and dz[0]['A'] == 200 and dz[0]['fl'] & 0x10000, str(dz[:1] and (dz[0]['A'], hex(dz[0]['fl']))))
check('fd3b_dozer_runs_its_cycle', dz and len(set(o['a0'] for o in dz)) >= 2, str(sorted(set(o['a0'] for o in dz))))

# ---- the Kingdom Stone scene + the mutated Milla (Lilac / Carol) -> the results -> FD4a
SC3 = INV + '50 TP 12900 1290\n100 400 R\n' + attacks(1950, 4400) + ''.join('%d %d J\n' % (f, f + 8) for f in range(1990, 4400, 90))
for nm, ch in CHARS[:2]:
    r = run('fd3b', ch, SC3 + CONFIRM(4400, 5600), 5600, {'FP_F3B_LIVES': '1'})
    b3 = r['b3']
    sc = first(b3, lambda d: d['sc'] == 1)
    check('fd3b_scene_starts_near_stone_%s' % nm, sc and 13312 - 200 - 60 <= sc['px'] <= 13312 - 200 + 10 and 120 <= sc['f'] <= 200,
          str(sc and (sc['f'], sc['px'])))                              # RV b2_0: f 155 at x 13112
    ds = changes([d for d in b3 if d['sc'] == 1], 'D')
    check('fd3b_scene_D_0_to_8_%s' % nm, ds == list(range(9)), str(ds))
    d7 = first(b3, lambda d: d['D'] == 7); d8 = first(b3, lambda d: d['D'] == 8)
    check('fd3b_scene_timeline_rv_%s' % nm, d7 and d8 and abs(d7['f'] - 1260) <= 80 and abs(d8['f'] - 1760) <= 80,
          str(d7 and d8 and (d7['f'], d8['f'])))                         # RV b2_0: D 7 at f 1260, D 8 at f 1760
    fi = first(b3, lambda d: d['fi'] == 1)
    check('fd3b_fight_starts_%s' % nm, fi and abs(fi['f'] - 1931) <= 100, str(fi and fi['f']))   # RV b2_0: E 171 / tailend D 2 at ~1931
    hit = first(b3, lambda d: d['fi'] == 1 and d['A'] < 60)
    check('fd3b_head_takes_hits_%s' % nm, hit is not None, '')
    res = first(b3, lambda d: d['res'] == 1)
    check('fd3b_defeat_results_%s' % nm, res and first(b3, lambda d: d['defF'] > 0) and res['f'] - first(b3, lambda d: d['defF'] > 0)['f'] >= 600,
          str(res and res['f']))
    check('fd3b_results_to_fd4a_%s' % nm, r['loads'] == ['fd3b', 'fd4a'], str(r['loads']))
# five lives of A 60 (ctor O 5; 16577: A 60 again, O - 1; RV b2_0 f 3100 -> 3200)
r = run('fd3b', 0, SC3, 4400)
a = [d['A'] for d in r['b3'] if d['fi'] == 1]
check('fd3b_head_refills_a_life', any(x < 20 for x in a) and any(a[i] == 60 and a[i - 1] < 60 for i in range(1, len(a))), str(a[-3:]))
d0 = first(r['b3'], lambda d: d['defF'] > 0)
refills = sum(1 for i in range(1, len(a)) if a[i] == 60 and a[i - 1] < 60)
check('fd3b_head_five_lives', d0 is None or refills >= 4, 'refills %d' % refills)

# ---- Milla (16237 returns for her; 16395 Classic: past the stone, CTRL.D 90 frames, Sign.B 179 -> the results; RV b2_4)
r = run('fd3b', 4, INV + '50 TP 12900 1290\n100 900 R\n' + CONFIRM(900, 2400), 2400)
check('fd3b_milla_no_scene_no_fight', all(d['sc'] == 0 and d['fi'] == 0 for d in r['b3']), '')
res = first(r['b3'], lambda d: d['res'] == 1)
check('fd3b_milla_results_after_90', res and res['px'] > 13312, str(res and (res['f'], res['px'])))
check('fd3b_milla_to_fd4a', r['loads'] == ['fd3b', 'fd4a'], str(r['loads']))

# ======================================================================== FD4b
# the arena (camera limits 1856..3263); the player is put on its floor left of the lift shaft (the act's route up is the gimmicks').
# The whole sequence runs with the module's host knob FP_F4B_AUTOHIT (a 4-point hit on the suit's head / Brevon every n frames); the
# shared combat rules' hits on them are checked separately below with teleports next to them (TPO; RV b4_0: a player standing still
# lands one hit on the suit every ~800 frames).
SC4 = INV + '20 TP 2300 290\n' + attacks(300, 12000) + ''.join('%d %d J\n' % (f, f + 6) for f in range(345, 12000, 90))
for nm, ch in CHARS:
    milla = ch == 4
    env = {'FP_F4B_HITS': '0' if milla else '2', 'FP_F4B_LIVES': '2', 'FP_F4B_BA': '24', 'FP_F4B_AUTOHIT': '20'}
    r = run('fd4b', ch, SC4, 12000, env)
    b4 = r['b4']
    i2 = first(b4, lambda d: d['ph'] == 2)
    check('fd4b_intro_trigger_%s' % nm, i2 and i2['py'] - REF <= 336 + 1, str(i2 and (i2['f'], i2['py'] - REF)))
    d2 = first(b4, lambda d: d['ph'] == 2 and d['iD'] == 2)
    check('fd4b_intro_walk_to_brevon_%s' % nm, d2 and abs(d2['px'] - (2687 - 192)) <= 6, str(d2 and d2['px']))   # 16833: Brevon.x - 192
    e60 = first(b4, lambda d: d['iE'] >= 60 and d['F'] == 1)
    check('fd4b_mech_rises_at_e60_%s' % nm, e60 and e60['iE'] == 60, str(e60 and e60['iE']))
    m = first(b4, lambda d: d['ph'] == 3)
    check('fd4b_mech_fight_%s' % nm, m and d2 and m['f'] - d2['f'] >= 91, str(m and m['f']))
    if not milla:   # (RV b3_4: Milla's attacks with this input never reach the weak spot in the original either)
        fl = changes([d for d in b4 if d['ph'] == 3], 'F')
        check('fd4b_mech_flies_lands_jumps_%s' % nm, fl[:4] == [1, 2, 0, 3], str(fl[:8]))
        hit = first(b4, lambda d: d['ph'] == 3 and d['wE'] < 2)
        check('fd4b_weak_spot_takes_hits_%s' % nm, hit is not None, '')
    md = first(b4, lambda d: d['ph'] == 4)
    g4 = [x['g4'] for x in r['rows'] if md and x.get('f', 0) > md['f']][:1]
    check('fd4b_mech_defeat_checkpoint_g4_2_%s' % nm, md and md['dead'] == 1 and g4 == [2], str((md and md['f'], g4)))
    su = first(b4, lambda d: d['ph'] == 5)
    check('fd4b_cut_scene_then_suit_%s' % nm, su and md and 590 <= su['f'] - md['f'] <= 640, str(su and md and su['f'] - md['f']))
    sd = first(b4, lambda d: d['ph'] == 6)
    g4 = [x['g4'] for x in r['rows'] if sd and x.get('f', 0) > sd['f']][:1]
    check('fd4b_suit_defeat_checkpoint_g4_3_%s' % nm, sd and g4 == [3], str((sd and sd['f'], g4)))
    sl = changes([d for d in b4 if d['ph'] in (6, 7)], 'sL')
    res = first(b4, lambda d: d['res'] == 1)
    if milla:   # RV b5_4: Milla's suit end goes L 11 -> 12 -> the results (no Brevon fight)
        check('fd4b_suit_end_L10_11_12_milla', sl[:3] == [10, 11, 12], str(sl))
        check('fd4b_milla_results_after_L12', res and sd and 360 + 120 + 120 <= res['f'] - sd['f'] <= 360 + 120 + 140 and
              first(b4, lambda d: d['ph'] == 8) is None, str(res and sd and res['f'] - sd['f']))
    else:       # RV b4_0 / b4_2 / b5_0: L 11 runs to M 280 -> Brevon (scene ~110 frames, the fight, the finish, the results)
        check('fd4b_suit_end_L10_11_brevon_%s' % nm, sl[:2] == [10, 11] and 12 not in sl, str(sl))
        bs = first(b4, lambda d: d['ph'] == 7); bf = first(b4, lambda d: d['ph'] == 8)
        check('fd4b_brevon_scene_%s' % nm, bs and sd and bf and 360 + 280 - 5 <= bs['f'] - sd['f'] <= 360 + 280 + 5 and 105 <= bf['f'] - bs['f'] <= 115,
              str(bs and bf and sd and (bs['f'] - sd['f'], bf['f'] - bs['f'])))
        bd = changes([d for d in b4 if d['ph'] == 8], 'bD')
        check('fd4b_brevon_attacks_%s' % nm, len(set(bd) & {1, 2, 3, 5, 6, 7, 8}) >= 3, str(bd[:12]))
        d60 = first(b4, lambda d: d['bD'] == 60); d62 = first(b4, lambda d: d['bD'] == 62)
        check('fd4b_brevon_defeat_finish_%s' % nm, d60 and d62 and 105 <= d62['f'] - d60['f'] <= 115, str(d60 and d62 and d62['f'] - d60['f']))
        e_res = 520 if ch == 2 else 620                                        # 17394 (Lilac's 0x230fe) / 17408 (Carol's 0x230f0)
        check('fd4b_results_at_stone_e_%s' % nm, res and d60 and abs(res['f'] - d60['f'] - e_res) <= 3, str(res and d60 and res['f'] - d60['f']))
    check('fd4b_ending_%s' % nm, 'ending begin chr %d' % ch in r['out'], '')

# the act's own way in (the lift under the arena: FD_GIM / FD_FLOW), RV b1_0: the intro at f ~634, the walk left to x 2491
r = run('fd4b', 0, INV + '60 560 R\n', 900)
i2 = first(r['b4'], lambda d: d['ph'] == 2); d2 = first(r['b4'], lambda d: d['ph'] == 2 and d['iD'] == 2)
check('fd4b_intro_from_the_lift_lilac', i2 and 600 <= i2['f'] <= 700 and d2 and abs(d2['px'] - 2491) <= 6, str(i2 and d2 and (i2['f'], d2['px'])))

# the shared combat rules on the suit's head (Q20) and Brevon's hitbox (Q20, A 160): restarts + teleports next to them (Lilac / Carol:
# Milla's tapped attacks reach neither the weak spot nor the head, in the original either: RV b3_4)
for nm, ch in CHARS[:2]:
    r = run('fd4b', ch, INV + '20 TP 2300 290\n' + ''.join('%d TPO 3002 -10 10\n' % f for f in range(700, 1500, 7)) + attacks(700, 1500), 1500, {'FP_G4': '2'})
    check('fd4b_suit_head_takes_hits_%s' % nm, first(r['b4'], lambda d: d['ph'] == 5 and (d['hA'] < 10 or d['sP'] < 7)) is not None, '')
    if True:
        r = run('fd4b', ch, INV + '20 TP 2300 290\n' + ''.join('%d TPO 2993 -20 0\n' % f for f in range(320, 1300, 9)) + attacks(320, 1300), 1300, {'FP_G4': '3'})
        check('fd4b_brevon_takes_hits_%s' % nm, first(r['b4'], lambda d: d['ph'] == 8 and d['bA'] < 160) is not None, '')

# restarts (start events 17474 / 17475): g4 2 -> the beaten mech's exit and the suit; g4 3 -> L 11 -> Brevon (Lilac) / the results (Milla)
r = run('fd4b', 0, INV + '20 TP 2300 290\n', 1200, {'FP_G4': '2'})
b4 = r['b4']
check('fd4b_restart_g4_2_suit', b4 and b4[0]['ph'] == 3 and b4[0]['dead'] == 1 and first(b4, lambda d: d['ph'] == 5) is not None, str(b4[:1] and b4[0]['ph']))
r = run('fd4b', 0, INV + '20 TP 2300 290\n', 600, {'FP_G4': '3'})
check('fd4b_restart_g4_3_brevon', first(r['b4'], lambda d: d['ph'] == 8) is not None, '')
r = run('fd4b', 4, INV + '20 TP 2300 290\n', 400, {'FP_G4': '3'})
check('fd4b_restart_g4_3_milla_results', first(r['b4'], lambda d: d['res'] == 1) is not None, '')

print('%d failures' % fails)
sys.exit(1 if fails else 0)
