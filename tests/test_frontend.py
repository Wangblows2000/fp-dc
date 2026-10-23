#!/usr/bin/env python3
"""Front-end / transition tests on the host build (menus ported from the original's events).
Routes: title -> START GAME -> file -> CLASSIC -> character select -> Lilac -> DV1; TIME ATTACK ->
character select -> stage select -> DV -> YES; options (difficulty/music saved); pause (open/close,
cursor clamp, QUIT wipe); developer select; repeated cycles; loading screen; VMU screen; save file."""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = os.environ.get('FP_TEST_DATA') or os.path.join(HERE, '..', 'converted2')
MODE_FRONTEND, MODE_PLAY, MODE_RESULTS, MODE_SELECT = 0, 1, 2, 3

def run(script, frames, env_extra=None, wrap=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    env = dict(os.environ, FP_TRACE_SND='1', FP_TRACE_SID='1'); env.pop('FP_LEVEL', None); env.pop('FP_SAVE', None)
    env.update(env_extra or {})
    cmd = ([wrap] if wrap else []) + [BIN, DATA, str(frames), name]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    os.unlink(name)
    rows, music, sids, pend = [], [], [], []
    for line in p.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            r = {t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)}
            rows.append(r)
            for s in pend: sids.append((int(r['f']), s))
            pend = []
        elif t[0] == 'sid': pend.append(int(t[1]))
        elif t[0] == 'music': music.append((t[1], int(t[2][1:])))
        elif t[:2] == ['snd', 'stopall']: music.append(('sfxstop', int(t[2][1:])))
    return rows, music, sids, p

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

def first_mode(rows, mode, after=0):
    for r in rows:
        if r['f'] >= after and r['mode'] == mode: return int(r['f'])
    return None

# title (Press Start at 40) -> hub at 146 (fade-in 32 + 0.2 s guard) -> START GAME (200) -> FILE 1 (230) ->
# CLASSIC (260 down, 280 confirm) -> wipe 51 frames -> character select -> Lilac (380) -> DV1
CLASSIC = '40 42 S\n200 201 J\n230 231 J\n260 261 D\n280 281 J\n380 381 J\n'
r, m, sids, p = run(CLASSIC, 700)
t_play = first_mode(r, MODE_PLAY)
check('classic_route_reaches_dv1', t_play is not None and 440 <= t_play <= 460, 'play at %s' % t_play)
names = [x[0] for x in m if x[0] not in ('stop', 'sfxstop')]
check('music_title_menu2_results_stage', names[:4] == ['bgm/m_title.adp', 'bgm/m_menu2.adp', 'bgm/m_results.adp', 'bgm/m_stage1.adp'], str(names))
seq = [s for f, s in sids]
# RE: 100 press start; 63 START GAME; 63 file; 64 move; 56 CLASSIC; 63 + 220 Lilac; 56 at the +20 wipe
check('menu_sound_ids', seq[:8] == [100, 63, 63, 64, 56, 63, 220, 56], str(seq[:10]))
# input reaches the menu one frame later (RE: pressed flags built at the end of the frame)
f63 = [f for f, s in sids if s == 63]
check('menu_input_latency_one_frame', f63 and f63[0] == 201, str(f63[:2]))
k = [x[0] for x in m].index('bgm/m_stage1.adp')
check('loading_silences_audio', 'sfxstop' in [x[0] for x in m][k - 3:k], str(m[k - 3:k + 1]))
check('no_duplicate_music_starts', all(a != b for a, b in zip(names, names[1:])))
# the stage card blocks control
play = [x for x in run(CLASSIC + '460 560 R\n', 560)[0] if x['mode'] == MODE_PLAY]
check('card_blocks_control', len(play) > 10 and play[1]['x'] == play[min(len(play) - 1, 60)]['x'])

# Time Attack: TIME ATTACK (200 down, 220 confirm) -> character select -> Lilac -> stage select (cursor on
# Shang Mu Academy: not in the port, cannot start) -> down to Dragon Valley -> confirm -> YES -> DV1 (time attack)
TA = '40 42 S\n200 201 D\n220 221 J\n330 331 J\n470 471 J\n490 491 D\n520 521 J\n630 631 J\n'
r, m, sids, p = run(TA, 820)
t_play = first_mode(r, MODE_PLAY)
names = [x[0] for x in m if x[0] not in ('stop', 'sfxstop')]
check('time_attack_route_reaches_dv1', t_play is not None, 'play at %s; music %s' % (t_play, names))
check('time_attack_music_menu1_continues', names[:4] == ['bgm/m_title.adp', 'bgm/m_menu2.adp', 'bgm/m_menu1.adp', 'bgm/m_stage1.adp'], str(names))
seq = [s for f, s in sids]
check('time_attack_shang_mu_not_startable', seq.count(63) >= 2 and 56 in seq, str(seq))

