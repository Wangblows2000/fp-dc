#!/usr/bin/env python3
"""Host tests for the Pangu Lagoon bosses (session 23; src/game/pl_boss.c; build/re/spec_pl_bosses.md): the Holo-Dragon of PL6, for
Lilac (the stage's only character), against the original's traces (build/orig_traces/pl_bosses: p1 passive with invulnerability, p3
jump-attacks, p4 / p6 the Eye2 phase, the defeat and the ending, p7 passive without invulnerability). The scripts teleport like the
trace scripts (TP 5000 1700 at 60: the arena's left end); the port applies a TP one frame earlier (port frame = RV frame - 1).
usage: test_pl_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(frames, script, env=None, level='pl6'):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_PLB_O')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': '0', 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_DBG_PLB': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    rows, ev, d = [], [], {}
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('sid', 'music', 'plboss', 'plflow') or t[0].startswith('load['):
            ev.append((len(rows), line))
        elif t[0] == 'plb' and len(t) > 2:
            d[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, d

def sids(ev, *ids):
    return [f for f, l in ev if l.startswith('sid ') and int(l.split()[1]) in ids]
def first_sid(ev, i): s = sids(ev, i); return s[0] if s else None
def tag(ev, word): return [int(l.split('@')[1]) for f, l in ev if l.startswith('plboss ' + word)]
def near(a, b, tol=2): return a is not None and b is not None and abs(a - b) <= tol
def changes(d, key):
    out, prev = [], None
    for f in sorted(d):
        v = d[f].get(key)
        if v != prev: out.append((f, v)); prev = v
    return out
def fv(d, f, k): return float(d[f][k]) if f in d and k in d[f] else None

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 400, 800, 1200, 1600, 2000, 2400, 2800, 3200, 3600, 4000, 4400, 4800, 5200, 5600, 6000, 6400))
TP = '60 TP 5000 1700\n'
JX = lambda a, b: ''.join('%d %d J\n%d %d X\n' % (f, f + 14, f + 6, f + 9) for f in range(a, b, 36))   # the traces' jump-attacks

# ---- 1. passive (RV p1): the countdown from the arena entry (13333: M -360 -> 120), the fight start (13336), the entrance
# (L 255 -> 0, the roar), the flight, the laser (push 10 px a frame), the sounds at their RV frames
rows, ev, d = run(1600, INV + TP)
rv = {333: 539, 330: 792, 331: 933, 200: 1226, 169: 1374, 332: 1478}
for sid, f in rv.items():
    check('p1_sound_%d' % sid, near(first_sid(ev, sid), f - 1, 3), '%s vs RV %d' % (first_sid(ev, sid), f))
st = [(f, v) for f, v in changes(d, 'K')]
check('p1_states', [v for _, v in st][:4] == ['-2', '-1', '0', '1'], str(st[:6]))
check('p1_fight_start', near(tag(ev, 'fight')[0] if tag(ev, 'fight') else None, 539, 3), str(tag(ev, 'fight')))
check('p1_entrance_D', fv(d, 600, 'D') == 5154.0, str(fv(d, 600, 'D')))                    # RV p1: D = player X + 100 = 5154
check('p1_fade_in', near(fv(d, 700, 'L'), 94, 3) and fv(d, 800, 'L') == 0, '%s %s' % (fv(d, 700, 'L'), fv(d, 800, 'L')))   # RV L 94 at 700
check('p1_flight', near(fv(d, 1099, 'D'), 3406.5 + 15, 40) and fv(d, 1100, 'I') == -15.0, '%s %s' % (fv(d, 1099, 'D'), fv(d, 1100, 'I')))   # RV D 3406.5 at 1100
check('p1_turn_laser', near(fv(d, 1350, 'D'), 4734, 2) and fv(d, 1350, 'J') == 1.0, str(fv(d, 1350, 'D')))                  # RV D 4734 (X - 320 J)
xs = [rows[i]['x'] for i in range(1380, 1450) if i < len(rows) and 'x' in rows[i]]
check('p1_laser_push', len(xs) > 10 and xs[-1] - xs[0] > 400, '%s -> %s' % (xs[:1], xs[-1:]))                              # RV 5054 -> 5786 (10 a frame)
check('p1_next_attack', fv(d, 1520, 'N') == 2.0 and fv(d, 1520, 'K') == 0.0, str(d.get(1520, {}).get('N')))
music = [l for f, l in ev if l.startswith('music bgm/m_boss4')]
check('p1_music_slot4', len(music) == 1 and near(first_sid(ev, 330), [f for f, l in ev if l.startswith('music bgm/m_boss4')][0], 1), str(music))

# ---- 2. damage (RV p7, no invulnerability from the fight): the laser hurts (sound 25 every 5 frames in RV), the claws (10), the flames
rows, ev, d = run(2700, '3 SETV others E 99999\n300 SETV others E 99999\n700 SETV others E 99999\n940 SETV others E 0\n' + TP)
h = sids(ev, 25)
lz = [f for f in h if 1360 <= f <= 1470]
check('p7_laser_burns', len(lz) >= 6 and all(4 <= b - a <= 7 for a, b in zip(lz, lz[1:4])), str(lz))     # RV p7: 25 every 5 frames from 1376
hp0, hp1 = fv(d, 1370, 'hp'), fv(d, 1470, 'hp')
check('p7_laser_damage', hp0 is not None and hp1 is not None and 2 <= hp0 - hp1 <= 9, '%s -> %s (RV 14 -> 5 half-petals)' % (hp0, hp1))
check('p7_claw_hurts', any(1860 <= f <= 1950 for f in h), str([f for f in h if f > 1800][:6]))            # RV p7: 1868, 1929
check('p7_flame_hurts', any(2350 <= f <= 2550 for f in h), str([f for f in h if f > 2300][:6]))           # RV p7: 2375, 2440

# ---- 3. Lilac's real attacks destroy hit spots (RV p3: O 23 at 2900, 59 at 7000 with the same jump-attack script)
rows, ev, d = run(4000, INV + TP + JX(950, 3990))
o = fv(d, 3999, 'O') or 0
check('p3_spots_by_attacks', o >= 10, 'O %s (RV p3: 35 at 4000)' % o)
check('p3_spot_sound_45', len(sids(ev, 45)) == o, '%d sounds, O %s' % (len(sids(ev, 45)), o))
check('p3_cycle', [v for _, v in changes(d, 'K')][3:9] == ['1', '0', '2', '1', '0', '3'], str(changes(d, 'K')[:10]))   # laser, claws, flames (13358 / 13383 / 13389)

# ---- 4. Eye2 with real attacks (RV p4: 85 spots set, Eye2 A 20 -> 0 in 1500 frames of jump-attacks, defeat 334), the ending, the results
rows, ev, d = run(6000, INV + TP + JX(950, 5990), {'FP_PLB_O': '950'})
df = tag(ev, 'defeat'); rs = tag(ev, 'results')
check('p4_eye2_shows', any(d[f].get('e2h') == '0' for f in d if f > 960), '')
check('p4_eye2_by_attacks', len(df) == 1 and first_sid(ev, 334) is not None, str(df))
if df:
    check('p4_flash_76', near(first_sid(ev, 76), df[0] + 399, 3), '%s vs defeat + 399 (RV p4 3148 -> 3547)' % first_sid(ev, 76))
    check('p4_pangu_51', first_sid(ev, 51) is not None and 230 <= first_sid(ev, 51) - df[0] - 400 <= 300, '%s (RV p4: K 7 + 263)' % first_sid(ev, 51))
    check('p4_explosions', len([f for f in sids(ev, 311, 99) if df[0] < f < df[0] + 400]) >= 40, '%d (RV p4: 49)' % len(sids(ev, 311, 99)))
    check('p4_results', len(rs) == 1 and near(rs[0], df[0] + 821, 3), '%s vs defeat + 821 (RV p4 / p6)' % rs)
    check('p4_lilac_scene', any(abs(r.get('x', 0) - 6000) < 1 for r in rows[df[0] + 430: df[0] + 500]), '')
check('p4_wipe_to_frontend', any(l.startswith('load[frontend]') for f, l in ev), '')

# ---- 4b. the flow's arena entry point (13237: MV (6080, 24)), no invulnerability: she falls to the floor, the countdown, the fight
rows, ev, d = run(800, '60 TP 6080 24\n')
fs = tag(ev, 'fight')
check('arena_entry_fight', len(fs) == 1 and 530 <= fs[0] <= 560 and near(fv(d, fs[0] + 5, 'D'), 6180, 2), '%s D %s' % (fs, fv(d, fs[0] + 5, 'D') if fs else None))
check('arena_entry_input_lock', fs and all(abs(rows[i].get('x', 0) - rows[fs[0]].get('x', 0)) < 1 for i in range(fs[0], min(fs[0] + 300, len(rows)))), '')

# ---- 5. a forced end (BOSSHPT: every spot then Eye2), no other boss in PL1-PL5 (no PL_BOSS type placed)
rows, ev, d = run(2100, INV + TP + '1000 BOSSHPT 2236 0\n1000 BOSSHPT 2237 0\n1000 BOSSHPT 2238 0\n1100 BOSSHPT 2261 0\n')
check('force_defeat', tag(ev, 'defeat') and near(tag(ev, 'defeat')[0], 1101, 1), str(tag(ev, 'defeat')))
check('force_results', tag(ev, 'results') and near(tag(ev, 'results')[0], 1101 + 820, 2), str(tag(ev, 'results')))
for lv in ('pl1', 'pl3', 'pl5'):
    rows, ev, d = run(30, '', level=lv)
    check('%s_no_dragon' % lv, not d, '')

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
