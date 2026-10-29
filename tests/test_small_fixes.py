#!/usr/bin/env python3
"""Host tests for the small gameplay / audio-visual fixes (session 26c; TODO "Session 26c"). Every check fails on the port before the
fixes and passes after.

Evidence (RE build/re, RV original traces of session 26c, scratch only: rv1 / rv2 under the orig lock):
 1. RM2's jump pads (handle 1284) draw their own images (jpad_rm rows for json 26; RV rm2_pad f40).
 2. Deaths (RE 2416 / 2417 -> loop_check_death -> loop_sound_die_8; 1689 turns the death group 0x21a38 on a frame later, whose
    1959 / 4724 leaves a ridden bike wrecked and 1960 / 4725 explodes every wrecked ObjectBike (sound 29, Carol's off-map template
    is one). RV death_l / death_c / death_b (DV1, petals 0, a Turretus pellet): Lilac 17 + 91, Carol 17 + 128 then 29 a frame later
    (on the bike too, the wreck a frame after the hit), no 25 / hurt voice at a death.
 3. TB4 metal doors (RE 11837 / 11838): 279 when an opening front reaches A 1, 280 when a closing one reaches A 2 (RV tb4_door: the
    door at x 24 closing from 10 at the teleport, 280 at 75 = port 74), not at A 0 / 10.
 4. BG2's fly-by plane (RE 14241 / 4874-4877): LevelBackgroundPlane2_2227 from X > 1130, sound 102 when it reaches view x - 49 (RV
    bg2_102 / c_jumppad_bg2: 102 at 120 for the teleport to 8330 at 60).
 5. JC5's white card (OT_J5_FADE has no sprite: objects_draw skipped it; RV l_kill2 f3600: the screen white).
 6. FD1a's pod rain starts at the jumbotron script's A 3 / B 160 (RE 15284 -> 15287), not at X > 800.
 7. FD1b's alarm (RE 15388): alpha 255 - clamp(200 cos A + 180) (75 at 90 / 270) drawn additively (RV fd1b_alarm: brightest near
    A 190, not at A 90).
 8. Spring beams (RE 2997-3012): an EffectsSpringBeam_540 row per direction where such springs stand; one at each launch.
 9. FD1a's jumbotron dialog: voices br_pa_01 (15273) / br_pa_02 (B 20, 15279) and the subtitle paragraphs (RV fd1a_dlg: both voices
    at 650 / 669 for the teleport at 650; f700 paragraph 0 in two rows; late_gim f532 paragraph 1).
10. FD transporter nodes are visible (RV fd1a_node f440: the plain junction circles at (1152, 224) / (1408, 224)).
11. SB3's back eyes stand on json layer 1 (0.8 scroll) and only SBBackEye2 (the glow, list Frames + 0x125b0) fades (RV sb3_eye rv2:
    TP 7893 495: the glow's A + 4 a frame, the base eye's A 0, the eye on screen); TB3 / TB6 heat sky compared with the original's
    shots (p7_l / x2_l) by hand (BUILD_AND_TEST_LOG).
usage: test_small_fixes.py [data_dir]   (default: converted2)"""
import os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.environ.get('FP_TEST_BIN', os.path.join(HERE, '..', 'host', 'fpdc_host'))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
SRC = os.path.join(HERE, '..', 'src', 'game')

def run(level, frames, script, ch=0, env=None, dump=None):
    """rows {frame: f-line dict}, sids {frame: [ids]} (the events of frame f), voices {frame: [names]}, other lines, ppm images"""
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK', 'FP_TRACE_SID')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'}); e.update(env or {})
    imgs = {}
    with tempfile.TemporaryDirectory() as d:
        args = [BIN, DATA, str(frames), name] + ([d, str(dump[0])] if dump else [])
        r = subprocess.run(args, capture_output=True, text=True, env=e, timeout=3600)
        if dump:
            for fn in os.listdir(d):
                fr = int(fn[1:6])
                if fr in dump[1]:
                    raw = open(os.path.join(d, fn), 'rb').read(); hdr = raw.split(b'\n', 3); w, h = map(int, hdr[1].split()); imgs[fr] = (w, h, hdr[3])
    os.unlink(name)
    rows, sids, vox, other, ps, pv = {}, {}, {}, [], [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            fr = int(t[1]); rows[fr] = {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2) if re.match(r'^-?[\d.]+$', t[i + 1])}
            if ps: sids[fr] = ps; ps = []
            if pv: vox[fr] = pv; pv = []
        elif t[0] == 'sid': ps.append(int(t[1]))
        elif t[0] == 'voice': pv.append(t[1])
        else: other.append(t)
    return rows, sids, vox, other, imgs