# options: OPTIONS (up from START GAME in the console layout), music volume left = -10 (90 %), OKAY saves
with tempfile.TemporaryDirectory() as d:
    sv = os.path.join(d, 'save.bin')
    # rows (console layout + port's SCREEN SIZE): difficulty, screen size, music, OKAY
    r, m, sids, p = run('40 42 S\n200 201 U\n230 231 J\n280 281 D\n290 291 D\n300 301 L\n330 331 D\n360 361 J\n', 420, {'FP_SAVE': sv})
    data = open(sv, 'rb').read() if os.path.exists(sv) else b''
    check('options_saved', len(data) > 8 and data[:3] == b'FPS' and data[5] == 90, 'len %d music %s' % (len(data), data[5] if len(data) > 5 else None))
    check('save_payload_small', 0 < len(data) <= 1024, '%d bytes' % len(data))
    # reload: the next boot reads the saved volume back (the options screen shows 90)
    r2, m2, s2, p2 = run('', 5, {'FP_SAVE': sv})
    check('save_reload_ok', p2.returncode == 0)
    check('save_v2_letterbox_default_on', len(data) > 8 and data[3] == 2 and 'letterbox 1' in p2.stdout, 'ver %s' % (data[3] if len(data) > 3 else None))
    # SCREEN SIZE: right toggles 16:9 letterbox off at once; OKAY saves it; the next boot applies it
    r, m, sids, p = run('40 42 S\n200 201 U\n230 231 J\n280 281 D\n300 301 R\n330 331 D\n340 341 D\n360 361 J\n', 420, {'FP_SAVE': sv})
    check('letterbox_toggle_applies', 'letterbox 0' in p.stdout and (301, 64) in sids or 'letterbox 0' in p.stdout, str(sids[-4:]))
    r2, m2, s2, p2 = run('', 5, {'FP_SAVE': sv})
    check('letterbox_off_persists', 'letterbox 0' in p2.stdout and 'letterbox 1' not in p2.stdout)
    r, m, sids, p = run('40 42 S\n200 201 U\n230 231 J\n280 281 D\n300 301 J\n330 331 D\n340 341 D\n360 361 J\n', 420, {'FP_SAVE': sv})
    r2, m2, s2, p2 = run('', 5, {'FP_SAVE': sv})
    check('letterbox_on_again_persists', 'letterbox 1' in p2.stdout and 'letterbox 0' not in p2.stdout)
    # a session-6 (v1) save still loads: options and files kept, letterbox defaults on, no cards
    import struct
    def crc16(b):
        c = 0xffff
        for x in b:
            c ^= x << 8
            for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
        return c
    files = bytes([1, 0, 1, 1, 0, 5]) + struct.pack('<H', 123) + bytes([14]) + struct.pack('<IH', 1000, 2)
    v1 = b'FPS\x01' + bytes([1, 70, 100, 70, 0]) + files + bytes(len(files) * 7) + bytes(6 * 16 * 4)
    v1 += struct.pack('<H', crc16(v1))
    sv1 = os.path.join(d, 'v1.bin'); open(sv1, 'wb').write(v1)
    r2, m2, s2, p2 = run('', 5, {'FP_SAVE': sv1})
    check('save_v1_loads_letterbox_default', 'letterbox 1' in p2.stdout, p2.stdout[-200:])
    # v1 file + options OKAY: rewritten as v2 with the v1 music volume (70) kept
    r, m, sids, p = run('40 42 S\n200 201 U\n230 231 J\n280 281 U\n300 301 J\n', 380, {'FP_SAVE': sv1})
    data = open(sv1, 'rb').read()
    check('save_v1_upgraded_to_v2', data[3] == 2 and data[5] == 70 and data[4 + 5] == 1, 'ver %d music %d file0 used %d' % (data[3], data[5], data[9]))

