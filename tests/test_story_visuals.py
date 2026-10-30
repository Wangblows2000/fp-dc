#!/usr/bin/env python3
"""Adventure story scenes against the original's look (session 27, cutscene polish; src/game/cutscene.c, tools/story_rv2scn.py,
docs/ADVENTURE.md section 12). The host build plays a scene (ADVNEW, SCENE) with the reader's presses of the original's RV
trace (tools/orig/story_rv.sh, STORY_TAP=0; the frames below are scene-relative) and the frame is compared with values measured
on the original's screenshots at the same scene time (852 x 480 box-filtered to 426 x 240): sampled pixels, the actors drawn
(ACTORS), the location name and the dialogue's line wrap. The music runs on across chained scenes (FP_TRACE_SND).
usage: test_story_visuals.py [data_dir]   (default: converted2, with tools/import_story.py's story rooms)"""
import os, re, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.environ.get('FP_HOST_BIN', os.path.join(ROOT, 'host', 'fpdc_host'))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
S0 = 240                                   # the host frame the scene starts at

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + str(info) + ']' if info != '' else ''))
    fails += 0 if cond else 1

def play(sid, at, presses=(), chr_=0, actors=(), env=None):
    """the scene's frame at scene time `at` (PPM rows) and the host's output; presses / actors: scene times"""
    d = tempfile.mkdtemp(prefix='fpsv')
    sc = '5 ADVNEW %d\n%d SCENE %s\n' % (chr_, S0, sid)
    for p in presses: sc += '%d %d J\n' % (S0 + p, S0 + p + 1)
    for t in actors: sc += '%d ACTORS\n' % (S0 + t)
    scn = os.path.join(d, 's.txt')
    open(scn, 'w').write(sc)
    e = dict(os.environ, FP_TRACE_SND='1')
    for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE'): e.pop(k, None)
    e.update(env or {})
    r = subprocess.run([BIN, DATA, str(S0 + at + 1), scn, d, str(S0 + at)], capture_output=True, text=True, env=e)
    img = None
    p = os.path.join(d, 'f%05d.ppm' % (S0 + at))
    if os.path.exists(p):
        b = open(p, 'rb').read()
        m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\s', b)
        w, h = int(m.group(1)), int(m.group(2)); px = b[m.end():]
        img = (w, h, px)
    shutil.rmtree(d)
    return img, r.stdout + r.stderr

def pix(img, x, y):
    w, h, px = img; o = 3 * (y * w + x)
    return tuple(px[o:o + 3])

def near(a, b, tol): return all(abs(int(u) - int(v)) <= tol for u, v in zip(a, b))

def samples(name, img, pts, tol=24):
    """pts: (x, y, (r, g, b)) measured on the original's shot"""
    if img is None: check(name, False, 'no frame'); return
    bad = [(x, y, pix(img, x, y), c) for x, y, c in pts if not near(pix(img, x, y), c, tol)]
    check(name, not bad, bad[:3] if bad else '%d points' % len(pts))

def drawn(out):
    """the actors drawn at the last ACTORS: [(handle, row, layer, alpha)]"""
    blk = out.split('csxa seg')[-1].splitlines()[1:] if 'csxa seg' in out else []
    return [(int(t[2]), t[3], int(t[7]), int(t[8])) for t in (l.split() for l in blk) if len(t) >= 10 and t[0] == 'csxa']

def head(out):
    m = re.findall(r'csxa seg (\d+) clock (\d+) fade (\d+) rgb ([0-9a-f]+) wipe (-?\d+)', out)
    return m[-1] if m else None

def region(img, x0, y0, x1, y1):
    w, h, px = img; n = 0; acc = [0, 0, 0]
    for y in range(y0, y1):
        for x in range(x0, x1):
            o = 3 * (y * w + x); n += 1
            for c in range(3): acc[c] += px[o + c]
    return tuple(round(v / n) for v in acc)

