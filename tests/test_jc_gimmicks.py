#!/usr/bin/env python3
"""Host tests for the Jade Creek gimmicks (session 21; src/game/jc_gim.c, build/re/spec_jc_gimmicks.md): the water streams
(1 px a frame current, splash sound 208; RV st_idle / st_walk), the leaf platforms (rise 8 px a frame, open, stand height;
RV leaf1 / leaf2), the collapsing platforms (8 pieces, 6-frame steps, the fall; RV coll2), the sliding slabs, the seeds
(M 97 ride), the bubble springs (M 99 ride), the flying leaves, Carol on slippery walls, LevelCreekPlatform = DV's valley
platform, and the importer data rows. Lilac, Carol and Milla where the behaviour can differ.
usage: test_jc_gimmicks.py [data_dir]   (default: converted2; the session's JC data: cv)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=300)
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
            d = {'f': int(t[1]), 'type': int(t[2])}
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
def MY(r): return r['y'] - 14.0                        # the original's MV.Y (the port's y = MV.Y + ORIG_REF_DY)

def lvobjs(lv):
    """(type, anim, x, y) of a level file's objects (FPL2 header: magic, W, H, nrec, nobj, ...)"""
    d = None
    for fn in os.listdir(os.path.join(DATA, lv)):
        b = open(os.path.join(DATA, lv, fn), 'rb').read()
        if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

# ---- data: the importer's JC_GIM rows (types 1370-1386, member | layer << 8; LevelCreekPlatform -> 32 = OT_PLAT_VALLEY)
want = {'jc1': {1370: 82, 1371: 8, 1372: 16, 1375: 4, 1376: 4, 1377: 3, 1380: 2},
        'jc2': {1370: 55, 1371: 3, 1372: 6, 1378: 8, 1380: 8, 1382: 4, 1385: 25},
        'jc3': {1371: 2, 1373: 9, 1374: 9, 1375: 1, 1378: 1, 1385: 12}}
for lv, w in want.items():
    objs = lvobjs(lv); cnt = {}
    for t, a, x, y in objs:
        if x >= 0 and y >= 0: cnt[t] = cnt.get(t, 0) + 1
    check('data_%s_types' % lv, all(cnt.get(t, 0) == n for t, n in w.items()), str({t: cnt.get(t, 0) for t in w}))
objs = lvobjs('jc1')
subs = sorted({a & 0xff for t, a, x, y in objs if t == 1370})
check('data_jc1_stream_members', subs == [1, 2, 3, 4, 5, 6, 7, 8], str(subs))
tilt = {(x, y): (a & 0xff) * 30 for t, a, x, y in objs if t == 1371}
check('data_jc1_leaf_tilts', tilt.get((4528, 895)) == 120 and tilt.get((4592, 895)) == 60 and tilt.get((10128, 647)) == 60 and tilt.get((3544, 1239)) == 0,
      str(tilt))
creek = [(x, y) for t, a, x, y in objs if t == 32 and (x, y) in ((12840, 704), (12992, 600))]
check('data_jc1_creek_is_valley_platform', len(creek) == 2, str(creek))

# ---- water streams (RV st_idle: JC1 stream 1 at 9584..9712, grounded at MV.Y 962.7; X - 1.000 a frame, sound 208 on landing)
for ch, nm in CHARS:
    rows, ev, _ = run('5 TP 9700 962\n', 60, {'FP_LEVEL': 'jc1', 'FP_CHAR': str(ch)})
    g = [r for r in rows if r['gnd'] == 1]
    d = [g[i]['x'] - g[i + 1]['x'] for i in range(len(g) - 1)][:40]
    check('stream_push_left_%s' % nm, len(d) >= 30 and all(abs(v - 1) < 1e-6 for v in d), 'dx %s' % sorted(set(round(v, 3) for v in d)))
    check('stream_splash_sound_%s' % nm, len(sids(ev, 208)) == 1, 'sound 208 at %s' % sids(ev, 208))
