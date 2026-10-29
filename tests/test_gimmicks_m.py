#!/usr/bin/env python3
"""Host tests for the session-26 gimmick gaps (build/gimmick_audit.md items 2, 4 and 12; RE + RV in the comments of the code):
 2. Carol's jump pads in TB4, TH3, BG2, JC2-JC4, FN4 and FN5 (levels.c PADS_*: the act's start configuration, RV pad list dump):
    every on-map pad has a table row whose index A is the spread index (newest first = reverse frame-JSON order) and is drawn
    (TB4 / BG2: ObjectJumpPad_539's objs rows; TH3 1284 / JC / FN 1529: their own jpad_rm rows); Carol rides each act's first lit
    pad (floor, wall and ceiling pads; RV c_jumppad_tb4 / c_jumppad_bg2: TB4 floor -> ceiling -> launch down with 61 / 33 / 113,
    BG2 wall chain launched right); JC3's single pad is lit at 90 degrees but inert (no 4943-4949 in its frame); Lilac and Milla
    on a lit floor pad: no ride, the pad goes dark and two crystals pop up (PinballMovement 36 / 25) and are taken 33 / 40 frames
    after the jump, when they land (RV lpop_l / lpop_m: 114 / 121 after the jump at 81).
 4. fd2b's vacuum air rule (water.c air_vacuum_events): from the alarm (siren A 2, sound 372) OTHERS.J counts a second at a time
    (RV fd2b_air: +1 every 60 frames from 24 frames after 372), dings 18 at 5 / 10 / 15, warnings 41 / 42 from 20, death at 32
    (RV 1884 frames after 372) with the act restart; the GUIAir counter slides in (16 frames); never for Milla (no alarm). The 6
    oxygen patches exist on Easy / Casual only (fd2b's start destroys them while global 12 < 2): touching one opens it (sound 20)
    and gives the air back (J -> 18 at once from the countdown, then -1 a frame to 0).
 12. JC5's storm rain (jc_boss5.c rain_frame, RE 11611-11618): one drop a frame up to min(50, (MV.X - 200 + GT) / 120) (RV rain_tp:
    50 a frame after a TP to x 9000; rain_l: none while standing at the start), splashes on the water line, the rain loop 293
    (volume 2 n - 2), the layer-0 tint (R = G = 255 - (MV.X - 400 + GT) / 25), the bolt only once the rain has more than 40 drops.
usage: test_gimmicks_m.py [data_dir]   (default: converted2)"""
import os, re, subprocess, sys, tempfile, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')

def run(level, char, frames, script, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_DIFF')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char), 'FP_TRACE_SID': '1', 'FP_TRACE_SND': '1'}); e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e, timeout=900)
    os.unlink(name)
    rows, sids, other = {}, [], []
    fr = 0
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: t[i + 1] for i in range(0, len(t) - 1, 2)}
        elif t[0] in ('sid', 'voice'): sids.append((fr + 1, t[1]))
        else: other.append((fr + 1, t))
    return rows, sids, other

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def sid(sids, n): return [f for f, i in sids if i == str(n)]
def F(r, k): return float(r[k])

# =========================================================================================== item 2: jump pads
JSON = {'tb4': 48, 'th3': 59, 'bg2': 61, 'jc2': 41, 'jc3': 42, 'jc4': 43, 'fn4': 33, 'fn5': 34}
src = open(os.path.join(ROOT, 'src', 'game', 'levels.c')).read()
def pad_table(lv):
    m = re.search(r'static const padcfg_t PADS_%s\[\] = \{(.*?)\};' % lv.upper(), src, re.S)
    return [tuple(float(v) for v in g.split(',')) for g in re.findall(r'\{([-0-9., ]+)\}', m.group(1))] if m else []
for lv, js in JSON.items():
    objs = json.load(open(os.path.join(ROOT, 'tables', 'frames', 'frame_%d.json' % js)))['objects']
    pads = [(o['x'], o['y']) for o in objs if o['cls'] == 'objectjumppad']
    want = {(x, y): len(pads) - 1 - k for k, (x, y) in enumerate(pads) if x >= 0 and y >= 0}   # 3564: the spread, newest first
    tab = {(int(r[0]), int(r[1])): int(r[2]) for r in pad_table(lv)}
    check('pads_table_%s' % lv, want and tab == want, '%d pads, table %d' % (len(want), len(tab)))
for lv in JSON:   # every pad bound to a sprite (the rows exported for its handle) and at its table angle
    rows, s, o = run(lv, 2, 3, '', {'FP_TRACK': '132:-1'})
    obs = [t for f, t in o if t[0] == 'ob' and t[1] == '2']
    on = [t for t in obs if float(t[t.index('x') + 1]) >= 0 and float(t[t.index('y') + 1]) >= 0]
    drawn = [t for t in on if int(t[t.index('an') + 1]) >= 0 and t[t.index('hd') + 1] == '0']
    check('pads_drawn_%s' % lv, on and len(drawn) == len(on), '%d of %d' % (len(drawn), len(on)))
    if lv in ('th3', 'jc2', 'jc3', 'jc4', 'fn4', 'fn5'):   # their own images: the _ens bank's jpad_rm rows
        rep = open(os.path.join(DATA, lv, lv + '_report.txt')).read()
        check('pads_rows_%s' % lv, "'jpad_rm'" in rep and "'jpad_rm_lit'" in rep)