# ------------------------------------------------------------------------------------------------------------- checks
# (name, scene, scene frame, the reader's presses before it (scene frames, RV), [(box x0, y0, x1, y1, mean RGB on the original's
# shot)], what it shows). Mean RGB of the box, 426 x 240 units, within TOL per channel. RV: build/orig_traces/pol (never committed).
CHECKS = []
TOL = 12
REGIONS = [
    ('opening_smoke_translucent', 'adv_opening', 1200, [],              # RV c81_g2 1200: the crash smoke over the airship (PAR_ALPHA 591)
     [(150, 120, 300, 175, (78, 76, 109))]),
    ('dv_crash2_location_name', 'dv_crash2', 107, [],                   # RV c75_g1 2640: DRAGON VALLEY / LILAC'S TREEHOUSE (RE 3316-3319)
     [(16, 192, 200, 232, (101, 86, 61))]),
    ('dialog_wrap_37_columns', 'dv_crash2', 347, [],                    # RV c75_g1 2880: "... out here? The" on the first row (37 columns)
     [(321, 10, 345, 26, (54, 71, 117))]),
    ('restart_fade_dark', 'jiang_river2', 179, [118],                   # RV r36_g99 840: the frame restarts (10 % brightness)
     [(0, 0, 426, 240, (15, 7, 6))]),
    ('flashback_shader20', 'th_torque_revealed2', 71, [],              # RV b15_g11_l 3120: FlashbackBG_3617 darkens (dst x (1 - src))
     [(0, 0, 100, 60, (35, 45, 80)), (300, 0, 426, 60, (23, 26, 97))]),
    ('caravan_layer_offset', 'rm_meet_milla', 120, [],                  # RV b76_g2_l 120: json 77's layer 1 (the caravan) at x -3200
     [(280, 40, 426, 200, (91, 102, 60))]),
    ('airship_layer_offset', 'zao_airship', 600, [],                    # RV b78_g2_l 600: the airship's hull (json 79's layer 1 risen)
     [(0, 20, 426, 190, (174, 81, 79))]),
    ('world_piece_not_tiled', 'fn_panther_defeated', 600, [70, 111, 233],   # RV b78_g1_l 600: no Zao airship (world piece 3962)
     [(200, 150, 426, 220, (34, 48, 64))]),
    ('night_sky_untraced_pieces', 'zao_airship2', 808, [159, 200, 241, 282, 394, 435, 476, 623, 664, 705, 746],   # RV b78_g2_l 3000
     [(0, 0, 426, 100, (47, 33, 53))]),
    ('palace_daytime_sky', 'st_magister_talk', 600, [139, 180, 221, 262],   # RV a83_g8 600: DaytimeSky_4118 (a SystemBox)
     [(60, 10, 330, 110, (120, 150, 176))]),
    ('dialog_box_moved', 'ending', 2520, [879, 1524, 1605, 1707, 1996, 2037, 2078, 2119, 2160, 2448, 2489],   # RV e81_g3n 2520: y 148
     [(30, 150, 380, 200, (14, 51, 139))]),
    ('dragon_flight_black_sky', 'sf_army_l', 3300,                      # RV r81_g0l 3300: black behind the dragon (RE 20454), the ground
     [434, 682, 722, 763, 1177, 1639, 1680, 1721, 1762, 1803, 1949, 1990, 2031, 2072, 2288, 2588, 2919, 3144, 3266],
     [(0, 0, 426, 60, (10, 6, 30)), (0, 215, 426, 240, (146, 141, 153))]),
]

def check_regions():
    for name, sid, at, presses, boxes in REGIONS:
        img, out = play(sid, at, presses, actors=(at,) if name == 'dragon_flight_black_sky' else ())
        if img is None: check(name, False, 'no frame'); continue
        got = [(region(img, *b[:4]), b[4]) for b in boxes]
        check(name, all(near(g, w, TOL) for g, w in got), got)
        if name == 'dragon_flight_black_sky':            # RV r81_g0l 1860-1876: the white bursts are gone long before (address reuse)
            check('bursts_gone_by_3300', 'csxa seg' in out and not any(h == 31 for h, *_ in drawn(out)) and any(h == 4016 for h, *_ in drawn(out)),
                  [d for d in drawn(out) if d[0] == 31][:3])   # (the guards, NPCShieldGuard_4016, drawn)
CHECKS.append(check_regions)

def music_events(sid, frames):
    sc = '5 ADVNEW 0\n%d SCENE %s\n' % (S0, sid) + ''.join('%d %d J\n' % (f, f + 1) for f in range(S0 + 60, frames, 15))
    d = tempfile.mkdtemp(prefix='fpsv'); scn = os.path.join(d, 's.txt'); open(scn, 'w').write(sc)
    e = dict(os.environ, FP_TRACE_SND='1')
    for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE'): e.pop(k, None)
    r = subprocess.run([BIN, DATA, str(frames), scn], capture_output=True, text=True, env=e)
    shutil.rmtree(d)
    return [l for l in r.stdout.splitlines() if l.startswith(('music', 'story start'))]

def check_music():
    # ending -> ending2: m_ending runs on (no restart: the original starts it once, RV e81_g3n; ending2 has no music of its own)
    ev = music_events('ending', 3700)
    i = next((k for k, l in enumerate(ev) if l.startswith('story start ending2')), None)
    check('music_ending2_no_restart', i is not None and sum(l.startswith('music bgm/m_ending') for l in ev) == 1
          and not any(l.startswith('music stop') for l in ev[next(k for k, l in enumerate(ev) if l.startswith('story start ending ')):]), ev[-6:])
    # sm_zao_help (C1) -> zao_airship (B): m_hub2 keeps playing into the airship (RV c83_g4: no BGM at F 4215)
    ev = music_events('sm_zao_help', 2400)
    i = next((k for k, l in enumerate(ev) if l.startswith('story start zao_airship')), None)
    check('music_chained_scene_not_silent', i is not None and not any(l.startswith('music stop') for l in ev[i - 3:i + 1])
          and any(l.startswith('music bgm/m_hub2') for l in ev[:i]), ev[-5:])
CHECKS.append(check_music)

if __name__ == '__main__':
    if not os.path.isfile(BIN): sys.exit('no host build: make -C host')
    for c in CHECKS: c()
    print('%d failures' % fails)
    sys.exit(1 if fails else 0)
