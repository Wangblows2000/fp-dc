#!/usr/bin/env python3
"""Host tests for the Trap Hideout bosses (session 21b; src/game/th_boss.c, th_boss_iris.c, th_boss_basher.c, th_boss_arena.c and
the TH3 variant of sb_spade.c; build/re/spec_th_bosses.md). Each encounter from entry to defeat to progression, for Carol and Milla
(Trap Hideout's characters), against the original's traces (RV numbers below, build/orig_traces/th_bosses: i1_c / i2_m the Laser
Iris, c1_c / c2_c the Crystal Basher, a1_c the arena and Spade). The port's scripts teleport like the trace scripts; the port applies
a TP one frame earlier, so a port frame is the trace frame - 1 (TH1 / the basher) or - 2 (the arena: TP at 59 for the trace's 60).
usage: test_th_bosses.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
DBG = ('thi', 'thc', 'tha', 'sbp')

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_THA_A3')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1', 'FP_TRACE_HUD': '1',
              'FP_DBG_THI': '1', 'FP_DBG_THC': '1', 'FP_DBG_THA': '1', 'FP_DBG_SBP': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=e)
    os.unlink(name)
    rows, ev, dbg, loads = [], [], {}, []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
        elif t[0] in ('sid', 'music', 'thflow', 'voice', 'hud'):
            ev.append((len(rows), line))                     # (the frame the event happened in)
        elif t[0].startswith('load['):
            loads.append((len(rows), t[0][5:-1]))
        elif t[0] in DBG and len(t) > 2:
            dbg.setdefault(t[0], {})[int(t[1])] = {t[i]: t[i + 1] for i in range(2, len(t) - 1, 2)}
    return rows, ev, dbg, loads

def sids(ev, *ids):
    out = []
    for f, l in ev:
        if l.startswith('sid ') and int(l.split()[1]) in ids and (f, int(l.split()[1])) not in out: out.append((f, int(l.split()[1])))
    return [f for f, _ in out]
def music(ev, name): return [f for f, l in ev if l.startswith('music ' + name)]
def huds(ev): return [(f, float(l.split()[2])) for f, l in ev if l.startswith('hud ')]
def near(a, b, tol=1): return len(a) == len(b) and all(abs(x - y) <= tol for x, y in zip(a, b))
def first(d, cond): return next((f for f in sorted(d) if cond(d[f])), None)
def changes(d, key):
    out, prev = [], None
    for f in sorted(d):
        v = d[f].get(key)
        if v != prev: out.append((f, v)); prev = v
    return out

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
INV = ''.join('%d SETV others E 99999\n' % f for f in (3, 400, 800, 1200, 1600, 2000, 2400, 2800))
RV = lambda xs, off=1: [x - off for x in xs]

# ---- TH1: the Laser Iris (13502-13531). RV i1_c (Carol: TP 2200 2300 at 60, right 100-440, TP 5150 2340 at 450, A 0 at 1300) and
# i2_m (Milla, jumping / attacking, A 0 at 1500): the flaps (sound 336), the beam (169 at each sweep start), the trigger shake
# (23 x3), the defeat: explosions (311 / 319 / 320 every 7 frames), 38 at Q 16, the railings' crystals and sound 40 31 frames later
IRIS = '60 TP 2200 2300\n100 440 R\n450 TP 5150 2340\n'
for ch, cn, kill, extra, rv_las, rv_38, rv_40 in (
        (2, 'carol', 1300, '', [580, 831, 1082], 1406, 1437),
        (4, 'milla', 1500, ''.join('%d %d J\n%d %d X\n' % (f, f + 14, f + 6, f + 10) for f in range(560, 1400, 45)), [580, 831, 1082, 1333], 1606, 1637)):
    rows, ev, d, _ = run('th1', ch, kill + 200, INV + IRIS + extra + '%d BOSSHPT 1680 0\n' % kill)
    s = d.get('thi', {})
    check('th1_%s_flaps' % cn, near(sids(ev, 336)[:3], RV([164, 185, 206])), str(sids(ev, 336)[:3]))
    check('th1_%s_trigger_shake' % cn, len([f for f in sids(ev, 23) if f < 400]) == 3, str(sids(ev, 23)))
    check('th1_%s_beam' % cn, near(sids(ev, 169), RV(rv_las)), str(sids(ev, 169)))
    st = [v for _, v in changes(s, 'I')]
    check('th1_%s_states' % cn, st[:3] == ['3', '4', '5'] and st[-1] == '6' and st.count('3') >= 4, str(st))   # (the debug lines start with the fight)
    # (the explosions run on g3 mod 7: their phase against the defeat frame is the port's frame counter, INFERRED; Carol's run is
    # exact, Milla's 3 frames late)
    check('th1_%s_defeat' % cn, near([f for f in sids(ev, 23) if f >= kill - 2], [kill], 1) and near([f for f in sids(ev, 38)][:1], RV([rv_38]), 4),
          str((sids(ev, 23), sids(ev, 38)[:2])))
    check('th1_%s_explosions' % cn, len([f for f in sids(ev, 311, 319, 320) if kill <= f < rv_38]) >= 14, str(len(sids(ev, 311, 319, 320))))
    ra = first(s, lambda v: v.get('ra') == '0')
    check('th1_%s_arena_open' % cn, near(sids(ev, 40), RV([rv_40]), 4) and ra is not None and abs(ra - rv_40) <= 4, str((sids(ev, 40), ra)))
    if ch == 2:   # RV i1_c state / position, frames 460-1180 (the trace's iris columns; exact)
        try:
            import csv
            r = csv.reader(open(os.path.join(HERE, '..', 'build', 'orig_traces', 'th_bosses', 'i1_c.csv'))); h = next(r)
            ix = {k: h.index('iris_' + k) for k in ('x', 'y', 'I', 'J')}
            bad = n = 0
            for row in r:
                f = int(row[0]) - 1
                if 460 <= f <= 1180 and f in s:
                    n += 1; bad += any(abs(float(s[f][k]) - float(row[i])) > 0.01 for k, i in ix.items())
            check('th1_carol_rv_i1_c', n > 600 and bad == 0, '%d frames, %d differ' % (n, bad))
        except (OSError, ValueError, StopIteration):
            print('SKIP th1_carol_rv_i1_c (no trace)')

# ---- TH3: the Crystal Basher (13880-13935). RV c2_c (Carol, TP 420 390 / 420 300, the face A 0 at 1000): spikes (132 at each
# extension), swings (74), defeat 23, the parts fly at E 120 (38), the railings at E > 160 (40) and the flow's end (g4 5)
BASH = '60 TP 420 390\n66 TP 420 300\n'
for ch, cn in ((2, 'carol'), (4, 'milla')):
    rows, ev, d, _ = run('th3', ch, 1250, INV + BASH + '999 BOSSHPT 1686 0\n')
    s = d.get('thc', {})
    check('th3_basher_%s_spikes' % cn, near(sids(ev, 132), RV([85, 247, 317, 518, 588, 807, 840, 876, 912])), str(sids(ev, 132)))
    check('th3_basher_%s_swings' % cn, near(sids(ev, 74), RV([207, 478])), str(sids(ev, 74)))
    check('th3_basher_%s_defeat' % cn, near(sids(ev, 23), RV([1000])) and near(sids(ev, 38), RV([1119])) and near(sids(ev, 40), RV([1160])),
          str((sids(ev, 23), sids(ev, 38), sids(ev, 40))))
    cl = [f for f, l in ev if l.startswith('thflow th3 basher cleared')]
    check('th3_basher_%s_cleared' % cn, near(cl, RV([1160]), 1) and s.get(1200, {}).get('g4') == '5', str(cl))

# ---- TH3: the arena (14047-14064) and Spade (sb_spade.c drop-in variant, 13936-14043). RV a1_c (Carol, g4 5, TP 11700 690 at
# 60, the goal A 3 at 500, Spade's hitbox A 0 at 1100): the goal at 61 (sound 363), A 2 at 181, a ninja dropped every 16 frames
# from 76 (D 99 -> 84, 15 up: the landings' sound 61 at 105 + 16 k), A 4 151 frames after A 3 (m_boss2: g4 5), Spade drops in
# (F -1 -> 0 at 741, 2 at 771, 4 at 824, back to 0 at 841: jump sound 8 at 770 / 871 / 992, dash 155 at 895 / 909), beaten
# (156 at 1100, F 6 at 1101, F 9 at 1104), the results (Sign.B 179 at about 1330) and the stage end (the wipe, sound 56)
ARENA = '5 THBASH\n59 TP 11700 690\n'
rows, ev, d, loads = run('th3', 2, 1900, INV + ARENA + '1098 BOSSHPT 1222 0\n', {'FP_THA_A3': '499'})
a, sp = d.get('tha', {}), d.get('sbp', {})
ach = changes(a, 'A')
check('th3_arena_carol_goal', [v for _, v in ach][:5] == ['0', '1', '2', '3', '4'] and near([f for f, _ in ach[1:3]], RV([61, 181], 2)), str(ach))
check('th3_arena_carol_goal_sound', near(sids(ev, 363)[:1], RV([60], 2)), str(sids(ev, 363)))
check('th3_arena_carol_drops', near(sids(ev, 61), RV([105 + 16 * k for k in range(15)], 2)) and a.get(400, {}).get('D') == '84' and a.get(400, {}).get('K') == '99',
      str((sids(ev, 61), a.get(400))))
check('th3_arena_carol_boss_music', near(music(ev, 'bgm/m_boss2'), RV([651], 2), 2), str(music(ev, 'bgm/m_boss2')))
fch = [(f, v) for f, v in changes(sp, 'F') if v != '-1']
check('th3_spade_carol_states', near([f for f, v in fch[:5]], RV([741, 771, 824, 841, 872], 2)) and [v for _, v in fch[:5]] == ['0', '2', '4', '0', '2'], str(fch[:6]))
check('th3_spade_carol_drop_x', abs(float(sp.get(700, {}).get('A', 0)) - 11864) < 0.5, str(sp.get(700)))
check('th3_spade_carol_sounds', near(sids(ev, 8)[:3], RV([770, 871, 992], 2)) and near(sids(ev, 155)[:2], RV([895, 909], 2)), str((sids(ev, 8), sids(ev, 155))))
f6, f9 = first(sp, lambda v: v.get('F') == '6'), first(sp, lambda v: v.get('F') == '9')
check('th3_spade_carol_beaten', near(sids(ev, 156), RV([1100], 2)) and f6 is not None and abs(f6 - 1099) <= 1 and f9 is not None and abs(f9 - 1102) <= 1, str((sids(ev, 156), f6, f9)))
res = next((i for i, r in enumerate(rows) if r.get('res', 0) > 0), None)
check('th3_carol_results', res is not None and abs(res - 1328) <= 4, str(res))
hd = [v for f, v in huds(ev) if 600 <= f <= 1000]
check('th3_arena_carol_hud_up', hd and min(hd) <= -40, str(hd[:3]))
check('th3_carol_stage_end', near(sids(ev, 56)[-1:], [res + 367], 6) and any(lv != 'th3' and f > res for f, lv in loads), str((sids(ev, 56), loads)))

# Milla: D 15 (14050); every mask beaten -> K 0 -> A 3 (sound 363) -> A 4 150 frames later -> Spade -> the results
kills = ''.join('%d BOSSHPT 1571 0\n' % f for f in range(150, 700, 40))
rows, ev, d, loads = run('th3', 4, 1500, INV + ARENA + kills + '900 BOSSHPT 1222 0\n')
a, sp = d.get('tha', {}), d.get('sbp', {})
ach = changes(a, 'A'); f3 = next((f for f, v in ach if v == '3'), None); f4 = next((f for f, v in ach if v == '4'), None)
check('th3_arena_milla_goal15', a.get(60, {}).get('D') == '15' and a.get(60, {}).get('K') == '15', str(a.get(60)))
check('th3_arena_milla_cleared', f3 is not None and a[f3].get('K') == '0' and f4 == f3 + 150 and (f3 - 1) in sids(ev, 363), str((ach, sids(ev, 363))))
f0 = first(sp, lambda v: v.get('F') == '0')
check('th3_spade_milla_starts', f0 is not None and abs(f0 - (f4 + 90)) <= 2, str((f4, f0)))
res = next((i for i, r in enumerate(rows) if r.get('res', 0) > 0), None)
check('th3_milla_results', res is not None and res > 900, str(res))

# Carol, every ninja: 99 drops (one per 16 frames, at most 15 up); beating them all ends the wave (no forced state)
# (the player idles outside the arena: the beaten ninjas' crystals pile up and the 1024-object pool delays the last drops)
kills = ''.join('%d BOSSHPT 1571 0\n' % f for f in range(150, 2600, 30))
rows, ev, d, _ = run('th3', 2, 2700, INV + ARENA + kills)
a = d.get('tha', {})
f3 = first(a, lambda v: v.get('A') == '3')
drops = sorted({int(v['D']) for v in a.values()})
check('th3_arena_carol_99', f3 is not None and a[f3].get('K') == '0' and drops == list(range(0, 100)) and 1600 < f3 < 2600, str((f3, a.get(f3))))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
