#!/usr/bin/env python3
"""Host tests for the rope / rail / grip hang targets (session 22 hang fix; src/game/objects.c obj_hang_box, fn_gimb.c ropes,
fn_gima.c clotheslines, obj_hang_try). Original traces: build/orig_traces/hang_fix/ (RV, not committed).
- Thermal Base rails (GimmickClothesline_1987 + HangingLongSensor2_1703, TB2-TB5, events 8128-8133 in f46-f49) had no collision
  box in the port (no rope sprite in the TB _ens banks): never grabbable. RV tb2_fall_l / _c / _m / _b: Lilac, Carol, Milla and
  Carol on the bike all hang (M 41, Y = rope y + 32) and shimmy 3 px a frame to the end sensor.
- Fortune Night ropes / clotheslines / trolley grips used their cropped sprite boxes (rope 72x6 at +5 instead of the 72x11 image;
  grip 42x10 instead of 50x15): grabs came late or were missed at the edges.
Checks: every rope span of TB2-TB5, FN4-FN5 (ropes) and FN1-FN2 (clotheslines) is grabbable along its whole length (no gaps);
grab / shimmy / end timing against the RV traces; release (jump, down) and the ground <-> hang transitions; the trolley grip margin.
usage: test_hang_fix.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
REF = 14.0                                        # host y = MV.Y + 14 (ORIG_REF_DY)
ST_HANG, ST_NORMAL = 11, 0

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=600)
    os.unlink(name)
    rows = []
    for line in r.stdout.splitlines():
        t = line.split()
        if t and t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
    return rows

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

def lvobjs(lv):
    d = None
    for fn in os.listdir(os.path.join(DATA, lv)):
        b = open(os.path.join(DATA, lv, fn), 'rb').read()
        if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

ROPE, CLOTHES, CLSTOP = 684, 583, 584             # GimmickClothesline_1987, GimmickClothesline2_1702, GimmickHangingLongSensor2_1703
def spans(lv, t):
    """rope pieces (72 x 11 boxes at x - 64 .. x + 8) merged into horizontal spans: (y, x0, x1)"""
    ps = sorted((y, x - 64, x + 8) for tt, a, x, y in lvobjs(lv) if tt == t and x >= 0 and y >= 0)
    out = []
    for y, a, b in ps:
        if out and out[-1][0] == y and a <= out[-1][2]: out[-1][2] = max(out[-1][2], b)
        else: out.append([y, a, b])
    return out
def grabs(lv, t, ch=0):
    """fall onto every span at x steps of 8 (from 2 px inside each end): each sample must hang at the rope's Y + 32"""
    jobs = []
    for y, a, b in spans(lv, t):
        xs = list(range(a + 2, b - 2, 8)) + [b - 3]
        jobs += [(y, x) for x in xs]
    def one(j):
        y, x = j
        r = run('1 SETV others E 99999\n2 TP %d %d\n' % (x, y - 4), 40, {'FP_LEVEL': lv, 'FP_CHAR': str(ch)})
        ok = r[-1]['st'] == ST_HANG and abs(r[-1]['y'] - REF - (y + 32)) < 0.01 and abs(r[-1]['x'] - x) < 20   # x: a post at a rope end may push her a few px
        return ok, j, (r[-1]['st'], r[-1]['x'], r[-1]['y'] - REF)
    with ThreadPoolExecutor(8) as ex: res = list(ex.map(one, jobs))
    bad = [(j, got) for ok, j, got in res if not ok]
    return len(jobs), bad

# ---- data: the TB rails and their end sensors are imported (tb_inventory.md: A2 11 / 8, A3 4 / 2, A4 8 / 6, A5 4 / 2)
for lv, n, m in (('tb2', 11, 8), ('tb3', 4, 2), ('tb4', 8, 6), ('tb5', 4, 2)):
    o = lvobjs(lv)
    c1 = sum(1 for t, a, x, y in o if t == ROPE and x >= 0); c2 = sum(1 for t, a, x, y in o if t == CLSTOP and x >= 0)
    check('data_%s_rails' % lv, (c1, c2) == (n, m), '%d ropes, %d ends' % (c1, c2))

