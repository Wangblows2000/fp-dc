#!/usr/bin/env python3
"""Host tests for the late-stage gimmick gaps (session 26; build/gimmick_audit.md, items 1, 3, 5-11, 14, 15 and the BG4 arrow sign /
FD4b terminals). Every check fails on the port before these fixes (no sprite rows / types / rules) and passes after.

RE: build/re (decompiled events; labels in the modules): fd4a 16658-16673 (vanishing blocks on PL's list, set_visible 1); pl_gim.c /
tb_gim.c per-type lists (TB5 67 conveyors, PL5 / PL6 70 / 90 time-gate pieces); DV4 5372-5378 (eruption, sound 44, launch G =
0.1 (E - 180) - 12); 3008 (YellowSpringDown: G 11, force_animation(0) then (5)); fd3a / fd3b SFX 285 (key card) / 286 (terminal);
16206 / 16192 (FD3BackPillar: x = trunc(0.8 view x + A), y = trunc(0.8 view y + B), angle (x - view x) / 426 45 - 22.5, y scale 4);
15273-15303 (fd1a's jumbotron at trunc(0.9 view x + 960), its screen shown at A 3 (X > 11111, Y > 1760), anim 7 at B 20, frames
1-3 at B 90 / 100 / 110, BackgroundSpawners at 140 < B < 240 (B even) living 11 frames, hidden at B > 270); 12249-12268 / 12704-12713
(LevelHeatSky alpha); 16183-16185 (FD3a's AlarmLight: alpha 255 - clamp(200 cos A + 180), A + 5, off at B > 300 and A < 20);
9835 (SBBackEye2 alpha drawn; SB3: json layer 1 at 0.8, session 26c); 14547 (BGArrowSign direction = the Sparkies alive). RV (original traces, session 26, under the orig
lock; scratch only): fd3a start shot (the pillars), fd3a alarm (sound 310 at 759 for the teleport at 220; + (152, 56, 0) over the
view at the pulse's peak), DV4 sound 44, BG4 arrow sign, fd1a jumbotron shots.
usage: test_late_gimmicks.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, re, math
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.environ.get('FP_TEST_BIN', os.path.join(HERE, '..', 'host', 'fpdc_host'))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
SRC = os.path.join(HERE, '..', 'src', 'game')

def run(level, frames, script, ch=0, track=None, extra=None, dump=None):
    """rows {frame: f-line dict}, obs [(frame, type, dict)], sids {frame: [ids]}, other lines"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_TRACE_SID')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    if extra: e.update(extra)
    args = [BIN, DATA, str(frames), name] + (list(dump) if dump else [])
    r = subprocess.run(args, capture_output=True, text=True, env=e, timeout=1800)
    os.unlink(name)
    rows, obs, sids, other, pend = {}, [], {}, [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2) if re.match(r'^-?[\d.]+$', t[i + 1])}
            if pend: sids[fr] = pend; pend = []
        elif t[0] == 'sid': pend.append(int(t[1]))
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]), int(t[2]), {k: (int(v, 16) if k == 'fl' else float(v)) for k, v in d.items()}))
        else: other.append(t)
    return rows, obs, sids, other, r.stderr

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def at(obs, f, t=None): return [d for fr, ty, d in obs if fr == f and (t is None or ty == t)]
def snd_frames(sids, sid): return [f for f in sorted(sids) if sid in sids[f]]
INV = '3 SETV others E 99999'

# ======================================================================== 1. FD4a: the 124 vanishing blocks are drawn (PL's row in json 72)
rows, obs, sids, _, _ = run('fd4a', 40, INV, track='2144:-1')
vb = at(obs, 39, 2144)
shown = [d for d in vb if d['hd'] == 0]
check('fd4a_vanishing_blocks_have_sprites', len(vb) == 124 and all(d['an'] >= 0 for d in vb), '%d alive, %d with a sprite' % (len(vb), sum(d['an'] >= 0 for d in vb)))
check('fd4a_vanishing_blocks_cycle_shown_and_hidden', 0 < len(shown) < 124, '%d shown' % len(shown))