# Carol on each act's first lit pad (floor pads: press jump standing on it; wall pads: against the wall in the air)
RIDES = {'tb4': '60 TP 4192 2758;80 84 J;110 114 J;135 139 J', 'bg2': '60 TP 8330 870;60 90 L;72 75 J;100 103 J;125 128 J',
         'th3': '60 TP 470 1330;60 90 R;72 75 J', 'jc2': '60 TP 7014 994;60 90 R;72 75 J', 'jc4': '60 TP 6136 1158;80 84 J',
         'fn4': '60 TP 7200 462;80 84 J', 'fn5': '60 TP 4144 830;80 84 J'}
TARGET = {'tb4': (4520, 2664), 'bg2': (8304, 576), 'th3': (496, 952), 'jc2': (6768, 528), 'jc4': (6648, 1192), 'fn4': (7344, 208), 'fn5': (4624, 864)}
for lv, scr in RIDES.items():
    rows, s, o = run(lv, 2, 200, scr)
    ride = [f for f in sorted(rows) if rows[f]['st'] == '20']
    tx, ty = TARGET[lv]
    at = [f for f in ride if abs(F(rows[f], 'x') - tx) < 40 and abs(F(rows[f], 'y') - ty) < 40]   # reached the next pad
    check('carol_pad_ride_%s' % lv, len(ride) > 15 and at and sid(s, 61) and sid(s, 33), '%d frames riding, at next pad %s' % (len(ride), at[:1]))
rows, s, o = run('tb4', 2, 200, RIDES['tb4'])     # RV c_jumppad_tb4: floor -> ceiling -> ceiling (D 1) -> launched straight down
launch = [f for f in sorted(rows) if f > 130 and rows[f]['st'] == '0']
check('carol_pad_launch_tb4', launch and F(rows[launch[0] + 3], 'vy') > 7 and abs(F(rows[launch[0] + 3], 'x') - 4896) < 2 and
      sid(s, 113) == [81, 135], 'vy %s x %s 113 at %s' % (rows[launch[0] + 3]['vy'], rows[launch[0] + 3]['x'], sid(s, 113)))
rows, s, o = run('bg2', 2, 200, RIDES['bg2'])     # RV c_jumppad_bg2: wall pads 0 -> 1 -> 2 (D 1, B 270): launched right (B = +8.5)
launch = [f for f in sorted(rows) if f > 110 and rows[f]['st'] == '0']
check('carol_pad_launch_bg2', launch and F(rows[launch[0] + 2], 'vx') > 7 and abs(F(rows[launch[0]], 'y') - 254) < 3,
      'vx %s y %s' % (rows[launch[0] + 2]['vx'], rows[launch[0]]['y']) if launch else 'no launch')
rows, s, o = run('jc3', 2, 120, '60 TP 8574 2090;60 90 R;72 75 J', {'FP_TRACK': '132:-1'})
pad = [t for f, t in o if t[0] == 'ob' and f == 100]
check('jc3_pad_inert', not any(r['st'] == '20' for r in rows.values()) and pad and pad[0][pad[0].index('ang') + 1] == '90.00' and
      int(pad[0][pad[0].index('an') + 1]) >= 0, str(pad[0][4:20]) if pad else 'no pad')
for ch, nm in ((0, 'lilac'), (4, 'milla')):      # 4944 / 3275 / 4945: no ride, the pad dark, two crystals taken when they land
    rows, s, o = run('tb4', ch, 135, '60 TP 4192 2758;80 84 J', {'FP_TRACK': '132:4192'})
    jump = sid(s, 8)[:1]; cr = sid(s, 27)
    dark = [t for f, t in o if t[0] == 'ob' and f == 100]
    check('pad_crystals_%s' % nm, not any(r['st'] == '20' for r in rows.values()) and jump and [c - jump[0] for c in cr] == [33, 40] and
          dark and dark[0][dark[0].index('a0') + 1] == '0.000', 'jump %s crystals %s' % (jump, cr))
rows, s, o = run('tb4', 0, 220, '60 TP 4192 2758;80 84 J;84 140 R', {'FP_TRACK': '0:-1'})   # left behind: they land and stay (RV lpop_run)
check('pad_crystals_rest', not sid(s, 27), str(sid(s, 27)))