# ---- every rail / rope / clothesline span is grabbable along its whole length (no invisible gaps)
for lv, t in (('tb2', ROPE), ('tb3', ROPE), ('tb4', ROPE), ('tb5', ROPE), ('fn4', ROPE), ('fn5', ROPE), ('fn1', CLOTHES), ('fn2', CLOTHES)):
    n, bad = grabs(lv, t)
    check('grab_all_%s' % lv, n > 0 and not bad, '%d samples, misses %s' % (n, bad[:4]))
n, bad = grabs('tb2', ROPE, 4)
check('grab_all_tb2_milla', n > 0 and not bad, '%d samples, misses %s' % (n, bad[:4]))

def first(rows, f0, cond):
    for k, r in enumerate(rows):
        if k >= f0 and cond(r): return k
    return None
def hang(r): return r['st'] == ST_HANG

# ---- RV tb2_fall_l / _c / _m / _b (TB2 rail at y 2936, ends 2824 / 3032): TP (2950, 2925), R from 40 to 80.
# RV: the grab comes on the frame after Y 2941.5, G 3 (bike: G 3.2998), i.e. at Y 2944.75 the TopBig rows 2926-2936 meet the
# rope's top row (the cropped sprite box started at 2941: 2 frames later); hang Y 2968; shimmy 3 px a frame (OTHERS.Y 42),
# stopped at X 3016 by the end sensor (OTHERS.Y 41)
for ch, nm, g0 in ((0, 'lilac', 3.0), (2, 'carol', 3.0), (4, 'milla', 3.0), (3, 'bike', 3.3)):
    r = run('5 TP 2950 2925\n40 80 R\n', 100, {'FP_LEVEL': 'tb2', 'FP_CHAR': str(ch)})
    g = first(r, 1, hang)
    check('tb2_rail_grab_%s' % nm, g is not None and r[g - 1]['y'] - REF == 2941.5 and abs(r[g - 1]['vy'] - g0) < 0.01 and r[g]['y'] - REF == 2968,
          'grab %s after y %.2f vy %.3f' % (g, r[g - 1]['y'] - REF, r[g - 1]['vy']) if g else 'no grab')
    xs = [r[k]['x'] for k in range(41, 80)]
    steps = {round(b - a, 3) for a, b in zip(xs, xs[1:]) if b != a}
    check('tb2_rail_shimmy_%s' % nm, steps == {3.0} and r[80]['x'] == 3016 and r[45]['oy'] == 42 and r[79]['oy'] == 41 and hang(r[99]),
          'steps %s end %.2f oy %d/%d' % (steps, r[80]['x'], r[45]['oy'], r[79]['oy']))

# ---- RV tb2_jump_l / fn4_jump_l: a jump from the floor under a rope catches it on the way down 29 frames after the take-off
# (TB2: floor 3072, rope 2936 -> Y 2968; FN4: floor 624, rope 484 -> Y 516), all three characters (no character test, RE 8128)
for lv, x, fy, ry in (('tb2', 2950, 3058, 2936), ('fn4', 8700, 610, 484)):
    for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
        r = run('5 TP %d %d\n40 60 J\n100 140 R\n' % (x, fy), 160, {'FP_LEVEL': lv, 'FP_CHAR': str(ch)})
        j = first(r, 30, lambda q: q['vy'] < 0); g = first(r, 30, hang)
        ok = j is not None and g is not None and r[g]['y'] - REF == ry + 32
        if ch == 0: ok = ok and g - j == 29                                     # RV (Lilac)
        check('%s_jump_grab_%s' % (lv, nm), ok, 'jump %s grab %s y %.2f' % (j, g, r[g]['y'] - REF if g else 0))