# ======================================================================== 3. PL5 / PL6: every time-gate piece (70 / 90) has its sprite and orbits
for lv, n in (('pl5', 70), ('pl6', 90)):
    rows, obs, sids, _, _ = run(lv, 41, INV, track='2146:-1')
    a, b = at(obs, 10, 2146), at(obs, 40, 2146)
    moved = sum(1 for p, q in zip(a, b) if abs(p['x'] - q['x']) + abs(p['y'] - q['y']) > 0.5)
    check('%s_time_gate_pieces_all_bound' % lv, len(b) == n and all(d['an'] >= 0 for d in b), '%d alive, %d bound' % (len(b), sum(d['an'] >= 0 for d in b)))
    check('%s_time_gate_pieces_all_orbit' % lv, len(a) == n and moved == n, '%d of %d moved between frames 10 and 40' % (moved, n))

# ======================================================================== 6. TB5: all 67 conveyors bound; the last three (x 13248 / 13280 / 13408) push
rows, obs, sids, _, _ = run('tb5', 3, INV, track='1917:-1')
cv = at(obs, 2, 1917)
check('tb5_conveyors_all_bound', len(cv) == 67 and all(d['an'] >= 0 for d in cv), '%d alive, %d bound' % (len(cv), sum(d['an'] >= 0 for d in cv)))
rows, obs, sids, _, _ = run('tb5', 40, INV + ';5 TP 13424 2470')
check('tb5_conveyor_13408_pushes_left', 39 in rows and rows[39]['x'] < 13400 and rows[39]['gnd'] == 1 and abs(rows[39]['y'] - 2488) < 1,
      'x %.1f y %.1f gnd %d' % (rows[39]['x'], rows[39]['y'], rows[39]['gnd']) if 39 in rows else '-')

# ======================================================================== 5. DV4 geyser (one plate at 11600, 687): sound 44, the dust, launch G - 12
# RV (session 26, DV4 TP 11450 600; the player then stood at 11538.7): sound 44 at sample 87; the plate erupts every 150 frames
rows, obs, sids, _, _ = run('dv4', 400, INV + ';5 TP 11538 670', track='100:-1')
s44 = snd_frames(sids, 44)
check('dv4_geyser_sound_44_every_150', len(s44) == 3 and s44[1] - s44[0] == 150 and s44[2] - s44[1] == 150 and abs(s44[0] - 88) <= 2, str(s44))
dust = [f for f, t, d in obs if t == 100 and abs(d['x'] - 11600) <= 40 and d['lay'] == 1]
check('dv4_geyser_rising_dust', len(set(dust)) >= 60, '%d frames with dust' % len(set(dust)))
rows, obs, sids, _, _ = run('dv4', 120, INV + ';5 TP 11600 660')
fs = sorted(rows)
launch = [f for a, f in zip(fs, fs[1:]) if rows[f]['vy'] < -10 and rows[a]['vy'] > -10]
seq = [round(rows[launch[0] + k]['vy'], 3) for k in range(3)] if launch else []
check('dv4_geyser_launch_g_minus_12', seq == [-11.8, -11.55, -11.3], 'vy %s (DV2 rule: -13.8)' % seq)

# ======================================================================== 7. YellowSpringDown: drawn in PL1 / PL2 / SB4, the press animation (3008)
for lv, n in (('pl1', 7), ('pl2', 8), ('sb4', 4)):
    rows, obs, sids, _, _ = run(lv, 3, INV, track='29:-1')
    s = [d for d in at(obs, 2, 29) if d['x'] >= 0]
    check('%s_yellow_springs_down_have_sprites' % lv, len(s) == n and all(d['an'] >= 0 for d in s), '%d placed, %d bound' % (len(s), sum(d['an'] >= 0 for d in s)))