def pix(img, x, y):
    w, h, data = img; i = 3 * (y * w + x); return tuple(data[i:i + 3])

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def frames_of(d, v): return [f for f in sorted(d) if v in d[f]]
def ens_rows(lvl):
    rep = open(os.path.join(DATA, lvl, lvl + '_report.txt'), encoding='utf-8', errors='replace').read()
    m = re.findall(r"^ens bank: \[(.*)\]$", rep, re.M)
    return [s.strip().strip("'") for s in m[-1].split(',')] if m else []
def dead_frame(rows): return next((f for f in sorted(rows) if rows[f].get('st') == 6), None)
INV = '3 SETV others E 99999'

# ======================================================================== 1. RM2: the pads' own images
check('rm2_pads_have_their_own_rows', {'jpad_rm', 'jpad_rm_lit'} <= set(ens_rows('rm2')), str([r for r in ens_rows('rm2') if 'jpad' in r]))

# ======================================================================== 2. death sounds (DV1: petals 0, the Turretus' pellet; RV 189)
# a hit with no petals (the host's HURT: the damage rule itself) and a fall below the act (2417; RV pit_c / pit_l: Carol 128 at the
# teleport frame 5, 29 at 6; Lilac 91 at 5)
for ch, nm, die in ((0, 'lilac', 91), (2, 'carol', 128), (3, 'carol_bike', 128), (4, 'milla', 178)):
    rows, sids, vox, _, _ = run('dv1', 80, '5 TP 68.715 440;50 HP 0;60 HURT 10', ch)
    d = dead_frame(rows)
    at = sids.get(d, []) if d else []
    check('%s_damage_death_plays_its_die_sound_only' % nm, d is not None and die in at and 25 not in at and not vox.get(d),
          'death %s sounds %s voices %s' % (d, at, vox.get(d)))
    rows, sids, vox, _, _ = run('dv1', 40, '5 TP 3000 1950', ch)
    d = dead_frame(rows)
    at = sids.get(d, []) if d else []
    check('%s_pit_death_plays_its_die_sound' % nm, d is not None and die in at and 25 not in at, 'death %s %s' % (d, at))
    if ch in (2, 3):
        check('%s_death_29_a_frame_later' % nm, d is not None and 29 not in at and 29 in sids.get(d + 1, []) and len(frames_of(sids, 29)) == 1,
              'death %s 29 at %s' % (d, frames_of(sids, 29)))
    else:
        check('%s_death_no_29' % nm, d is not None and not frames_of(sids, 29), str(frames_of(sids, 29)))
check('lilac_has_91_in_her_set', ' 91 ' in re.search(r'static const int SND_LILAC\[\] = \{([^}]*)\}', open(os.path.join(SRC, 'game.c')).read()).group(1) + ' ')

# ======================================================================== 3. TB4 metal doors: 280 at A 2 (RV 75 = port 74), 279 at A 1
rows, sids, vox, _, _ = run('tb4', 100, '60 TP 4192 2758', 2, {'FP_SPAWN_X': '71', 'FP_SPAWN_Y': '2586'})
check('tb4_door_280_when_closing_reaches_a2', frames_of(sids, 280) == [74], str(frames_of(sids, 280)))
check('tb4_door_279_when_opening_reaches_a1', frames_of(sids, 279) == [1], str(frames_of(sids, 279)))

# ======================================================================== 4. BG2's fly-by plane: sound 102 (RV 120 = port 119 +- 2)
rows, sids, vox, _, imgs = run('bg2', 160, INV + ';60 TP 8330 870', 0)
s102 = frames_of(sids, 102)
check('bg2_plane_sound_102', len(s102) == 1 and abs(s102[0] - 119) <= 2, str(s102))
check('bg2_plane_row_exported', 'bgf_plane' in ens_rows('bg2'))
check('bg2_sfx_group_has_102', '102' in re.search(r'static const short SFX_BG2\[\] = \{([^}]*)\}', open(os.path.join(SRC, 'levels.c')).read()).group(1).replace(' ', '').split(','))
rows, sids, vox, _, _ = run('bg2', 160, INV + ';60 TP 8330 870', 4)
check('bg2_plane_not_for_milla', not frames_of(sids, 102), str(frames_of(sids, 102)))

# ======================================================================== 5. JC5's white card is drawn (sound 76 = F 1200: opacity 255)
def jc5_fight():
    L = [''.join('%d SETV others E 99999\n' % f for f in range(3, 6000, 300)), '60 TP 13300 560', '70 140 R', '280 BOSSHPT 822 1', '1100 BOSSHPT 1485 1']
    for f in range(300, 2400, 36): L += ['%d %d J' % (f, f + 8), '%d %d X' % (f + 12, f + 14), '%d %d X' % (f + 22, f + 24)]
    for k, f in enumerate(range(300, 2400, 100)): L.append('%d %d %s' % (f, f + 100, 'LR'[k & 1]))
    return '\n'.join(L)