# ---- RV fn4_bar_l: the first trolley's grip (sensor at 1616, 753) from the platform at 864: caught 37 frames after the
# take-off at Y 785, then the ride (RV X 1645.33 / 1753.97 / 1946.78 at +20 / +40 / +60)
r = run('5 TP 1590 850\n40 60 J\n', 140, {'FP_LEVEL': 'fn4', 'FP_CHAR': '0'})
j = first(r, 30, lambda q: q['vy'] < 0); g = first(r, 30, hang)
check('fn4_grip_grab', g is not None and g - j == 37 and abs(r[g]['y'] - REF - 785) < 0.5, 'jump %s grab %s' % (j, g))
if g: check('fn4_grip_ride', all(abs(r[g + d]['x'] - rv) < 2 for d, rv in ((20, 1645.33), (40, 1753.97), (60, 1946.78))),
            ' '.join('%.2f' % r[g + d]['x'] for d in (20, 40, 60)))

# ---- the grip's whole 50 x 15 box (img 847, hotspot 25, 5): falling past it 28 px right of its centre (TopBig 23-33 px right) still grabs
# (the cropped sprite box ended 21 px right of the centre); RV fn4_barmargin_l: TP (1644, 700), grabbed at Y 785
r = run('5 TP 1644 700\n', 60, {'FP_LEVEL': 'fn4', 'FP_CHAR': '0'})
check('fn4_grip_margin', any(hang(q) for q in r))

# ---- release and transitions (RE 4041 / 4042; RV tb2_rel_l, tb2_drop_l): Down alone keeps the hang; Jump lets go upwards
# (G -4.5) and, straight under the rail, she catches it again 31 frames later; Jump with Down held drops (M 40, G 0) without a
# regrab and she lands on the floor (3072); a jump from the floor catches the rail again 29 frames after the take-off
for ch, nm in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
    r = run('2 SETV others E 99999\n5 TP 2950 2925\n30 33 D\n40 42 J\n90 100 D\n94 96 J\n140 160 J\n', 200, {'FP_LEVEL': 'tb2', 'FP_CHAR': str(ch)})
    u = first(r, 40, lambda q: not hang(q)); g2 = first(r, 41, hang)
    d = first(r, 90, lambda q: not hang(q)); l1 = first(r, 90, lambda q: q['gnd'] == 1)
    j = first(r, 140, lambda q: q['vy'] < 0); g3 = first(r, 140, hang)
    check('tb2_rail_down_keeps_%s' % nm, all(hang(r[k]) for k in range(20, 40)))
    check('tb2_rail_jump_off_%s' % nm, u is not None and r[u]['vy'] == -4.5 and g2 is not None and g2 - u == 31 and r[g2]['y'] - REF == 2968,
          'off %s grab %s' % (u, g2))
    check('tb2_rail_drop_%s' % nm, d is not None and r[d]['vy'] == 0 and l1 is not None and abs(r[l1]['y'] - 3072) < 1 and not any(hang(r[k]) for k in range(d, l1 + 1)),
          'drop %s land %s' % (d, l1))
    check('tb2_rail_regrab_%s' % nm, j is not None and g3 is not None and r[g3]['y'] - REF == 2968 and (ch != 0 or g3 - j == 29), 'jump %s grab %s' % (j, g3))

# ---- the FN1 clothesline (RV fn1_clothes_l): falling onto the line at (2800, 1104) hangs at Y 1136 and shimmies right
r = run('5 TP 2800 1090\n40 80 R\n', 100, {'FP_LEVEL': 'fn1', 'FP_CHAR': '0'})
g = first(r, 1, hang)
check('fn1_clothes_grab', g is not None and r[g]['y'] - REF == 1136 and r[70]['x'] > 2800 and hang(r[99]), 'grab %s x70 %.1f' % (g, r[70]['x']))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