# =========================================================================================== item 4: fd2b's vacuum
INV = ''.join('%d SETV others E 99999;' % f for f in range(3, 2600, 300))
def vac(char, frames, script, env=None):
    rows, s, o = run('fd2b', char, frames, script, dict(env or {}, FP_DBG_AIR='1'))
    v = {int(t[1]): {t[i]: int(t[i + 1]) for i in range(2, len(t) - 1, 2)} for f, t in o if t[0] == 'vac'}
    return rows, s, v
for ch, nm in ((0, 'lilac'), (2, 'carol')):
    rows, s, v = vac(ch, 2320, INV + '3 TP 6240 5600')
    a = sid(s, 372)[:1]
    up = [f for f in sorted(rows) if rows[f]['air'] == '1'][:1]
    dead = [f for f in sorted(rows) if rows[f]['st'] == '6'][:1]
    check('vacuum_count_%s' % nm, a and up and up[0] - a[0] == 23 and dead and dead[0] - a[0] == 1883,
          '372 %s J 1 %s death %s' % (a, up, dead))
    ds = sid(s, 91 if ch == 0 else 128)   # loop_sound_die_8 (RV fd2b_air: Lilac 91; fd2b_air_c: Carol 128), the music unqueued
    check('vacuum_death_sound_%s' % nm, dead and ds and abs(ds[0] - dead[0]) <= 1, '%s at %s' % (91 if ch == 0 else 128, ds))
    check('vacuum_sounds_%s' % nm, len(sid(s, 18)) == 3 and len(sid(s, 41)) + len(sid(s, 42)) == 11, '18 %s 41 %s 42 %s' % (sid(s, 18), sid(s, 41), sid(s, 42)))
    ic = [v[f]['icon'] for f in sorted(v) if a and a[0] <= f <= a[0] + 20]
    check('vacuum_hud_%s' % nm, ic and max(ic) == 16 and ic.count(16) <= 5, str(ic))
rows, s, v = vac(4, 600, INV + '3 TP 6240 5600')
check('vacuum_never_milla', not sid(s, 372) and all(r['air'] == '0' for r in rows.values()))
rows, s, v = vac(0, 30, INV + '3 TP 6240 5600')
check('vacuum_no_patches_normal', v and all(x['patches'] == 0 for x in v.values()), str(list(v.values())[:1]))
rows, s, v = vac(0, 1780, INV + '3 TP 6240 5600;1700 TP 2736 5600', {'FP_DIFF': '2'})   # Easy (RV fd2b_patch2, global 12 = 2)
A = lambda f: int(rows[f]['air']) if f in rows else None   # RV: J 22 -> 18 on the patch (row 1701), then -1 a frame to 0 (1719); 20 there,
check('vacuum_patch_refill', v and max(x['patches'] for x in v.values()) == 6 and A(1690) >= 20 and A(1699) == 18 and A(1708) == 9 and
      A(1717) == 0 and sid(s, 20)[:1] and 1698 <= sid(s, 20)[0] <= 1701 and len(sid(s, 18)) == 6 and max(x['open'] for x in v.values()) == 1,
      'J %s %s %s %s 20 %s 18 %s' % (A(1690), A(1699), A(1708), A(1717), sid(s, 20), sid(s, 18)))   # 18 at 1704 / 1709 / 1714 (J down past 15 / 10 / 5)

# =========================================================================================== item 12: JC5's rain
def j5(char, frames, script):
    rows, s, o = run('jc5', char, frames, script, {'FP_DBG_J5': '1'})
    rn = {}
    for f, t in o:
        if t[0] == 'j5rain': rn[f] = {t[i]: t[i + 1] for i in range(1, len(t) - 1, 2)}
    return rows, s, rn
rows, s, rn = j5(0, 260, '3 SETV others E 99999')          # standing at the start: (72 - 200 + GT) / 120 -> 1 drop from GT 129
g = lambda f, k: int(rn.get(f, {}).get(k, -1))
check('rain_start', g(100, 'n') == 0 and g(200, 'n') == 1 and g(200, 'vol') == 0, str((g(100, 'n'), g(200, 'n'))))
rows, s, rn = j5(0, 200, '5 TP 9000 560')                   # RV rain_tp: one a frame from the TP to 50 (n 5 at 10, 15 at 20, 50 at 60)
n = {f: int(rn[f]['n']) for f in rn}
check('rain_fill', n.get(10) in (4, 5, 6) and n.get(20) in (14, 15, 16) and n.get(70) == 50 and g(100, 'vol') == 98 and
      rn.get(100, {}).get('tint') == '0000ff' and sid(s, 293)[:1] == [1], str((n.get(10), n.get(20), n.get(70), g(100, 'vol'), sid(s, 293)[:2])))
check('rain_bolt_after_40', sid(s, 292) and g(sid(s, 292)[0], 'n') > 40, str(sid(s, 292)[:1]))
rows, s, rn = j5(0, 140, '3 SETV others E 99999;60 TP 1300 1440')   # the view over the water line: splashes
check('rain_splashes', max([int(r['spl']) for r in rn.values()] or [0]) > 0, str(max([int(r['spl']) for r in rn.values()] or [0])))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