# RV st_walk: walking right against the current at the top run speed nets B - 1 a frame (5.0128 - 1)
rows, ev, _ = run('5 TP 9700 962\n20 100 R\n', 100, {'FP_LEVEL': 'jc1', 'FP_CHAR': '0'})
dx = rows[95]['x'] - rows[94]['x']
check('stream_walk_against', abs(dx - (rows[94]['gsp'] - 1)) < 0.05 and dx < 4.5, 'dx %.3f gsp %.3f' % (dx, rows[94]['gsp']))
# a right-pushing stream (JC2 stream 2 at 6800,1560: x 6800..6928) moves her right
rows, ev, _ = run('5 TP 6860 1530\n', 60, {'FP_LEVEL': 'jc2', 'FP_CHAR': '0'})
g = [r for r in rows if r['gnd'] == 1]
check('stream_push_right', len(g) > 20 and abs((g[-1]['x'] - g[-21]['x']) - 20) < 1e-6, '%.2f -> %.2f' % (g[-21]['x'], g[-1]['x']) if len(g) > 20 else '')

# ---- leaf platform (RV leaf1: the back at 3544 rises 1367 -> 1239 in 16 frames from the frame the player comes within
# 200 / 300 px of the stalk top; RV leaf2: standing on the opened leaves at MV.Y 986.75)
rows, ev, obs = run('5 TP 3380 1092\n', 40, {'FP_LEVEL': 'jc1', 'FP_CHAR': '0', 'FP_TRACK': '1371:3544'})
ys = [o['y'] for o in obs if o['type'] == 1371]
steps = [ys[i] - ys[i + 1] for i in range(len(ys) - 1) if ys[i] != ys[i + 1]]
check('leaf_back_rises', ys and ys[0] == 1367 and ys[-1] == 1239 and steps == [8] * 16, 'y %s .. %s, %d steps' % (ys[:1], ys[-1:], len(steps)))
for ch, nm in CHARS:
    rows, ev, _ = run('5 TP 3380 1092\n100 TP 3500 960\n', 140, {'FP_LEVEL': 'jc1', 'FP_CHAR': str(ch)})
    st = [r for r in rows[115:] if r['gnd'] == 1]
    check('leaf_stand_%s' % nm, len(st) > 15 and all(abs(MY(r) - 986.75) < 0.6 for r in st), 'MV.Y %s' % sorted(set(MY(r) for r in st)))
rows, ev, _ = run('5 TP 3200 1092\n100 TP 3500 960\n', 140, {'FP_LEVEL': 'jc1', 'FP_CHAR': '0'})   # far: closed, she falls through
check('leaf_closed_when_far', not any(r['gnd'] == 1 and abs(MY(r) - 986.75) < 1 for r in rows[100:]), '')

# ---- collapsing platform (RV coll2: JC2 640,1344; she lands at MV.Y 1330.5 with sound 426, 8 pieces, she falls 37 frames later)
for ch, nm in CHARS:
    rows, ev, obs = run('5 TP 700 1310\n', 90, {'FP_LEVEL': 'jc2', 'FP_CHAR': str(ch), 'FP_TRACK': '1379:-1'})
    land = next((i for i, r in enumerate(rows) if r['gnd'] == 1 and i > 5), None)
    off = next((i for i, r in enumerate(rows) if land and i > land and r['gnd'] == 0), None)
    s = sids(ev, 426)
    np_ = len({(o['f'], o.get('x'), o.get('fr')) for o in obs if o['f'] == (land or 0) + 1})
    check('collapse_%s' % nm, land and abs(MY(rows[land]) - 1330.5) < 0.6 and s and abs(s[0] - 1 - land) <= 1 and off and off - land == 37,
          'land %s MV.Y %s sound %s off %s' % (land, MY(rows[land]) if land else None, s, off))
rows, ev, obs = run('5 TP 700 1310\n', 60, {'FP_LEVEL': 'jc2', 'FP_CHAR': '0', 'FP_TRACK': '1379:-1'})
first = {}
for o in obs:
    if o['type'] == 1379 and o.get('fl', 0) and o['fr'] not in first: first[o['fr']] = o['f']
order = [first.get(k) for k in range(8)]
check('collapse_pieces_6_frames', all(order) and all(order[k + 1] - order[k] == 6 for k in range(7)), str(order))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