rows, sids, vox, _, _ = run('jc5', 3200, jc5_fight(), 0)
s76 = frames_of(sids, 76)
if s76:
    t = s76[0] + 20
    rows, sids, vox, _, imgs = run('jc5', t + 1, jc5_fight(), 0, dump=(t, (t,)))
    im = imgs.get(t)
    px = [pix(im, x, y) for x, y in ((20, 120), (213, 120), (400, 220))] if im else []   # (the HUD stays over the card)
    check('jc5_white_card_drawn', px and all(min(c) >= 230 for c in px), '76 at %d, pixels %s' % (s76[0], px))
else:
    check('jc5_white_card_drawn', False, 'no sound 76')

# ======================================================================== 6. FD1a's pod rain: from the dialog's A 3 / B 160 only
rows, sids, vox, _, _ = run('fd1a', 300, INV + ';5 TP 11800 1600', 0)
check('fd1a_no_pod_rain_above_y_1760', not frames_of(sids, 345), str(frames_of(sids, 345)[:4]))
rows, sids, vox, _, _ = run('fd1a', 400, INV + ';5 TP 11300 1850;150 TP 11800 1950', 0)
s345 = frames_of(sids, 345)
check('fd1a_pod_rain_from_b160', s345 and 160 <= s345[0] <= 175 and all(b - a == 10 for a, b in zip(s345, s345[1:])), str(s345[:4]))

# ======================================================================== 7. FD1b's alarm light: cos rule, additive (RV fd1b_alarm)
SC = INV + ';300 TP 13000 2000'
rows, sids, vox, oth, _ = run('fd1b', 345, SC, 0, {'FP_DBG_FDB': '1'})
al = {int(t[1]): int(dict(zip(t[2::2], t[3::2])).get('al', -1)) for t in oth if t[0] == 'fdb'}
check('fd1b_alarm_alpha_cos_rule', [al.get(f) for f in (300, 317, 318, 336)] == [0, 58, 75, 255], str({f: al.get(f) for f in (300, 317, 318, 336)}))
f = 337
_, _, _, _, a = run('fd1b', f + 1, SC, 0, dump=(f, (f,)))
_, _, _, _, b = run('fd1b', f + 1, SC, 0, {'FP_NO_VIEW_TINT': '1'}, dump=(f, (f,)))
dd = [tuple(x - y for x, y in zip(pix(a[f], *p), pix(b[f], *p))) for p in ((20, 120), (213, 60), (400, 200))] if f in a and f in b else []
check('fd1b_alarm_drawn_additive_152_56_0', dd and all(abs(x[1] - 56) <= 1 and x[2] == 0 for x in dd), str(dd))

# ======================================================================== 8. spring beams: the rows where springs stand, one per launch
check('dv1_spring_beam_rows', {'spb0', 'spb4', 'spb8', 'spb12'} <= set(ens_rows('dv1')) and not {'spb16', 'spb24'} & set(ens_rows('dv1')), str([r for r in ens_rows('dv1') if r.startswith('spb')]))
check('pl1_spring_beam_rows', {'spb8', 'spb16', 'spb24'} <= set(ens_rows('pl1')), str([r for r in ens_rows('pl1') if r.startswith('spb')]))
check('dv4_no_spring_beam_rows', not [r for r in ens_rows('dv4') if r.startswith('spb')])
rows, sids, vox, oth, _ = run('dv1', 40, '5 TP 3984 1300', 0, {'FP_TRACK': '100:-1'})
ens = ens_rows('dv1'); a8 = ens.index('spb8') if 'spb8' in ens else -2
beams = sorted({int(t[1]) for t in oth if t[0] == 'ob' and t[2] == '100' and 'an' in t and int(t[t.index('an') + 1]) == a8})
launch = frames_of(sids, 5)
check('dv1_up_spring_beam_at_the_launch', launch and beams and beams[0] in (launch[0], launch[0] + 1) and len(beams) <= 21, 'launch %s beam frames %s' % (launch, beams[:3] + beams[-1:]))

