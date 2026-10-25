#!/usr/bin/env python3
"""Host tests for the end of Relic Maze act 5 (src/game/rm_mantalith.c, build/re/spec_rm_mantalith.md): per character
(Lilac, Carol, Carol on the bike, Milla) leaving the labyrinth (6861) into the Kingdom Stone scene (0x22511), the Mantalith
intro and fight, its defeat through the host cheat (BOSSHP 0 five times: arms and head), the results started by the boss
(7083, results.c path 3), the story save written at D = 150 (5394: Relic Maze = stage index 2), the result voice
(6589-6592) and the post-results scene ending the stage (7097 -> frame 8: no stage after RM is ported, so the front end).
usage: test_rm_mantalith.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS = 15, 8, 4

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW')]; e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, mt = [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if t and t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        if t and t[0] == 'mt': mt.append(dict(zip(['f'] + t[2::2], [int(t[1])] + t[3::2])))
    return rows, mt, r.stdout + r.stderr

def parse_save(path):                       # save v3 (tests/test_saves.py): files and progress
    d = open(path, 'rb').read(); p = 4 + 5; files = []
    for i in range(FILES):
        used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', d, p); p += 8
        st = []
        for s in range(STAGES): st.append(struct.unpack_from('<IHB', d, p)); p += 7
        files.append(dict(used=used, ch=ch, last=last, stage=st))
    p += 6 * STAGES * CHARS
    return files, list(d[p:p + CHARS])

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# TP just before the labyrinth exit (X > 2707), run right into the scene; then the cheat beats one phase every 400 frames
SCRIPT = '30 TP 2690 500\n30 SETV others E 99999\n40 520 R\n' + ''.join('%d BOSSHP 0\n' % f for f in (1000, 1400, 1800, 2200, 2600))
VOICE = {'0': 'lilac_woohoo', '2': 'carol_feelmypower', '3': 'carol_feelmypower', '4': 'milla_yay'}
with tempfile.TemporaryDirectory() as d:
    for ch, nm in (('0', 'lilac'), ('2', 'carol'), ('3', 'carolbike'), ('4', 'milla')):
        sv = os.path.join(d, 'save_%s.bin' % ch)
        rows, mt, out = run(SCRIPT, 4800, {'FP_LEVEL': 'rm5', 'FP_CHAR': ch, 'FP_DBG_MT': '1', 'FP_TRACE_SID': '1',
                                           'FP_FILE': '2', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        grp = lambda m, k: m['grp'][k] == '1'
        scene = [m for m in mt if grp(m, 1)]
        check('%s_kingdom_stone_scene' % nm, scene and max(int(m['sD']) for m in scene) == 4, str(len(scene)))
        check('%s_reaches_the_fight' % nm, any(grp(m, 3) and m['L'] == '62' for m in mt))
        sids = [l.split()[1] for l in out.splitlines() if l.startswith('sid ')]
        check('%s_roar_and_landing_sounds' % nm, '148' in sids and '132' in sids and '146' in sids)
        dead = [m for m in mt if float(m['hG']) >= 5]
        check('%s_boss_defeated' % nm, dead and max(float(m['hI']) for m in dead) >= 330, str(len(dead)))
        res = [r['f'] for r in rows if r.get('res', 0) >= 1]
        check('%s_results_started' % nm, bool(res), str(res[:1]))
        check('%s_result_voice' % nm, ('voice ' + VOICE[ch]) in out)
        files, prog = parse_save(sv)
        f = files[2]
        check('%s_story_save_stage2' % nm, f['used'] == 1 and f['last'] == 2 and 10000 < f['stage'][2][0] < 120000 and prog[{'0': 0, '2': 1, '3': 1, '4': 2}[ch]] == 3,
              'time %d ms last %d progress %s' % (f['stage'][2][0], f['last'], prog))
        post = [m for m in mt if grp(m, 5)]
        check('%s_post_results_scene' % nm, post and max(int(float(m['hJ'])) for m in post) >= 260, str(len(post)))
        check('%s_stage_ended' % nm, rows and rows[-1]['mode'] == 0 and 'load[frontend]' in out, 'mode %s' % (rows[-1]['mode'] if rows else None))
        if ch == '3': check('carolbike_leaves_the_bike', rows and rows[-200]['chr'] == 2 and rows[100]['chr'] == 3)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