rows, obs, sids, _, _ = run('pl1', 20, INV + ';5 TP 3760 893;4 9 J', track='29:3744')
an = {f: d['an'] for f, t, d in obs}
rest = an.get(3)
pressed = [f for f in sorted(an) if an[f] != rest]
check('pl1_yellow_spring_down_launches_and_plays_its_press', rows.get(5, {}).get('vy') == 11.25 and rest is not None and rest >= 0 and pressed
      and pressed[0] == 5 and an[19] == rest, 'vy %s anims %s' % (rows.get(5, {}).get('vy'), [(f, an[f]) for f in sorted(an) if 3 <= f <= 12]))

# ======================================================================== 8. FD3a / FD3b sound groups: 285 (key card) and FD3a 286 (terminal)
lv = open(os.path.join(SRC, 'levels.c')).read()
def grp(name): return [int(x) for x in re.search(r'static const short SFX_%s\[\] = \{([^}]*)\}' % name, lv).group(1).replace(' ', '').split(',') if x]
check('fd3a_sfx_has_285_286', {285, 286} <= set(grp('FD3A')), str(grp('FD3A')))
check('fd3b_sfx_has_285_286', {285, 286} <= set(grp('FD3B')))
for name in ('FD3A', 'FD3B'):   # sound RAM: the stage set on top of the shared boot set <= 200 KB (11025 Hz ADPCM, tails trimmed)
    g = open(os.path.join(SRC, 'game.c')).read()
    def ids(n): return set(int(x) for x in re.sub(r'/\*.*?\*/', '', re.search(r'static const int %s\[\] = \{(.*?)\};' % n, g, re.S).group(1), flags=re.S).replace(' ', '').replace('\n', '').split(',') if x.strip().isdigit())
    shared = ids('SND_IDS') - ids('SND_LILAC') - ids('SND_CAROL') - ids('SND_MILLA')
    own = [i for i in grp(name) if i not in shared]
    kb = sum(os.path.getsize(os.path.join(DATA, 'sfx', 's%d.wav' % i)) for i in own if os.path.exists(os.path.join(DATA, 'sfx', 's%d.wav' % i))) / 1024
    check('%s_stage_sounds_converted_and_within_200kb' % name.lower(), all(os.path.exists(os.path.join(DATA, 'sfx', 's%d.wav' % i)) for i in (285, 286)) and kb <= 200, '%.1f KB' % kb)
for name, n in (('fd3a', len(grp('FD3A'))), ('fd3b', len(grp('FD3B')))):
    _, _, _, _, err = run(name, 2, INV)
    check('%s_loads_its_%d_stage_sounds' % (name, n), ('stage sounds %d' % n) in err)

# ======================================================================== 9. FD3a / FD3b back pillars (14 each): 0.8-scrolled, tilted, y scale 4
for lvl in ('fd3a', 'fd3b'):
    rows, obs, sids, _, _ = run(lvl, 200, INV + ';150 TP 2000 1500', track='2793:-1')
    p = at(obs, 199, 2793)
    check('%s_back_pillars_exported_with_sprites' % lvl, len(p) == 14 and all(d['an'] >= 0 and d['hd'] == 1 for d in p), '%d alive' % len(p))
    bad = []
    for f in (120, 199):
        cx, cy = rows[f - 1]['cx'], rows[f - 1]['cy']     # (the frame's events see the view of the frame before its camera update)
        for d in at(obs, f, 2793):
            ex, ey = math.trunc(int(cx) * 0.8 + d['a0']), math.trunc(int(cy) * 0.8 + d['a1'])
            ea = (ex - int(cx)) / 426 * 45 - 22.5
            if abs(d['x'] - ex) > 0.5 or abs(d['y'] - ey) > 0.5 or abs(d['ang'] - ea) > 0.02: bad.append((f, d['x'], ex, d['y'], ey, d['ang'], round(ea, 2)))
    check('%s_back_pillars_follow_16192' % lvl, p and not bad, str(bad[:3]))