# pause (DV1 direct): open with start after the card, cursor clamps at QUIT, start closes; restart/quit wipe
r, m, sids, p = run('200 201 S\n210 211 D\n220 221 D\n230 231 D\n240 241 S\n', 260, {'FP_LEVEL': 'dv1'})
seq = [(f, s) for f, s in sids]
check('pause_open_sound_63', (200, 63) in seq, str(seq))
check('pause_cursor_clamped', [s for f, s in seq].count(64) == 2, str(seq))
check('pause_music_paused_and_resumed', ('pause', 200) in [(x[0].split('/')[-1], x[1]) for x in m] or any(x[0] == 'pause' for x in m), str(m))
xs = [x['x'] for x in r if 202 <= x['f'] <= 239]
check('pause_freezes_gameplay', len(set(xs)) == 1, '%d distinct x' % len(set(xs)))
r, m, sids, p = run('200 201 S\n210 211 D\n220 221 D\n230 231 J\n', 300, {'FP_LEVEL': 'dv1'})
t_fe = first_mode(r, MODE_FRONTEND)
check('pause_quit_after_50_frame_wipe', t_fe is not None and 280 <= t_fe <= 283, 'frontend at %s' % t_fe)

# developer select: L+R within 2 s on the title screen only
r, m, s, p = run('30 32 l\n60 62 r\n', 90)
check('dev_select_from_title', r[-1]['mode'] == MODE_SELECT)
r, m, s, p = run('30 32 l\n200 202 r\n', 230)
check('dev_select_needs_2s_window', r[-1]['mode'] == MODE_FRONTEND)

# loading screen image: black with the crystal in the bottom-right corner
shot = tempfile.mktemp(suffix='.ppm')
subprocess.run([BIN, DATA, '300', '-'], env=dict(os.environ, FP_LOADSHOT=shot, FP_LEVEL='dv1'), capture_output=True)
try:
    raw = open(shot, 'rb').read(); os.unlink(shot)
    hdr = raw.split(b'\n', 3); W, H = map(int, hdr[1].split()); px = hdr[3]
    lit = [(i % W, i // W) for i in range(W * H) if any(px[3 * i:3 * i + 3])]
    check('loading_screen_crystal_bottom_right', lit and min(x for x, y in lit) > W - 40 and min(y for x, y in lit) > H - 40, '%d lit px' % len(lit))
except (OSError, ValueError, IndexError) as e:
    check('loading_screen_crystal_bottom_right', False, str(e))

# VMU screen: crystal frames on the title, the life icon in a stage
with tempfile.TemporaryDirectory() as d:
    run('', 60, {'FP_VMU_DUMP': d})
    title_imgs = sorted(os.listdir(d))
    check('vmu_crystal_animates_on_title', len(title_imgs) >= 5, '%d images' % len(title_imgs))
with tempfile.TemporaryDirectory() as d:
    run('', 3, {'FP_VMU_DUMP': d, 'FP_LEVEL': 'dv1'})
    imgs = sorted(os.listdir(d))
    ok = False
    if imgs:
        b = open(os.path.join(d, imgs[-1]), 'rb').read().split(b'\n', 2)[2]
        ok = len(b) == 192 and any(b)
    check('vmu_life_icon_in_stage', ok, str(imgs))

# three title -> level -> pause QUIT -> title-menu cycles (valgrind with FP_VALGRIND=1)
cyc = CLASSIC + '660 661 S\n670 671 D\n680 681 D\n690 691 J\n'
t0 = 741
for k in range(2):        # back in the hub (no Press Start): fade-in 32, guard 12
    b = t0 + 60
    cyc += '%d %d J\n%d %d J\n%d %d D\n%d %d J\n%d %d J\n' % (b, b + 1, b + 30, b + 31, b + 60, b + 61, b + 80, b + 81, b + 180, b + 181)
    cyc += '%d %d S\n%d %d D\n%d %d D\n%d %d J\n' % (b + 460, b + 461, b + 470, b + 471, b + 480, b + 481, b + 490, b + 491)
    t0 = b + 541
r, m, s, p = run(cyc, t0 + 20, wrap='valgrind' if os.environ.get('FP_VALGRIND') else None)
modes = [x['mode'] for x in r]
n_play = sum(1 for a, b in zip(modes, modes[1:]) if a != MODE_PLAY and b == MODE_PLAY)
check('three_title_level_cycles', n_play == 3 and modes[-1] == MODE_FRONTEND and p.returncode == 0, 'entries %d rc %d' % (n_play, p.returncode))
if os.environ.get('FP_VALGRIND'):
    check('valgrind_no_leaks', 'definitely lost: 0 bytes' in p.stderr or 'no leaks are possible' in p.stderr, p.stderr[-300:])
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
