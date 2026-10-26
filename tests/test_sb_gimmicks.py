#!/usr/bin/env python3
"""Host tests for the Sky Battalion gimmicks (session 19; src/game/sb_gim.c, build/re/spec_sb_gimmicks.md): the SB2-SB4 opening
hover platforms (held at I -160 while the planks fly, released at sb_flow_intro() 2, the player rides one in), the fire jets
(90-frame burn of a 180-frame cycle, 5 damage, the fire shield), the fans' lift column, the act-end cannons (C 30 / 50 volleys,
sound 99), the flamethrower's flames (sound 263), SB3's lightning (10 damage, sound 371), SB4's crusher / moving block, the
loop pieces and the data rows (types, sub ids).
usage: test_sb_gimmicks.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1', 'FP_TRACE_SID': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=300)
    os.unlink(name)
    rows, ev = [], []
    for line in (r.stdout + r.stderr).splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] in ('sid', 'sbflow'):
            ev.append((len(rows), line))
    return rows, ev, r.stdout + r.stderr

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sids(ev, n): return [i for i, l in ev if l.split()[:2] == ['sid', str(n)]]

def lvobjs(lv):
    """(type, anim, x, y) of a level file's objects (FPL2 header: magic, W, H, nrec, nobj, natlas, ntable, TW, TH, pad)"""
    d = open(os.path.join(DATA, lv, 'level.bin'), 'rb').read() if os.path.exists(os.path.join(DATA, lv, 'level.bin')) else None
    if d is None:
        for fn in os.listdir(os.path.join(DATA, lv)):
            b = open(os.path.join(DATA, lv, fn), 'rb').read()
            if b[:4] == b'FPL2': d = b; break
    if d is None: return []
    W, H, nrec, nobj = struct.unpack_from('<2I2H', d, 4)
    off = 4 + struct.calcsize('<2I4H2HI') + nrec * struct.calcsize('<2i4H4B')
    return [struct.unpack_from('<2H2i', d, off + 12 * k) for k in range(nobj)]

# ---- data: the importer's SB_GIM rows (types 1100-1149, sub | layer << 8)
for lv, want in (('sb2', {1117: 4, 1118: 4, 1119: 20, 1105: 28, 1108: 4, 1124: 9, 1100: 2, 1101: 2, 1103: 4}),
                 ('sb3', {1106: 10, 1107: 10, 1109: 4, 1127: 6, 1132: 2}),
                 ('sb4', {1112: 12, 1113: 12, 1114: 15, 1115: 15, 1116: 5, 1121: 10, 1122: 10, 1126: 59})):
    objs = lvobjs(lv); cnt = {}
    for t, a, x, y in objs: cnt[t] = cnt.get(t, 0) + 1
    check('data_%s_types' % lv, all(cnt.get(t, 0) == n for t, n in want.items()), str({t: cnt.get(t, 0) for t in want}))
fires = sorted(a & 0xff for t, a, x, y in lvobjs('sb2') if t == 1105)
check('data_sb2_fire_subs', set(fires) == {0, 1, 2, 3}, str(sorted(set(fires))))

# ---- the opening: SB1 pad -> SB2 (g4 -1); the player lands on a held platform (x -32) and rides it in after the release
rows, ev, out = run('5 TP 1704 1120\n', 700, {'FP_LEVEL': 'sb1'})
sb2 = [r for r in rows if r['act'] == 2]
rel = [i for i, l in ev if l.startswith('sbflow release')]
held = [r for r in sb2 if r['gnd'] == 1 and r['x'] == -32]
check('opening_rides_held_platform', len(held) > 100, 'grounded at x -32: %d frames' % len(held))
after = [r for r in sb2 if rel and r['f'] > rows[rel[0] - 1]['f'] + 60] if rel else []
check('opening_carried_in', after and all(r['gnd'] == 1 for r in after[:60]) and after[60]['x'] > 60 if len(after) > 60 else False,
      str(after[60]['x'] if len(after) > 60 else None))
check('opening_no_fall', all(r['y'] < 800 for r in sb2), 'max y %s' % max(r['y'] for r in sb2))

# ---- fire jet (SB2 3648, 791 up): burns while A > 90 of its 180-frame cycle: 5 damage, sid none; the fire shield is immune
rows, ev, out = run('5 TP 3648 775\n', 400, {'FP_LEVEL': 'sb2'})
hp = [r['hp'] for r in rows]
check('fire_jet_hurts', min(hp[10:]) < hp[5], 'hp %s -> %s' % (hp[5], min(hp[10:])))

# ---- fan (SB2 8704, 1032): the column above it (400 px) takes 0.2 a frame off the fall speed (9351)
rows, ev, out = run('5 TP 8704 900\n', 120, {'FP_LEVEL': 'sb2'})
air = [r['vy'] for r in rows[6:50] if r['gnd'] == 0]
check('fan_slows_fall', air and max(air) < 3, 'max vy %s' % (max(air) if air else None))

# ---- cannons (SB2 11568): volleys at C 30 / 50 (sound 99), the base's sequence after its defeat
rows, ev, out = run('5 TP 11300 1000\n', 600, {'FP_LEVEL': 'sb2'})
s99 = sids(ev, 99)
check('cannon_fires', len(s99) >= 2, 'sound 99 at %s' % s99[:6])

# ---- flamethrower (SB2 4656, 416): sound 263 in its window, the flames (relic.c OT_FLAME)
rows, ev, out = run('5 TP 4600 380\n', 400, {'FP_LEVEL': 'sb2'})
check('flamethrower_sound', len(sids(ev, 263)) >= 1, str(sids(ev, 263)[:4]))

# ---- SB3 lightning (4608, 752): the radial ray hurts (10, sound 371) unless the metal shield
rows, ev, out = run('5 TP 4608 740\n', 400, {'FP_LEVEL': 'sb3'})
check('lightning_hurts', len(sids(ev, 371)) >= 1 or min(r['hp'] for r in rows[10:]) < rows[5]['hp'], str(sids(ev, 371)[:3]))

# ---- SB4 crusher (one of x 5760..10192): rises and falls (sound 75 at the bottom)
rows, ev, out = run('5 TP 5760 700\n', 600, {'FP_LEVEL': 'sb4'})
check('crusher_slams', len(sids(ev, 75)) >= 1, str(sids(ev, 75)[:3]))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
