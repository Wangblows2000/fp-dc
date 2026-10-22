#!/usr/bin/env python3
"""Front-end / transition tests on the host build (menus built from the original frames).
Checks: menu route to gameplay, music changes (no duplicate starts), developer level select only from
the title (L+R within 2 s), pause -> QUIT -> title, repeated title -> level -> title cycles."""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = os.path.join(HERE, '..', 'converted2')
MODE_FRONTEND, MODE_PLAY, MODE_RESULTS, MODE_SELECT = 0, 1, 2, 3

def run(script, frames, wrap=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    env = dict(os.environ, FP_TRACE_SND='1'); env.pop('FP_LEVEL', None)
    cmd = ([wrap] if wrap else []) + [BIN, DATA, str(frames), name]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    os.unlink(name)
    rows, music = [], []
    for line in p.stdout.splitlines():
        t = line.split()
        if t and t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        if t and t[0] == 'music': music.append((t[1], int(t[2][1:])))
    return rows, music, p

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# title -> START GAME -> file (START) -> character select -> Lilac -> stage card -> gameplay
ROUTE = '40 42 S\n60 62 J\n100 102 S\n140 142 J\n'
r, m, p = run(ROUTE, 400)
check('menu_route_reaches_gameplay', r[-1]['mode'] == MODE_PLAY, 'mode %d' % r[-1]['mode'])
names = [x[0] for x in m]
check('music_title_menu_stage', names[:3] == ['bgm/m_title.adp', 'bgm/m_menu1.adp', 'bgm/m_stage1.adp'], str(names))
check('no_duplicate_music_starts', all(a != b for a, b in zip(names, names[1:])))
# player has no control during the stage card (timer not running): x unchanged while R is held
r, m, p = run(ROUTE + '200 300 R\n', 300)
play = [x for x in r if x['mode'] == MODE_PLAY]
check('card_blocks_control', len(play) > 10 and play[0]['x'] == play[min(len(play) - 1, 60)]['x'])
# developer select: L+R within 2 s on the title screen
r, m, p = run('30 32 l\n60 62 r\n', 90)
check('dev_select_from_title', r[-1]['mode'] == MODE_SELECT)
r, m, p = run('30 32 l\n200 202 r\n', 230)
check('dev_select_needs_2s_window', r[-1]['mode'] == MODE_FRONTEND)
r, m, p = run(ROUTE + '420 422 l\n430 432 r\n', 450)
check('dev_select_not_in_gameplay', r[-1]['mode'] == MODE_PLAY and r[-1]['sel'] == 0)
# pause -> QUIT -> title; three title -> level -> title cycles
cyc = ''
t0 = 0
for k in range(3):
    cyc += '%d %d S\n%d %d J\n%d %d S\n%d %d J\n' % (t0 + 40, t0 + 42, t0 + 60, t0 + 62, t0 + 100, t0 + 102, t0 + 140, t0 + 142)
    cyc += '%d %d S\n%d %d D\n%d %d D\n%d %d J\n' % (t0 + 450, t0 + 452, t0 + 460, t0 + 462, t0 + 470, t0 + 472, t0 + 480, t0 + 482)
    t0 += 520
r, m, p = run(cyc, t0 + 20, wrap='valgrind' if os.environ.get('FP_VALGRIND') else None)
modes = [x['mode'] for x in r]
n_play = sum(1 for a, b in zip(modes, modes[1:]) if a != MODE_PLAY and b == MODE_PLAY)
check('three_title_level_cycles', n_play == 3 and modes[-1] == MODE_FRONTEND and p.returncode == 0, 'entries %d rc %d' % (n_play, p.returncode))
names = [x[0] for x in m]
check('music_restarts_per_cycle', names.count('bgm/m_stage1.adp') == 3 and names.count('bgm/m_title.adp') >= 3, str(names))
if os.environ.get('FP_VALGRIND'):
    check('valgrind_no_leaks', 'definitely lost: 0 bytes' in p.stderr or 'no leaks are possible' in p.stderr, p.stderr[-300:])
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
