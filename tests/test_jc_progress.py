#!/usr/bin/env python3
"""Host test (session 21): Jade Creek's end of stage for Lilac, Carol and Milla. The JC5 gunship's defeat (the act-5 module's
fight script, as test_jc_act5.py) -> results -> the save: story file block of stage 5 (Jade Creek), last stage 5, progress 6,
the record rec[5][char]; Time Attack writes only the record; then the wipe leaves the stage (results path, `.res_wipe`).
Also: a JC act end (JC1's right edge) does not touch the save (only the stage end does, like every other stage).
usage: test_jc_progress.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS, CARDBYTES, ACH = 15, 8, 4, 13, 56
JC = 5                                                   # stage index of Jade Creek
SAVE_CH = {0: 0, 2: 1, 4: 2}                            # FP_CHAR -> save character slot (Lilac, Carol, Milla)

def run(level, char, frames, script, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(char)}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    return r.stdout

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c

def parse(path):
    if not os.path.exists(path): return None
    d = open(path, 'rb').read()
    if d[:3] != b'FPS' or crc16(d[:-2]) != struct.unpack('<H', d[-2:])[0]: return None
    p = 4 + 5; sv = {'files': []}
    for i in range(FILES):
        used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', d, p); p += 8
        st = []
        for s in range(STAGES): st.append(struct.unpack_from('<IHB', d, p)); p += 7
        sv['files'].append(dict(used=used, ch=ch, last=last, stage=st))
    sv['rec'] = [[struct.unpack_from('<IH', d, p + 6 * (s * CHARS + c)) for c in range(CHARS)] for s in range(STAGES)]; p += 6 * STAGES * CHARS
    sv['progress'] = list(d[p:p + CHARS])
    return sv

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# the gunship fight of test_jc_act5.py (Serpentine's hitbox at 1, attacks, the gunship's hitbox at 1 from 1100)
INV = ''.join('%d SETV others E 99999\n' % f for f in range(3, 6000, 300))
def fight(ch, end_attacks):
    L = ['60 TP 13300 560', '70 140 R', '280 BOSSHPT 822 1', '1100 BOSSHPT 1485 1']
    if ch == 4:
        for k, f in enumerate(range(300, end_attacks, 130)): L += ['%d %d %s' % (f, f + 20, 'LR'[k & 1]), '%d %d X' % (f + 25, f + 90), '%d %d X' % (f + 95, f + 97)]
    else:
        for f in range(300, end_attacks, 36): L += ['%d %d J' % (f, f + 8), '%d %d X' % (f + 12, f + 14), '%d %d X' % (f + 22, f + 24)]
        for k, f in enumerate(range(300, end_attacks, 100)): L.append('%d %d %s' % (f, f + 100, 'LR'[k & 1]))
    if ch == 4: L.append('1600 BOSSHPT 1485 0')   # (Milla's cube rarely reaches the gunship in this script: the hitbox is set to 0)
    return INV + '\n'.join(L) + '\n'

with tempfile.TemporaryDirectory() as d:
    for ch, cn in ((0, 'lilac'), (2, 'carol'), (4, 'milla')):
        c = SAVE_CH[ch]
        # ---- story: file 3, new game started in Jade Creek 5
        sv = os.path.join(d, 'story_%s.bin' % cn)
        out = run('jc5', ch, 4200, fight(ch, 2400), {'FP_FILE': '3', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
        s = parse(sv)
        f = s and s['files'][3]
        check('%s_story_save_written' % cn, s is not None and any(l.startswith('save ') for l in out.splitlines()))
        check('%s_story_stage5_block' % cn, f and f['used'] == 1 and f['last'] == JC and f['stage'][JC][0] > 10000 and
              all(f['stage'][x][0] == 0 for x in range(STAGES) if x != JC), str(f and (f['last'], f['stage'][JC])))
        check('%s_story_progress_6' % cn, s and s['progress'][c] == JC + 1, str(s and s['progress']))
        check('%s_story_record_stage5' % cn, s and s['rec'][JC][c][0] == f['stage'][JC][0] and
              all(s['rec'][x][y][0] == 0 for x in range(STAGES) for y in range(CHARS) if (x, y) != (JC, c)), str(s and s['rec'][JC]))
        rows = [l.split() for l in out.splitlines() if l.startswith('f ')]
        modes = [int(r[r.index('mode') + 1]) for r in rows if 'mode' in r]
        if ch == 0:
            check('%s_leaves_stage_after_results' % cn, modes and modes[-1] != modes[len(modes) // 2], 'mode %s -> %s' % (modes[len(modes) // 2] if modes else None, modes[-1] if modes else None))
        else:   # session 21b: Classic routes Carol / Milla on to Trap Hideout (RE handle_frame_9: I 5, g1 != 0 -> frame 56)
            acts = [int(r[r.index('act') + 1]) for r in rows if 'act' in r]
            check('%s_leaves_stage_after_results' % cn, acts and acts[len(acts) // 2] == 5 and acts[-1] == 1 and modes[-1] == 1, 'act %s -> %s' % (acts[len(acts) // 2] if acts else None, acts[-1] if acts else None))
        # ---- Time Attack: the record only, no file
        st = os.path.join(d, 'ta_%s.bin' % cn)
        run('jc5', ch, 4200, fight(ch, 2400), {'FP_TA': '1', 'FP_SAVE': st})
        s = parse(st)
        check('%s_ta_record_stage5' % cn, s and s['rec'][JC][c][0] > 10000 and not any(x['used'] for x in s['files']), str(s and s['rec'][JC][c]))
    # ---- an act end inside the stage (JC1's right edge -> JC2) writes nothing
    sa = os.path.join(d, 'act.bin')
    out = run('jc1', 0, 900, '300 TP 13300 700\n310 500 R\n', {'FP_FILE': '1', 'FP_FILE_NEW': '1', 'FP_SAVE': sa})
    s = parse(sa)
    lv = [l.split()[l.split().index('lv') + 1] for l in out.splitlines() if l.startswith('f ') and ' lv ' in l]
    check('jc1_act_end_no_stage_save', s is None or not any(x['stage'][JC][0] for x in s['files']), str(s and s['files'][1]['stage'][JC]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
