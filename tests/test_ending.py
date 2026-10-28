#!/usr/bin/env python3
"""Host tests for the end of the game (session 24, src/game/ending.c, cutscene.c): Final Dreadnought round 4's results ->
the achievement pop-up -> the space ending (frame index 83) -> the credits and the statistics (frame index 85) ->
"Thanks for playing!" / CONTINUE -> the title menu (frame index 3), for Lilac, Carol and Milla; the final score from the
run's file; skip / input rules; no ending from Time Attack, another stage or a second call; the cutscene template; the
game-clear achievements once; the save's progress. Needs the ending data (tools/import_ending.sh <data_dir>).
usage: test_ending.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS, ACH = 15, 8, 4, 56

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACE_SND', 'FP_FE_TRACE')]; e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    return r.stdout + r.stderr

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c

def make_save(path, files):
    """a v4 save (save.c io3): files = {slot: (character, mode, last_stage, {s: (time_ms, crystals, deaths)})}"""
    d = bytearray(b'FPS\x04') + bytes([1, 100, 100, 70, 1])
    for i in range(FILES):
        ch, mode, last, blocks = files.get(i, (0, 0, 0xff, {}))
        d += bytes([1 if i in files else 0, ch, mode, last, 5, 0]) + struct.pack('<H', 0)
        for s in range(STAGES): d += struct.pack('<IHB', *blocks.get(s, (0, 0, 0)))
    d += bytes(6 * STAGES * CHARS) + bytes(CHARS) + bytes(13) + bytes(13) + bytes(ACH)
    d += struct.pack('<H', crc16(bytes(d)))
    open(path, 'wb').write(d)

def parse(path):
    d = open(path, 'rb').read(); p = 4 + 5; files = []
    for i in range(FILES):
        used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', d, p); p += 8
        st = [struct.unpack_from('<IHB', d, p + 7 * s) for s in range(STAGES)]; p += 7 * STAGES
        files.append(dict(used=used, ch=ch, last=last, stage=st))
    p += 6 * STAGES * CHARS
    prog = list(d[p:p + CHARS]); p += CHARS + 26
    return dict(files=files, progress=prog, ach=list(d[p:p + ACH]))

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

def ends(out): return [l for l in out.splitlines() if l.startswith('end st')]
def states(out):
    seq = []
    for l in ends(out):
        s = int(l.split()[2])
        if not seq or seq[-1][0] != s: seq.append([s, 1])
        else: seq[-1][1] += 1
    return seq
CONFIRM = ''.join('%d %d J\n' % (f, f + 2) for f in range(760, 1000, 40))   # the achievement pop-up pages (A after 30 frames)
# FD4's end through the shared results (path 3), as the Final Dreadnought module will end its last round, then the pop-up
FD4_END = '30 RESBOSS\n700 RESEND\n' + CONFIRM
CHR = {0: 'lilac', 2: 'carol', 4: 'milla'}
SK = {0: 1, 2: 2, 4: 3}

# all frames, the whole ending, per character: fast-forward the credits (A held), then CONTINUE
def whole(ch, tmp):
    sv = os.path.join(tmp, 'w%d.sav' % ch)
    make_save(sv, {1: (ch, 1, 12, {1: (300000, 120, 2), 5: (420000, 300, 0), 12: (250000, 80, 1)})})
    script = FD4_END + '2300 3200 J\n3800 3802 J\n'
    out = run(script, 4000, {'FP_LEVEL': 'fd4b', 'FP_CHAR': str(ch), 'FP_FILE': '1', 'FP_SAVE': sv, 'FP_TRACE_SND': '1', 'FP_FE_TRACE': '1'})
    return out, sv

with tempfile.TemporaryDirectory() as tmp:
    for ch in (0, 2, 4):
        n = CHR[ch]
        out, sv = whole(ch, tmp)
        seq = states(out)
        order = [s for s, _ in seq]
        check(n + '_sequence', order == [1, 2, 3, 4, 5], str(seq))
        st1 = dict((s, k) for s, k in seq)
        check(n + '_space_1110_frames', st1.get(1, 0) in (1110, 1111), 'RE 22441: B > 1110 (%d)' % st1.get(1, 0))
        check(n + '_hold_60', st1.get(2, 0) in (60, 61), str(st1.get(2)))
        m = re.search(r'ending begin chr (\d+) file (\d+) score time (\d+) crystals (\d+) deaths (\d+) shown (\w+)', out)
        check(n + '_score_from_file', m is not None and int(m.group(1)) == ch and int(m.group(3)) == 300000 + 420000 + 250000 + int(re.search(r'ach results stage 13 ms (\d+)', out).group(1))
              and int(m.group(4)) == 500 and int(m.group(5)) == 3 and int(m.group(6), 16) == (1 << 1 | 1 << 5 | 1 << 12 | 1 << 13), m.group(0) if m else 'none')
        check(n + '_credits_music', 'music bgm/m_credits.adp' in out)
        last = [l for l in out.splitlines() if l.startswith('fe ')]
        check(n + '_to_title_menu', last and last[-1].split()[3:6] == ['3', 'cur', '0'] and 'mode 0' in [l for l in out.splitlines() if l.startswith('f 3999')][0], last[-1] if last else '')
        check(n + '_one_begin', out.count('ending begin') == 1)
        s = parse(sv)
        f = s['files'][1]
        check(n + '_save_block13', f['last'] == 13 and f['stage'][13][0] > 0 and s['progress'][[0, 0, 1, 1, 2][ch]] == 14, 'last %d prog %s' % (f['last'], s['progress']))
        check(n + '_ach_clear', s['ach'][SK[ch] - 1] == 2 and all(s['ach'][k - 1] == 0 for k in (1, 2, 3) if k != SK[ch]), str(s['ach'][:6]))
        check(n + '_ach_no_death_clear_not_earned', s['ach'][3] == 0, 'the file has deaths: 4 stays locked (%d)' % s['ach'][3])
        # the end screen: the statistics stop at 220 before A reaches 6000 when the list is short; CONTINUE after 180 frames
        pins = [l for l in ends(out) if ' pin 1 ' in l]
        cont = [int(l.split()[10]) for l in pins]
        check(n + '_continue_after_180', cont and max(cont) == 180 and ' st 5 ' not in ' '.join(pins[:179]), str(len(pins)))
        if ch == 0:
            ens = ends(out)
            a3 = [float(l.split()[6]) for l in ens if l.split()[2] == '3']
            # skip rule (RE 22726 / 22728): + 1 every 2nd frame, + 10 a frame while a button is held (frames 2300-3200)
            check('credits_scroll_half_px', a3[1] - a3[0] in (0, 1) and abs((a3[40] - a3[0]) - 20) <= 1, '%s' % (a3[:3],))
            check('credits_fast_forward', any(b - a >= 10 for a, b in zip(a3, a3[1:])), 'held J')
            check('credits_amax_6000', max(float(l.split()[6]) for l in ens) == 6000)

    # the space scene ignores input (RE: no input test in frame 83's events); presses before CONTINUE do nothing
    sv = os.path.join(tmp, 'k.sav'); make_save(sv, {1: (0, 1, 12, {})})
    out = run('10 ENDING\n20 1100 JS\n', 1200, {'FP_LEVEL': 'dv1', 'FP_FILE': '1', 'FP_SAVE': sv})
    seq = states(out)
    check('space_no_skip', seq and seq[0][0] == 1 and seq[0][1] >= 1110, str(seq[:2]))

    # final score from real data: a pause RESTART in FD4 counts a death (RE loop_restart_20 / 1276) and the crystals of the act
    # go back; the FD4 block holds it; the statistics list each stage with a time and the totals
    sv = os.path.join(tmp, 'r.sav'); make_save(sv, {2: (0, 1, 12, {3: (61000, 7, 4), 12: (90010, 11, 0)})})
    script = '100 101 S\n112 113 D\n120 121 J\n' + FD4_END.replace('30 RESBOSS', '300 RESBOSS').replace('700 RESEND', '980 RESEND').replace(CONFIRM, ''.join('%d %d J\n' % (f, f + 2) for f in range(1040, 1300, 40))) + '2600 ENDSTATS\n'
    out = run(script, 2700, {'FP_LEVEL': 'fd4b', 'FP_CHAR': '0', 'FP_FILE': '2', 'FP_SAVE': sv})
    st = [l[6:] for l in out.splitlines() if l.startswith('stats|')]
    fd4 = parse(sv)['files'][2]['stage'][13]
    check('restart_counts_death', fd4[2] == 1, 'FD4 block %s' % (fd4,))
    tot = 61000 + 90010 + fd4[0]
    ttxt = '%s%d:%02d:%02d' % ('0' if tot < 600000 else '', tot // 60000, tot // 1000 % 60, tot // 10 % 100)
    check('stats_lines', st[:2] == ['                GAME STATS', ''] and st.count('            Deaths     4') == 1 and 'FORTUNE NIGHT' in st[2]
          and '              Time     01:01:00' in st and '          Crystals     11' in st and any('FINAL DREADNOUGHT 4' in l for l in st), str(st[:8]))
    check('stats_final_score', st[-12:] == ['', '', '                FINAL SCORE', '', '     TIME', '     ' + ttxt, '', '     CRYSTALS', '     %d' % (7 + 11 + fd4[1]), '', '     DEATHS', '     5'], str(st[-12:]))
    check('stats_line_count', len(st) == 14 + 5 * 3, 'RE set_height (5 A + 14) x 20: %d' % len(st))

    # no ending from Time Attack (RE frame 8: G0 0 -> frame 5), from another stage / act, or twice
    sv = os.path.join(tmp, 't.sav'); make_save(sv, {})
    out = run(FD4_END, 1300, {'FP_LEVEL': 'fd4b', 'FP_TA': '1', 'FP_SAVE': sv, 'FP_FE_TRACE': '1'})
    fe = [l for l in out.splitlines() if l.startswith('fe ') and ' idx 5 ' in l]
    check('ta_no_ending', 'ending begin' not in out and fe, 'Time Attack -> stage select')
    out = run('30 ENDTRY\n', 60, {'FP_LEVEL': 'dv4'})
    check('other_stage_refused', 'ending try -1' in out)
    out = run('30 ENDTRY\n', 60, {'FP_LEVEL': 'fd4a'})
    check('round4_first_act_refused', 'ending try -1' in out)
    out = run('30 ENDTRY\n', 60, {'FP_LEVEL': 'fd4b', 'FP_TA': '1'})
    check('ta_call_refused', 'ending try -1' in out)
    out = run('30 ENDTRY\n40 ENDTRY\n', 60, {'FP_LEVEL': 'fd4b'})
    check('script_call_and_no_retrigger', 'ending try 0' in out and 'ending try -1' in out and out.count('ending begin') == 1)
    check('script_call_saves_block', 'ach earn 1' in out)   # the game-clear achievements run once from a direct call too

    # the game-clear achievements are earned once: a second clear on the same save earns nothing new
    sv = os.path.join(tmp, 'a.sav'); make_save(sv, {1: (2, 1, 12, {})})
    out1 = run(FD4_END, 1100, {'FP_LEVEL': 'fd4b', 'FP_CHAR': '2', 'FP_FILE': '1', 'FP_SAVE': sv})
    out2 = run(FD4_END, 1100, {'FP_LEVEL': 'fd4b', 'FP_CHAR': '2', 'FP_FILE': '1', 'FP_SAVE': sv})
    check('ach_once', 'ach earn 2' in out1 and 'ach earn 2' not in out2 and 'ach earn 56' not in out2 and 'gal popup' not in out2, '')
    check('second_clear_goes_straight_to_ending', 'ending begin' in out2)

    # the cutscene template: not in the game flow, plays only through the host command; A advances, Start skips
    src = open(os.path.join(HERE, '..', 'src', 'game', 'ending.c')).read()
    users = [f for f in os.listdir(os.path.join(HERE, '..', 'src', 'game')) if f.endswith(('.c', '.inc')) and 'CS_EXAMPLE' in open(os.path.join(HERE, '..', 'src', 'game', f)).read()]
    check('template_not_wired', sorted(users) == ['cutscene_example.c', 'ending.c'] and src.index('CS_EXAMPLE') > src.index('#ifdef FP_HOST_ENV\n/* host test command "<f> ENDING"'), str(users))
    out = run('10 CUTSCENE\n' + ''.join('%d %d J\n' % (f, f + 2) for f in range(150, 300, 30)), 700, {'FP_LEVEL': 'dv1'})
    cues = [int(l.split()[2]) for l in out.splitlines() if l.startswith('cs cue')]
    check('template_plays_all_cues', cues == list(range(11)) and 'mode 0' in [l for l in out.splitlines() if l.startswith('f 699')][0], str(cues))
    out = run('10 CUTSCENE\n100 120 \n130 132 S\n', 300, {'FP_LEVEL': 'dv1'})
    check('template_start_skips', 'cs skip' in out and 'mode 0' in [l for l in out.splitlines() if l.startswith('f 299')][0])
    out = run('10 CUTSCENE\n', 600, {'FP_LEVEL': 'dv1'})
    cues = [int(l.split()[2]) for l in out.splitlines() if l.startswith('cs cue')]
    check('template_waits_for_A', cues == [0, 1, 2, 3], str(cues))

print('%d failures' % fails)
sys.exit(1 if fails else 0)
