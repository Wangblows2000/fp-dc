#!/usr/bin/env python3
"""Host tests for the Jade Creek bosses of acts 1-4 (session 21; src/game/jc_boss.c; build/re/spec_jc_bosses.md): the JC1
Hovercop fly-by, the JC2 Hovercop chase, the JC3 chase + Neera (ride, trigger, intro, fight, shields, rings, hits, defeat, the
act-3 hand-off, the leave) and JC4 (nothing), per character, against the original's traces (RV numbers, build/orig_traces/
jc_bosses: a1_look, a2_look, a3_run, a3_kill, a3_kill_c, a3_run_m, a3_hits). The port's player cannot reach the JC3 arena on
this branch (the route's platforms are other modules'), so the scripts teleport it there; the JC3 numbers are relative to the
trigger frame (10589, MasterLevel Y 1), the JC2 ones to the first missile (165).
usage: test_jc_bosses.py [data_dir]   (default: cv)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_JCB_SET', 'FP_DIFF')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACE_SID': '1', 'FP_DBG_JCB': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, ev, jcb = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'voice', 'jcbmusic'):
            ev.append((len(rows), line))                     # (the frame the event happened in)
        elif t[0] == 'jcb':
            jcb[int(t[1]) - 1] = {t[i]: float(t[i + 1]) if t[i] not in ('nfl', 'hfl') else int(t[i + 1], 16) for i in range(2, len(t) - 1, 2)}
    return rows, ev, jcb
# frames: the row / event frame (the 'f' rows); the jcb lines print G.frame (one more) and are keyed one less; FP_JCB_SET pokes at
# G.frame, so a poke for row frame f is written f + 1 (setv)
def setv(*pk): return ','.join('%d:%s' % (f + 1, v) for f, v in pk)

def sids(ev, *ids): return [f for f, l in ev if l.startswith('sid ') and int(l.split()[1]) in ids]
def near(a, b, tol=1): return len(a) == len(b) and all(abs(x - y) <= tol for x, y in zip(a, b))
def first(d, cond, after=-1): return next((f for f in sorted(d) if f > after and cond(d[f])), None)
def changes(d, key, t0):
    out, prev = [], None
    for f in sorted(d):
        if f < t0: continue
        v = int(d[f][key])
        if v != prev: out.append((f - t0, v)); prev = v
    return out

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 500, 1000, 1500, 2000, 2500))
JUMPS = ''.join('%d %d J\n' % (f, f + 18) for f in range(130, 1500, 45))

# ---- JC4: no boss (RE: handle_frame_43 has no boss event; its CameraTriggerBoss is a template) ----
rows, ev, d = run('jc4', 0, 30, INV)
check('jc4_no_boss', len(rows) == 30 and not d, str(len(d)))

# ---- JC1: the fly-by (10347-10357). The script runs the player across the bridge at 5 px a frame (the bridge is jc_gim's).
# RE: X > CameraTriggerBoss.x (11552) starts it; 10353 sound 276 on that frame (RV a1_look: 276 at 166, the group at 167);
# 10354 / 10355: once x > VIEWX + 80, five BossMeteors and sounds 99 every 10 frames (g3 % 10); 10357: the rig gone at VIEWX + 466
tp = ''.join('%d TP %d 1130\n' % (f, 11300 + 5 * (f - 100)) for f in range(100, 600, 4))
rows, ev, d = run('jc1', 0, 600, INV + '60 TP 11300 1000\n' + tp + '100 1400 R\n')
trig = first(d, lambda r: r['hcx'] < 13000)
s99 = sids(ev, 99)
check('jc1_flyby_start', trig is not None and sids(ev, 276) == [trig] and d[trig]['px'] > 11552 and d[trig - 1]['px'] <= 11552, str((trig, sids(ev, 276))))
check('jc1_meteors', len(s99) == 5 and all(b - a == 10 for a, b in zip(s99, s99[1:])) and (s99[0] + 1) % 10 == 0 and trig + 20 <= s99[0] <= trig + 120 and
      max(r['met'] for r in d.values()) >= 1, str(s99))
gone = first(d, lambda r: r['gone'] == 1)
check('jc1_rig_gone', gone is not None and abs(gone - (trig + 329)) <= 40 and d[gone - 1]['hcx'] > 0, str((trig, gone)))   # RV a1_look: the group off at 496 (167 + 329)
check('jc1_rig_speed', all(abs((d[f + 1]['A'] - d[f]['A']) - 4) < 1e-6 for f in range(trig + 1, trig + 30)), '')   # A + 4 a frame (10348)

# ---- JC2: the chase (RV a2_look, TP 7700 1500 at 60, holding right: 277 once, then the cycle: 4 crystals (77 at -90, -75, -60,
# -45 from the missile), the missile 165, the hook down 166 (+27), released 195 (+54), its burst 77 + 98 (+75); cycles 255
# frames apart; after the third (O > 2, flag 6) it flies off to the right; it cannot be damaged (no qualifier)
for ch, cn in CHARS:
    rows, ev, d = run('jc2', ch, 1300, INV + '60 TP 7700 1500\n100 3000 R\n')
    m = sids(ev, 165)
    check('jc2_%s_cycles' % cn, len(m) >= 3 and near([b - a for a, b in zip(m, m[1:3])], [255, 255], 2), str(m))
    c0 = m[0] if m else 0
    check('jc2_%s_missile' % cn, m and near(sids(ev, 166)[:1], [c0 + 27], 1) and near(sids(ev, 195)[:1], [c0 + 54], 1) and
          [f for f in sids(ev, 98) if c0 + 60 <= f <= c0 + 130][:1], str((sids(ev, 166)[:2], sids(ev, 195)[:2], sids(ev, 98)[:2])))   # (the burst: where it passes the player)
    lands = [f for f in sids(ev, 77) if c0 - 100 <= f < c0]
    check('jc2_%s_crystals' % cn, near(lands, [c0 - 90, c0 - 75, c0 - 60, c0 - 45], 4), str(lands))
    check('jc2_%s_once_277' % cn, len(sids(ev, 277)) == 1 and sids(ev, 277)[0] < c0, str(sids(ev, 277)))
    last = d[max(d)]
    check('jc2_%s_leaves' % cn, last['O'] >= 3 and last['D'] == 1 and last['hcx'] > last['px'] + 300, str((last['O'], last['hcx'], last['px'])))

# ---- JC3 (RV a3_run / a3_kill / a3_kill_c / a3_run_m / a3_hits; the trigger frame t0) ----
JC3 = INV + '60 TP 6450 1500\n470 TP 8600 1440\n100 3000 R\n' + JUMPS
# Neera's state E from the trigger: the intro (E 0..7 every 16 frames), the fight from +122: 0 fall, 1 stand, 3 shield, 2 ring,
# 4 jump, 1, 2, 4 ... (RV per character; the port's animation counters drift by up to 5 frames)
ORIG_E = {'lilac': [(0, 0), (10, 1), (26, 2), (42, 3), (58, 4), (74, 5), (90, 6), (106, 7), (122, 0), (146, 1), (176, 3), (209, 2), (241, 4), (318, 1), (349, 2), (383, 4), (460, 1), (491, 2), (526, 4)],
          'carol': [(0, 0), (10, 1), (26, 2), (42, 3), (58, 4), (74, 5), (90, 6), (106, 7), (122, 0), (146, 1), (176, 3), (211, 2), (242, 4), (319, 1), (350, 2), (385, 4)],
          'milla': [(0, 0), (10, 1), (26, 2), (42, 3), (58, 4), (74, 5), (90, 6), (106, 7), (122, 0), (146, 1), (176, 3), (209, 2), (240, 4), (317, 1), (348, 2), (383, 4), (460, 1), (491, 2), (526, 4)]}
for ch, cn in CHARS:
    rows, ev, d = run('jc3', ch, 1200, JC3)
    # the ride (10710-10720): Neera shown with the chase (10666), E 2 when the Hovercop parks at 8084, E 3 +30 (up), E 4 +16 (hidden)
    e1 = first(d, lambda r: r['nE'] == 1 and r['hid'] == 0)
    e2 = first(d, lambda r: r['nE'] == 2 and r['ride'] == 1); e3 = first(d, lambda r: r['nE'] == 3 and r['ride'] == 1); e4 = first(d, lambda r: r['nE'] == 4 and r['ride'] == 1)
    check('jc3_%s_ride' % cn, e1 and e2 and d[e1]['ch'] == 1 and abs(d[e2]['A'] - 8084) < 1e-6 and e3 - e2 == 30 and e4 - e3 == 16 and d[e4]['hid'] == 1, str((e1, e2, e3, e4)))
    t0 = first(d, lambda r: r['ml'] == 1)
    check('jc3_%s_trigger' % cn, t0 is not None and d[t0]['intro'] == 1 and d[t0]['ride'] == 0 and [f for f, l in ev if l.startswith('jcbmusic')] == [t0], str(t0))   # slot 4 asked (ControllerMusic, 10654)
    if t0 is None: continue
    he = changes(d, 'nE', t0)[:len(ORIG_E[cn])]
    check('jc3_%s_states' % cn, len(he) == len(ORIG_E[cn]) and all(a[1] == b[1] and abs(a[0] - b[0]) <= 6 for a, b in zip(he, ORIG_E[cn])), str(he))
    s33 = [f for f in sids(ev, 33) if f > t0]
    check('jc3_%s_fight_start' % cn, near(sids(ev, 298), [t0 + 122], 2) and any(abs(f - (t0 + 146)) <= 2 for f in s33), str((sids(ev, 298), s33[:3])))   # (Carol's moves play 33 too)
    sh = [f for f in sids(ev, 0, 57, 3, 2, 1) if f > t0]
    check('jc3_%s_shield' % cn, near(sh[:1], [t0 + 185], 4) and max(r['sh'] for r in d.values()) >= 1, str(sh[:2]))
    b31 = [f for f in sids(ev, 31) if f > t0]
    check('jc3_%s_ring_bounces' % cn, b31[:4] and near(b31[:4], [t0 + 286, t0 + 286, t0 + 365, t0 + 365], 6), str(b31[:6]))
    check('jc3_%s_no_voice' % cn, not [l for f, l in ev if l.startswith('voice n') or l.startswith('voice ne_')], '')   # 10783's loop n_hit_<0|1> does not exist

# the kill (RV a3_kill: A 0 at t0 + 405): 23 + 25 that frame, the knock-back, E 8 with 33 + 299 at +35, the act-3 hand-off
# (10772: g4 10) at +66, the fight's flags off (ML.Y 0), the Hovercop leaves (E + 0.2 to 10) and Neera rides it from +183
for ch, cn in CHARS:
    rows0, ev0, d0 = run('jc3', ch, 800, JC3)
    t0 = first(d0, lambda r: r['ml'] == 1)
    k = t0 + 405
    rows, ev, d = run('jc3', ch, k + 260, JC3, {'FP_JCB_SET': setv((k, 'A:0'))})
    check('jc3_%s_kill' % cn, near(sids(ev, 23), [k], 1) and near(sids(ev, 25), [k], 1) and d[k + 2]['nE'] == 7, str((sids(ev, 23), sids(ev, 25))))
    e8 = first(d, lambda r: r['nE'] == 8, k)
    # (RV: E 8 at +36 (Lilac) / +44 (Carol): the knock-back's length depends on where she was hit)
    check('jc3_%s_down' % cn, e8 and k + 25 <= e8 <= k + 50 and sids(ev, 299) == [e8] and e8 in sids(ev, 33), str((e8, sids(ev, 299))))
    g10 = first(d, lambda r: r['g4'] == 10, k)
    check('jc3_%s_act3_cleared' % cn, g10 and e8 and g10 - e8 == 30 and d[g10]['ml'] == 0 and d[g10]['fight'] == 0 and d[g10]['leave'] == 1, str((e8, g10)))   # RV +30 (F > 30)
    ride = first(d, lambda r: r['leave'] == 1 and r['nE'] == 0, g10 or k)
    check('jc3_%s_leave' % cn, ride and g10 and abs(ride - (g10 + 117)) <= 6 and d[max(d)]['E'] == 10 and d[ride]['ny'] == d[ride]['hcy'] - 50, str((g10, ride)))   # RV a3_kill: 1066 -> 1183

# shields and hits (RV a3_hits: C 5 at +195 (shield fresh: L > 0, nothing), +227 / +267 (it holds: 315, M - 1, L 30), +307
# (it breaks: 11, C 30); A 100 at +367 (< D 35 = 105: 296, D 2, knock-back, the next jump's ice rain 297 at +433); A 60 at
# +575 (296, D 1); A 0 at +805: 23 + 25, then 33 + 299 at +844; the second shield (fl 5 cleared by the hit) at +549
for ch, cn in CHARS[:1]:
    rows0, ev0, d0 = run('jc3', ch, 800, JC3)
    t0 = first(d0, lambda r: r['ml'] == 1)
    pk = setv(*((t0 + o, v) for o, v in ((195, 'C:5'), (227, 'C:5'), (267, 'C:5'), (307, 'C:5'), (367, 'A:100'), (575, 'A:60'), (805, 'A:0'))))
    rows, ev, d = run('jc3', ch, t0 + 900, JC3, {'FP_JCB_SET': pk})
    check('jc3_%s_shield_holds' % cn, near(sids(ev, 315), [t0 + 227, t0 + 267], 1) and d[t0 + 228]['nM'] == 1 and d[t0 + 268]['nM'] == 0, str(sids(ev, 315)))
    check('jc3_%s_shield_breaks' % cn, near(sids(ev, 11), [t0 + 307], 1) and d[t0 + 308]['sh'] == 0 and d[t0 + 308]['hC'] >= 28, str(sids(ev, 11)))
    check('jc3_%s_thirds' % cn, near(sids(ev, 296), [t0 + 367, t0 + 575], 1) and d[t0 + 368]['hD'] == 2 and d[t0 + 576]['hD'] == 1, str(sids(ev, 296)))
    check('jc3_%s_ice_rain' % cn, near(sids(ev, 297), [t0 + 433], 5) and max(r['cr'] for f, r in d.items() if t0 + 433 < f < t0 + 520) >= 3, str(sids(ev, 297)))
    sh = [f for f in sids(ev, 0, 57, 3, 2, 1) if f > t0]
    check('jc3_%s_second_shield' % cn, len(sh) >= 2 and abs(sh[1] - (t0 + 549)) <= 6, str(sh))
    check('jc3_%s_hits_defeat' % cn, near(sids(ev, 23), [t0 + 805], 1) and near(sids(ev, 299), [t0 + 844], 5), str((sids(ev, 23), sids(ev, 299))))

# damage (RE 10561 / 10792, no RV: the traces keep the player invulnerable): without OTHERS.E, the rings (10) and the grown ice
# spikes (20) hurt
rows, ev, d = run('jc3', 0, 1000, '60 TP 6450 1500\n470 TP 8600 1440\n100 3000 R\n' + JUMPS)
t0 = first(d, lambda r: r['ml'] == 1)
hp = {int(r['f']): r['hp'] for r in rows}
check('jc3_damage', t0 and hp.get(t0 + 400, 99) < hp.get(t0, 0), str((hp.get(t0), hp.get(t0 + 400))))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