# drawn (RV fd3a start: the pillar at x 128-172 over the parallax): the start view has its dark pillar left of the hub
with tempfile.TemporaryDirectory() as d:
    run('fd3a', 100, INV, dump=(d, '99'))
    ppm = sorted(f for f in os.listdir(d) if f.endswith('.ppm'))
    px = None
    if ppm:
        raw = open(os.path.join(d, ppm[-1]), 'rb').read()
        hdr = raw.split(b'\n', 3); w, h = map(int, hdr[1].split()); data = hdr[3]
        def pix(x, y): i = 3 * (y * w + x); return tuple(data[i:i + 3])
        px = [pix(x, 120) for x in range(140, 150)]
    check('fd3a_back_pillar_drawn_in_the_start_view', px is not None and all(max(c) < 40 for c in px), str(px[:3] if px else None))
# RV (original fd3a, the start view, the pillar placed at x 3248 logged): x 3248, y 187, angle 320.599
rows, obs, sids, _, _ = run('fd3a', 101, INV, track='2793:3248')
pp = at(obs, 100, 2793)
check('fd3a_back_pillar_where_the_original_has_it', pp and (pp[0]['x'], pp[0]['y']) == (3248, 187) and abs(pp[0]['ang'] - 320.599) < 0.01, str(pp[:1]))

# ======================================================================== 10. FD1a: the jumbotron and its screen (15273-15303)
rows, obs, sids, _, _ = run('fd1a', 420, INV + ';5 TP 11300 1850', track='2794:-1,2883:-1')
J = {f: at(obs, f, 2794) for f in range(0, 420)}
ok = all(len(J[f]) == 2 for f in range(10, 420))
check('fd1a_jumbotron_and_screen_exported', ok and all(d['an'] >= 0 and d['hd'] == 1 for d in J[100]), str(len(J[100])))
if ok:
    big = {f: J[f][0] for f in J}; scr = {f: J[f][1] for f in J}
    bad = [(f, big[f]['x'], math.trunc(int(rows[f - 1]['cx']) * 0.9 + 960)) for f in range(20, 420, 20) if big[f]['x'] != math.trunc(int(rows[f - 1]['cx']) * 0.9 + 960)]
    check('fd1a_jumbotron_x_follows_0.9_view_plus_960', not bad, str(bad[:3]))
    check('fd1a_screen_at_the_jumbotron_action_point', all(scr[f]['x'] == big[f]['x'] + 430 and scr[f]['y'] == big[f]['y'] + 128 for f in range(20, 420, 20)))
    # RV (original, TP 11300 1850 after the opening; extra objects logged): the view's bottom held at 2016 past x 10680 (view top 1776),
    # the jumbotron at (10938, 1694), its screen at (11368, 1822)
    check('fd1a_jumbotron_and_screen_where_the_original_has_them', rows[59]['cy'] == 1776 and (big[60]['x'], big[60]['y'], scr[60]['x'], scr[60]['y']) == (10938, 1694, 11368, 1822),
          'view top %s, %s %s' % (rows[59]['cy'], (big[60]['x'], big[60]['y']), (scr[60]['x'], scr[60]['y'])))
    a3 = next((f for f in range(1, 420) if scr[f]['st'] == 1), None)
    check('fd1a_screen_shown_at_x_11111_y_1760', a3 is not None and a3 <= 8, str(a3))
    if a3:
        an0 = scr[a3]['an']; an7 = scr[a3 + 25]['an']
        frs = [scr[a3 + k]['fr'] for k in (60, 92, 102, 112)]
        check('fd1a_screen_anim_7_then_frames_1_2_3', an7 != an0 and frs == [0, 1, 2, 3], 'anims %s %s frames %s' % (an0, an7, frs))
        check('fd1a_screen_back_to_its_noise_at_b_250', scr[a3 + 252]['an'] == an0)
        check('fd1a_screen_hidden_after_b_270', scr[a3 + 268]['st'] == 1 and scr[a3 + 275]['st'] == 0)
        bs = {f: [d for d in at(obs, f, 2883)] for f in range(a3, 420)}
        first = next((f for f in bs if bs[f]), None)
        check('fd1a_background_spawners_rise_from_the_screen', first is not None and 139 <= first - a3 <= 143 and max(len(v) for v in bs.values()) <= 6
              and all(d['y'] <= scr[f]['y'] for f in bs for d in bs[f]) and not bs[a3 + 260], 'first %s' % (first - a3 if first else None))