# ======================================================================== 9. FD1a's jumbotron dialog: voices and subtitles
rows, sids, vox, oth, imgs = run('fd1a', 820, INV + ';650 TP 11300 1850', 0, dump=(1, (700, 790)))
v1, v2 = frames_of(vox, 'br_pa_01'), frames_of(vox, 'br_pa_02')
check('fd1a_dialog_voices_at_650_669', v1 and v2 and abs(v1[0] - 650) <= 1 and v2[0] - v1[0] == 19, '%s %s' % (v1, v2))
def white_rows(im): return [y for y in range(170, 230) if sum(1 for x in range(60, 210) if min(pix(im, x, y)) >= 240) >= 12]   # the glyphs' white rows ("BREVON:")
w700, w790 = (white_rows(imgs[f]) if f in imgs else [] for f in (700, 790))
check('fd1a_subtitle_paragraph0_two_rows', w700 and min(w700) == 190 and 210 <= max(w700) <= 221, str(w700[:2] + w700[-2:]))
check('fd1a_subtitle_paragraph1_one_row', w790 and min(w790) == 200 and max(w790) <= 211, str(w790[:2] + w790[-2:]))
lv = open(os.path.join(SRC, 'levels.c')).read()
check('fd1a_dialog_voices_resident', re.search(r'"fd1a".*?\.vox = \(const char \*const\[\]\)\{ "br_pa_01", "br_pa_02" \}', lv, re.S) is not None)
def grp_kb(name, extra=()):
    g = open(os.path.join(SRC, 'game.c')).read()
    def ids(n): return set(int(x) for x in re.sub(r'/\*.*?\*/', '', re.search(r'static const int %s\[\] = \{(.*?)\};' % n, g, re.S).group(1), flags=re.S).replace(' ', '').replace('\n', '').split(',') if x.strip().isdigit())
    shared = ids('SND_IDS') - ids('SND_LILAC') - ids('SND_CAROL') - ids('SND_MILLA')
    grp = [int(x) for x in re.search(r'static const short SFX_%s\[\] = \{([^}]*)\}' % name, lv).group(1).replace(' ', '').split(',') if x]
    fs = [os.path.join(DATA, 'sfx', 's%d.wav' % i) for i in grp if i not in shared] + [os.path.join(DATA, 'sfx', v + '.wav') for v in extra]
    return all(os.path.exists(p) for p in fs), sum(os.path.getsize(p) for p in fs if os.path.exists(p)) / 1024
ok, kb = grp_kb('FD1A', ('br_pa_01', 'br_pa_02'))
check('fd1a_sounds_and_voices_within_200kb', ok and kb <= 200, '%.1f KB' % kb)
ok, kb = grp_kb('BG2')
check('bg2_sounds_within_200kb', ok and kb <= 200, '%.1f KB' % kb)

# ======================================================================== 10. FD transporter nodes drawn
rows, sids, vox, oth, _ = run('fd1a', 3, INV, 0, {'FP_TRACK': '2754:-1'})
nodes = [t for t in oth if t[0] == 'ob' and t[1] == '2']
vis = [t for t in nodes if int(t[t.index('an') + 1]) >= 0 and t[t.index('hd') + 1] == '0']
check('fd1a_transporter_nodes_visible', nodes and len(vis) == len(nodes), '%d of %d' % (len(vis), len(nodes)))
check('fd1a_node_1152_224_alive', any(t[5] == '1152.00' and t[7] == '224.00' for t in vis))

# ======================================================================== 11. SB3's eyes on their 0.8 layer (RV sb3_eye: A 0 at x 6344)
# RV sb3_eye (rv2: TP 7893 495): SBBackEye2's A + 4 a frame from the teleport (the eye at x 6344 is in its 0.8 layer's view, not in the
# 1:1 view), SBBackEye's A stays 0 (only list Frames + 0x125b0 is faded); the shots match the port's (the eye and its glow cycle)
rows, sids, vox, oth, _ = run('sb3', 41, INV + ';5 TP 7893 495', 0, {'FP_TRACK': '1127:6344'})
e = [t for t in oth if t[0] == 'ob' and t[1] == '40']
def fld(t, k): return t[t.index(k) + 1]
glow = [t for t in e if fld(t, 'an') == max(fld(x, 'an') for x in e)] if e else []
base = [t for t in e if t not in glow]
check('sb3_eyes_on_the_0_8_layer', e and all(fld(t, 'lay') == '-1' for t in e), str([fld(t, 'lay') for t in e]))
check('sb3_eye_glow_fades_in_its_layer_view', glow and all(float(fld(t, 'a0')) > 100 for t in glow), str([fld(t, 'a0') for t in glow]))
check('sb3_eye_base_stays_opaque', base and all(float(fld(t, 'a0')) == 0 and float(fld(t, 'a2')) == 255 for t in base), str([(fld(t, 'a0'), fld(t, 'a2')) for t in base]))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
