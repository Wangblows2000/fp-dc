#!/usr/bin/env python3
"""Host tests for the Thermal Base gimmicks (session 21c; src/game/tb_gim.c, build/re/spec_tb_gimmicks.md): the importer's TB_GIM
rows; key cards, terminals and locked doors (TB1 gate, all three characters; sounds 37 / 285 / 286 / 279); switch buttons and
explosive blocks (TB2 wall, TB4's left switch wall vs RV btn4l, all three characters; Carol's cling switches); fire platforms (TB1 G 80 cycle, RV fire1; TB2 phase, RV init2; carry; the
flames' burn); pistons and spin platforms (RV init2 positions); scrolling platforms (carry, the cycle, sound 283); conveyors;
floor traps (RV ftrap4: 36 frames, 4 half-petals); lava falls (240-frame cycle, burn); crank platforms (RV crank4 linkage, carry);
metal doors; the TB5 water levers (level_water_set).
usage: test_tb_gimmicks.py [data_dir]   (default: converted2; the session's TB data: cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    rows, ev, obs = [], [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'sid':
            ev.append((len(rows), int(t[1])))
        elif t[0] == 'ob':
            d = {'f': int(t[1]), 'type': int(t[2]), 'sx': int(t[3])}
            for i in range(4, len(t) - 1, 2):
                try: d[t[i]] = int(t[i + 1], 16) if t[i] == 'fl' else float(t[i + 1])
                except ValueError: pass
            obs.append(d)
    return rows, ev, obs

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sids(ev, n): return [i for i, s in ev if s == n]
def MY(r): return r['y'] - 14.0
def at(obs, f, t): return [o for o in obs if o['f'] == f and o['type'] == t]

def lvobjs(lv):
    d = None
    for fn in os.listdir(os.path.join(DATA, lv)):
        b = open(os.path.join(DATA, lv, fn), 'rb').read()
        if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

# ---- data: TB_GIM types per act (placements on the map; tb_inventory.md)
want = {'tb1': {1890: 2, 1891: 2, 1892: 6, 1893: 2, 1894: 14, 1934: 6, 1897: 12, 1898: 12, 1899: 12, 1900: 12, 1901: 2, 1902: 3, 1908: 5, 1912: 7},
        'tb2': {1896: 1, 1903: 6, 1904: 6, 1905: 12, 1906: 12, 1907: 12, 1913: 31, 1914: 4, 1909: 19, 1910: 44, 1911: 13},
        'tb3': {1915: 2, 1916: 8, 1910: 52},
        'tb4': {1917: 34, 1922: 4, 1923: 12, 1918: 5, 1920: 5, 1919: 7, 1927: 13},
        'tb5': {1924: 32, 1925: 8, 1926: 21, 1929: 50, 1930: 27, 1931: 2, 1932: 2},
        'tb6': {1909: 27}}
for lv, w in want.items():
    cnt = {}
    for t, a, x, y in lvobjs(lv):
        if x >= 0 and y >= 0: cnt[t] = cnt.get(t, 0) + 1
    check('data_%s_types' % lv, all(cnt.get(t, 0) == n for t, n in w.items()), str({t: (cnt.get(t, 0), n) for t, n in w.items() if cnt.get(t, 0) != n}))
check('data_no_lift_types_here', not any(t in (1888, 1889, 1895) for lv in want for t, a, x, y in lvobjs(lv)), 'lifts / wall lights are tb_flow.c types')
o1 = lvobjs('tb1')
check('data_tb1_door_members', sorted({a & 0xff for t, a, x, y in o1 if t == 1892}) == [0, 1, 2])

# ---- key card, terminal, locked door (TB1: card 6976,1912 on the ledge, terminal 7088,2040, barrier 7152,1944)
for ch, nm in CHARS:
    rows, ev, obs = run('5 TP 6988 2020\n30 45 J\n80 TP 7080 2026\n150 330 R\n', 330,
                        {'FP_LEVEL': 'tb1', 'FP_CHAR': str(ch), 'FP_TRACK': '1893:7152,1891:7088'})
    s37, s285, s286, s279 = sids(ev, 37), sids(ev, 285), sids(ev, 286), sids(ev, 279)
    check('key_pickup_sounds_%s' % nm, s37 and s285 and s37[0] < s285[0] < 80, '37 %s 285 %s' % (s37[:1], s285[:1]))
    t = [o for o in obs if o['type'] == 1891]
    opened = [o['f'] for o in t if o['a0'] == 2]
    check('terminal_opens_%s' % nm, s286 and s279 and opened and opened[0] - s286[0] in (30, 31, 32), 'insert %s open %s A2 %s' % (s286[:1], s279[:1], opened[:1]))
    b = [o for o in obs if o['type'] == 1893]
    check('barrier_off_after_open_%s' % nm, b and not (b[0]['fl'] & 1) and (b[-1]['fl'] & 1))
    check('walks_through_door_%s' % nm, rows[-1]['x'] > 7300, 'x %.0f' % rows[-1]['x'])
# without a card the barrier holds
rows, ev, obs = run('5 TP 7080 2026\n20 200 R\n', 200, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0'})
check('door_blocks_without_card', rows[-1]['x'] < 7160 and not sids(ev, 286), 'x %.0f' % rows[-1]['x'])

# ---- switch button + explosive blocks (TB2: SwitchButtonLeft 2408,2464; the wall 2248-2376 behind it)
for ch, nm in CHARS:
    rows, ev, obs = run('5 TP 2450 2470\n20 120 L\n', 160, {'FP_LEVEL': 'tb2', 'FP_CHAR': str(ch), 'FP_TRACK': '1913:2248,1913:2312'})
    gone = [o for o in obs if o['f'] == 150]
    check('button_press_%s' % nm, sids(ev, 49) and sids(ev, 38), 'sounds 49 %s 38 %s' % (sids(ev, 49)[:1], sids(ev, 38)[:1]))
    check('blocks_destroyed_%s' % nm, not gone and [o for o in obs if o['f'] == 30], '%d left at 150' % len(gone))
rows, ev, obs = run('5 TP 2450 2470\n', 120, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0', 'FP_TRACK': '1913:2248'})
check('blocks_stay_without_press', [o for o in obs if o['f'] == 110] and not sids(ev, 38))
# TB4's left switch (2528,1416) sits a row below its 3 x 2 wall (2304-2496, rows 1256 / 1320): RE 11958's three destroyers
# (the switch, + 64 / - 64, + 128 / - 128), 2 px a frame through the wall, each hit resetting every touching destroyer's B (RV btn4l:
# the press at 90, the six blocks lit at + 7 / 13 / 37 / 39 / 69 / 71, column by column, gone 30 frames later)
RV_LIT = [7, 13, 37, 39, 69, 71]
for ch, nm in CHARS:
    rows, ev, obs = run('3 SETV others E 99999\n60 TP 2570 1420\n80 180 L\n', 300, {'FP_LEVEL': 'tb4', 'FP_CHAR': str(ch), 'FP_TRACK': '1913:-1'})
    s49 = sids(ev, 49)
    lit = {}
    for o in obs:
        if o['type'] == 1913 and o['fl'] & 0x1000 and (o['x'], o['y']) not in lit: lit[(o['x'], o['y'])] = o['f']
    wall = [(x, y) for x in (2304, 2368, 2432) for y in (1256, 1320)]
    left = [(o['x'], o['y']) for o in at(obs, 299, 1913) if (o['x'], o['y']) in wall]
    d = sorted(f - s49[0] for k, f in lit.items() if k in wall) if s49 else []
    check('tb4_switch_wall_%s' % nm, s49 and not left and len(d) == 6 and all(abs(a - b) <= 1 for a, b in zip(d, RV_LIT)),
          'press %s lit %s (RV %s) left %s' % (s49[:1], d, RV_LIT, left))
    xs = [r['x'] for r in run('3 SETV others E 99999\n60 TP 2570 1420\n80 180 L\n200 TP 2450 1370\n220 330 L\n', 330, {'FP_LEVEL': 'tb4', 'FP_CHAR': str(ch)})[0]]
    check('tb4_switch_wall_passable_%s' % nm, xs and xs[-1] < 2290, 'x %.0f' % (xs[-1] if xs else -1))
# the mid-air switches (TB3 6912,2848 / 7056,2976, TB4 7096,544) are pressed by Carol's wall cling (M 47): each blows the rows of
# its three destroyers (TB3's right switch the upper three of the five, TB4's the whole column)
for lv, bx, by, keys, want in (('tb4', 7084, 540, 'R', [(7144, 384), (7144, 448), (7144, 512)]),
                               ('tb3', 6900, 2844, 'R', [(6960, 2688), (6960, 2752), (6960, 2816)])):
    rows, ev, obs = run('3 SETV others E 99999\n10 TP %d %d\n10 120 %s\n' % (bx, by, keys), 200, {'FP_LEVEL': lv, 'FP_CHAR': '2', 'FP_TRACK': '1913:-1'})
    left = [(o['x'], o['y']) for o in at(obs, 199, 1913) if (o['x'], o['y']) in want]
    check('%s_cling_switch_carol' % lv, sids(ev, 49) and not left, 'press %s left %s' % (sids(ev, 49)[:1], left))

# ---- fire platforms
rows, ev, obs = run('', 200, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0', 'FP_TRACK': '1934:3008,1897:2956'})
ys = [o['y'] for o in obs if o['type'] == 1934]
check('fireplat_tb1_amplitude_RV', min(ys) == 1712 and max(ys) in (1871, 1872), 'y %d..%d (RV fire1 1712..1871)' % (min(ys), max(ys)))
f0 = at(obs, 0, 1934)
ff = [o for o in obs if o['type'] == 1897]
check('fireplat_flame_follows', all(abs(o['x'] - 2956) < 1 for o in ff) and ff and {round(o['y'] - p['y']) for o in ff for p in at(obs, o['f'], 1934)} <= {31, -1}, '')
rows, ev, obs = run('', 3, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0', 'FP_TRACK': '1896:-1,1903:-1,1905:-1'})
f1 = {(int(o['x']), int(o['y'])) for o in at(obs, 1, 1896) + at(obs, 1, 1903) + at(obs, 1, 1905)}
RV1 = {(6304, 2481), (2016, 3201), (2528, 3326), (2720, 3137), (5952, 3454), (4052, 3176), (4103, 3396), (3883, 3447), (3832, 3227), (7004, 2664), (6784, 2715)}
check('tb2_platforms_frame1_RV_init2', RV1 <= f1, str(sorted(RV1 - f1)))
rows, ev, obs = run('5 TP 3008 1640\n', 250, {'FP_LEVEL': 'tb1', 'FP_CHAR': '2'})
check('fireplat_carry', all(r['gnd'] == 1 for r in rows[60:250]) and max(r['y'] for r in rows[60:]) - min(r['y'] for r in rows[60:]) > 120,
      'y %.0f..%.0f' % (min(r['y'] for r in rows[60:]), max(r['y'] for r in rows[60:])))

# ---- scrolling platforms (TB1): a pillar carries the player left 3 px a frame, sinks at the end marker, sound 283 as it comes back
rows, ev, obs = run('5 TP 8262 1500\n', 200, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0'})
d = [rows[f]['x'] - rows[f + 1]['x'] for f in range(35, 55)]
check('scroller_carry_3px', all(abs(v - 3) < 1e-6 for v in d), str(d[:5]))
check('scroller_sound_283', sids(ev, 283))

# ---- conveyors (TB4 right belt at 2336,2856: +2 a frame)
rows, ev, obs = run('5 TP 2380 2820\n', 80, {'FP_LEVEL': 'tb4', 'FP_CHAR': '0'})
d = [rows[f + 1]['x'] - rows[f]['x'] for f in range(50, 70)]
check('conveyor_push_2', all(abs(v - 2) < 1e-6 for v in d), str(d[:5]))

# ---- floor trap (TB4 6784,3040; RV ftrap4: 36 frames after the step, 4 half-petals, gone)
rows, ev, obs = run('60 TP 6810 3000\n', 160, {'FP_LEVEL': 'tb4', 'FP_CHAR': '0', 'FP_TRACK': '1915:6784'})
t = [o['f'] for o in obs]
s173 = sids(ev, 173)
hurt = [int(r['f']) for i, r in enumerate(rows[1:]) if r['hp'] < rows[i]['hp']]
check('floortrap_burst_36', s173 and t and t[-1] - s173[0] in (34, 35, 36), 'step %s gone after %s' % (s173[:1], t[-1:]))
check('floortrap_hurt_4', hurt and rows[hurt[0] - 1]['hp'] - rows[hurt[0] + 1]['hp'] == 4, 'hurt %s' % hurt[:2])

# ---- lava falls (TB2 4832..: c = frame mod 240; fall at c 150, warning sound 302 before)
rows, ev, obs = run('5 SETV others E 0\n', 260, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0', 'FP_TRACK': '1910:-1'})
on = sorted({o['f'] for o in obs if o['hd'] == 0 and o['type'] == 1910})
check('lavafall_starts_150', on and on[0] in (149, 150, 151), str(on[:3]))
rows, ev, obs = run('4 KILLT 1824\n5 TP 4900 3300\n', 220, {'FP_LEVEL': 'tb2', 'FP_CHAR': '0'})
hurt = [int(r['f']) for r in rows if r['mm'] == 8]
check('lavafall_burns', hurt and 160 <= hurt[0] < 180, 'M8 from %s (RV lava2: Carol 172)' % hurt[:1])

# ---- crank platforms (TB4: top1 at its RV crank4 linkage; the player rides)
rows, ev, obs = run('', 200, {'FP_LEVEL': 'tb4', 'FP_CHAR': '0', 'FP_TRACK': '1922:1000,1922:496'})
p1 = {(int(o['x']), int(o['y'])) for o in obs if o['sx'] == 1000}
p2 = {(int(o['x']), int(o['y'])) for o in obs if o['sx'] == 496}
RVC1, RVC2 = {(720, 2395), (862, 2287), (900, 2429), (828, 2467)}, {(508, 2353), (470, 2493), (540, 2531), (648, 2461)}   # RV crank4 (first run)
check('crank_tops_RV_linkage', RVC1 <= p1 and RVC2 <= p2, str(sorted(RVC1 - p1)) + str(sorted(RVC2 - p2)))
rows, ev, obs = run('5 TP 828 2440\n', 200, {'FP_LEVEL': 'tb4', 'FP_CHAR': '0'})
check('crank_carry', sum(r['gnd'] for r in rows[40:200]) > 150 and max(r['x'] for r in rows[40:]) - min(r['x'] for r in rows[40:]) > 60,
      'x %.0f..%.0f' % (min(r['x'] for r in rows[40:]), max(r['x'] for r in rows[40:])))

# ---- metal doors open near the player (TB1 3448,1896)
rows, ev, obs = run('5 TP 3440 1880\n', 60, {'FP_LEVEL': 'tb1', 'FP_CHAR': '0', 'FP_TRACK': '1894:3448'})
check('metaldoor_opens', at(obs, 50, 1894) and all(o['a0'] == 10 for o in at(obs, 50, 1894)) and sids(ev, 279))

# ---- TB5 water lever 1 (mask 3297,2800): hang on it, the water line (ML.B 2920) runs + 1 a frame toward 3240 (level_water_set)
for ch, nm in CHARS:
    e = dict(os.environ); e.update({'FP_LEVEL': 'tb5', 'FP_CHAR': str(ch), 'FP_DBG_TBG': '1'})
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f: f.write('5 TP 3297 2830\n20 30 J\n'); name = f.name
    out = subprocess.run([BIN, DATA, '250', name], capture_output=True, text=True, env=e, timeout=600).stdout; os.unlink(name)
    w = [int(l.split()[5]) for l in out.splitlines() if l.startswith('tbg ')]
    check('water_lever_%s' % nm, w and w[0] == 2920 and w[-1] > 3050 and all(b - a in (0, 1) for a, b in zip(w, w[1:])), 'B %s .. %s' % (w[:1], w[-1:]))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