# ======================================================================== 11. TB3 / TB6: the heat sky
P9 = ('3 SETV others E 99999\n59 TP 2860 3990\n398 BOSSHPT 1980 0\n1098 BOSSHPT 1980 0\n' + ''.join('%d HP 14\n' % f for f in range(80, 1440, 30)) +
      '1440 2600 R\n' + ''.join('%d %d J\n' % (f, f + 14) for f in range(1460, 2600, 40)))
rows, obs, sids, other, _ = run('tb3', 2400, P9, track='2001:-1,2002:-1', extra={'FP_DBG_TBP': '1'})
H = {f: d for f, t, d in obs if t == 2001}
tbp = {int(t[1]): dict(zip(t[2::2], t[3::2])) for t in other if t[0] == 'tbp'}
s310 = snd_frames(sids, 310)
on = next((f for f in sorted(H) if H[f]['st'] == 1), None)
check('tb3_heat_sky_and_cascade_end_exported', H and any(t == 2002 for f, t, d in obs))
check('tb3_heat_sky_shown_at_the_chase_12249', on is not None and s310 and abs(on - s310[0]) <= 1 and H[on]['a0'] == 0, '%s %s' % (on, s310[:1]))
ramp = 0                                            # (12252: + 1 a frame from 0 while K is 0: the camera looks back, 231 frames)
while on is not None and on + ramp + 1 in H and H[on + ramp + 1]['a0'] == ramp + 1: ramp += 1
check('tb3_heat_sky_alpha_rises_1_a_frame_while_the_camera_looks_back', 225 <= ramp <= 235, '%d frames' % ramp)
k1 = [f for f in sorted(tbp) if tbp[f]['ph'] == '6' and tbp[f]['K'] == '1' and f in H and f - 1 in rows]
def a12260(wall, cx):
    d = abs(int(wall) - int(cx)) * 0.5
    return 55 if d > 200 else 235 if d < 20 else max(0, min(255, 255 - int(d)))
bad = [(f, H[f]['a0'], a12260(float(tbp[f]['wall']), rows[f - 1]['cx'])) for f in k1 if abs(H[f]['a0'] - a12260(float(tbp[f]['wall']), rows[f - 1]['cx'])) > 3]
check('tb3_heat_sky_alpha_from_the_wall_12260', k1 and len(bad) <= len(k1) // 20, '%d of %d off %s' % (len(bad), len(k1), bad[:3]))
k2 = [f for f in sorted(tbp) if tbp[f]['ph'] == '6' and tbp[f]['K'] == '2' and f in H]
check('tb3_heat_sky_fades_out_past_the_cascade_end_12268', k2 and H[k2[-1]]['a0'] == 0 and float(tbp[k2[-1]]['px']) > 7473, '%s' % (H[k2[-1]]['a0'] if k2 else None))
X5 = '3 SETV others E 99999\n60 400 R\n698 BOSSHPT 1993 0\n1298 BOSSHPT 1993 0\n1898 BOSSHPT 1993 0\n' + ''.join('%d SETV others E 99999\n' % f for f in (300, 600, 900, 1200, 1500, 1800, 2100))
rows, obs, sids, other, _ = run('tb6', 2300, X5, track='2001:-1', extra={'FP_DBG_TBX': '1'})
H = {f: d for f, t, d in obs if t == 2001}
tbx = {int(t[1]): dict(zip(t[2::2], t[3::2])) for t in other if t[0] == 'tbx'}
s309 = snd_frames(sids, 309)
on = next((f for f in sorted(H) if H[f]['st'] == 1), None)
check('tb6_heat_sky_shown_at_the_defeat_12704', on is not None and s309 and abs(on - s309[0]) <= 1, '%s %s' % (on, s309))
def a12711(F): v = 255 - F * 0.5; return 127 if v < 128 else max(0, min(255, 255 - int(v)))
shown = [f for f in sorted(H) if on is not None and f >= on and H[f]['st'] == 1]   # (F = 1 in the defeat frame: 12711 runs after 12704)
bad = [(f, H[f]['a0'], a12711(f - on + 1)) for f in shown if H[f]['a0'] != a12711(f - on + 1)]
check('tb6_heat_sky_alpha_12711', len(shown) > 300 and not bad and max(H[f]['a0'] for f in shown) == 127, '%d frames %s' % (len(shown), bad[:3]))
gone = shown[-1] + 1 if shown else None
check('tb6_heat_sky_gone_with_the_squid_12713', gone is not None and 359 <= gone - on <= 362 and H.get(gone, {}).get('st') == 0, str(gone - on if gone else None))
# drawn: additive (248, 196, 16) x alpha over the view (image 6229): the same frame with and without the tint (host FP_NO_VIEW_TINT)
def frame_px(level, f, script, extra=None):
    with tempfile.TemporaryDirectory() as d:
        run(level, f + 1, script, extra=extra, dump=(d, str(f)))
        p = os.path.join(d, 'f%05d.ppm' % f)
        return open(p, 'rb').read().split(b'\n', 3)[3] if os.path.exists(p) else b''
def tint_delta(level, f, script):
    a, b = frame_px(level, f, script), frame_px(level, f, script, {'FP_NO_VIEW_TINT': '1'})
    dl = sorted(tuple(a[i + k] - b[i + k] for k in range(3)) for i in range(0, min(len(a), len(b)), 3 * 31) if max(b[i:i + 3]) < 90)
    return dl[len(dl) // 2] if dl else None
if shown:
    f = on + 300                                     # alpha 127: + trunc(c 127 / 255)
    check('tb6_heat_sky_drawn_additive_248_196_16', tint_delta('tb6', f, X5) == (123, 97, 7), str(tint_delta('tb6', f, X5)))

# ======================================================================== 14. FD3a: the alarm light's pulse (16183-16185)
rows, obs, sids, other, _ = run('fd3a', 1000, INV + ';5 TP 5200 1000', extra={'FP_DBG_FDALARM': '1'})
AL = {int(t[1]): dict(zip(t[2::2], t[3::2])) for t in other if t[0] == 'fdal'}
s310 = snd_frames(sids, 310)
on = next((f for f in sorted(AL) if AL[f]['shown'] == '1'), None)
check('fd3a_alarm_light_shown_with_the_alarm_16183', on is not None and s310 and abs(on - s310[0]) <= 1, '%s %s' % (on, s310))
def a16184(A): return 75 if A in (90, 270) else (255 if 200 * math.cos(math.radians(A)) + 180 < 0 else max(0, min(255, 255 - int(200 * math.cos(math.radians(A)) + 180))))
bad = [(f, AL[f]['alpha']) for f in sorted(AL) if AL[f]['fl'] == '1' and int(AL[f]['alpha']) != a16184((float(AL[f]['A']) - 5) % 360)]
check('fd3a_alarm_light_alpha_16184', on and not bad, str(bad[:3]))
off = next((f for f in sorted(AL) if on and f > on and AL[f]['fl'] == '0'), None)
check('fd3a_alarm_light_stops_16185', off and float(AL[off]['B']) > 300 and float(AL[off]['A']) < 20 and AL[off]['alpha'] == '0', str(AL.get(off)))
# RV fd3a alarm (shots 37 frames after the alarm): + (152, 56, 0) over the dark parts of the view at alpha 255
with tempfile.TemporaryDirectory() as d:
    imgs = {}
    for f in (on - 1, on + 36) if on else ():
        run('fd3a', f + 1, INV + ';5 TP 5200 1000', dump=(d, str(f)))
        raw = open(os.path.join(d, 'f%05d.ppm' % f), 'rb').read(); hdr = raw.split(b'\n', 3); imgs[f] = hdr[3]
    dl = []
    if len(imgs) == 2:
        a, b = imgs[on + 36], imgs[on - 1]
        for i in range(0, len(a), 3 * 97):
            if max(b[i:i + 3]) < 90: dl.append(tuple(a[i + k] - b[i + k] for k in range(3)))
    dl.sort()
    check('fd3a_alarm_light_adds_152_56_0', dl and dl[len(dl) // 2] == (152, 56, 0), str(dl[len(dl) // 2] if dl else None))

# ======================================================================== 15. SB3: the back eyes' alpha fade is drawn (9835)
# (session 26c, RV small fixes sb3_eye: SB3's eyes stand on json layer 1, scrolling at 0.8, and only SBBackEye2, the glow, fades:
# the player at 7893 sees the eye at 6344, 400)
rows, obs, sids, _, _ = run('sb3', 342, INV + ';5 TP 7893 495', track='1127:6344')
E = {f: d['a2'] for f, t, d in obs if d['an'] == max(dd['an'] for ff, tt, dd in obs)}
check('sb3_back_eye_alpha_cycles', E.get(300) is not None and E[300] < 10 and 100 < E.get(320, 0) < 160 and E.get(340) == 255, '%s %s %s' % (E.get(300), E.get(320), E.get(340)))
cam = {f: (rows[f]['cx'], rows[f]['cy']) for f in (300, 320, 340) if f in rows}
imgs = {f: frame_px('sb3', f, INV + ';5 TP 7893 495') for f in (300, 320, 340)}   # (the view stands still from ~200: one picture per alpha)
if len(set(cam.values())) == 1 and all(imgs.values()):
    cx, cy = [math.trunc(0.8 * int(v)) for v in cam[300]]   # (json layer 1 at 0.8)
    box = [(x, y) for y in range(max(0, 400 - cy - 64), min(240, 400 - cy + 256)) for x in range(max(0, 6344 - cx - 64), min(426, 6344 - cx + 256))]
    eye = [3 * (y * 426 + x) for x, y in box if imgs[340][3 * (y * 426 + x):3 * (y * 426 + x) + 3] != imgs[300][3 * (y * 426 + x):3 * (y * 426 + x) + 3]]
    mid = [i for i in eye if imgs[320][i:i + 3] not in (imgs[340][i:i + 3], imgs[300][i:i + 3]) and all(min(imgs[300][i + k], imgs[340][i + k]) <= imgs[320][i + k] <= max(imgs[300][i + k], imgs[340][i + k]) for k in range(3))]   # (between the hidden and the opaque eye)
    check('sb3_back_eye_drawn_half_transparent', len(eye) > 5000 and len(mid) > len(eye) // 2, '%d eye pixels, %d blended at alpha %s' % (len(eye), len(mid), E.get(320)))
else: check('sb3_back_eye_drawn_half_transparent', False, 'camera %s' % cam)

# ======================================================================== BG4 arrow sign (14547) and FD4b's terminals
rows, obs, sids, _, _ = run('bg4', 12, INV + ';6 KILLT 2480', track='2307:-1')
A = {f: d for f, t, d in obs}
check('bg4_arrow_sign_exported', 3 in A and A[3]['an'] >= 0)
check('bg4_arrow_sign_turns_when_the_sparky_is_gone', 3 in A and 11 in A and A[11]['an'] >= 0 and A[11]['an'] != A[3]['an'] and A[11]['fr'] == A[11]['fr'],
      '%s -> %s' % (A.get(3, {}).get('an'), A.get(11, {}).get('an')))
rows, obs, sids, _, _ = run('fd4b', 3, INV, track='2995:-1')
t = at(obs, 2, 2995)
check('fd4b_computer_terminals_have_sprites', len(t) == 5 and all(d['an'] >= 0 for d in t), '%d alive, %d bound' % (len(t), sum(d['an'] >= 0 for d in t)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
