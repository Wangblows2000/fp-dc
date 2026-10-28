#!/usr/bin/env python3
"""Host tests: an extra life every 200 crystals (session 22, user request; objects_add_crystals in src/game/objects.c).

The HUD counter is the original's countdown (RV: 200 on a fresh file, - 1 per crystal); RE 2580: at <= 0 it is set back
to 200 and an extra life is created (RE 2602: lives + 1, the 1-up jingle m_1up, RE 1313). Rows print `cr` (the counter)
and `lv` (lives); FP_TRACE_SND prints 'life award' and the music starts. Host-only script commands: `<f> CRYS v` sets
the counter, `<f> ADDCRYS n` adds n crystals at once.
Covered: single pickups, a threshold crossing with a remainder, several thresholds in one frame, no repeated awards
from later frames, death / respawn (the act-start counter), act and stage transitions, the save file and Continue,
Time Attack, the 1-up monitor path.
usage: test_crystal_lives.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
MODE_FRONTEND, MODE_PLAY = 0, 1

def run(script, frames, env):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_TRACE_SND': '1'}); e.update(env)
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, events, cur = [], [], -1
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            d = {t[i]: t[i + 1] for i in range(0, len(t) - 1, 2)}
            rows.append({'f': int(d['f']), 'cr': int(d['cr']), 'lv': int(d['lv']), 'mode': int(d['mode']), 'x': float(d['x']), 'y': float(d['y']), 'act': int(d['act'])}); cur = int(d['f'])
        elif t[:2] == ['life', 'award']: events.append(('life', cur + 1))
        elif t[0] == 'music' and len(t) >= 3 and t[2].startswith('@'): events.append((t[1], int(t[2][1:])))
    loads = [l.split('[', 1)[1].split(']', 1)[0] for l in r.stderr.splitlines() + r.stdout.splitlines() if l.startswith('load[')]
    return rows, events, loads

def restarts(rows, after):
    # frames where the act was reloaded after frame `after`: the player is back at a start (x or y jumps by > 200 px)
    return [b['f'] for a, b in zip(rows, rows[1:]) if b['f'] > after and abs(b['x'] - a['x']) + abs(b['y'] - a['y']) > 200]

def at(rows, f):
    return next(r for r in rows if r['f'] == f)

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

lives_awarded = lambda ev: sum(1 for e in ev if e[0] == 'life')
jingles = lambda ev: [e[1] for e in ev if e[0] == 'bgm/m_1up.adp']

# ---- single pickups: DV1's three crystals at (725, 250), (757, 218), (789, 250) with the counter at 2
PICK3 = '5 CRYS 2\n200 TP 725 250\n230 TP 757 218\n260 TP 789 250\n'
rows, ev, _ = run(PICK3, 500, {'FP_LEVEL': 'dv1', 'FP_CHAR': '0'})
check('fresh_counter_200_lives_5', rows[0]['cr'] == 200 and rows[0]['lv'] == 5, str(rows[0]))
seq = []
for r in rows:
    if not seq or (r['cr'], r['lv']) != seq[-1]: seq.append((r['cr'], r['lv']))
check('single_pickups_count_down_and_award_at_zero', seq == [(200, 5), (2, 5), (1, 5), (200, 6), (199, 6)], str(seq))
check('single_pickup_one_award_one_jingle', lives_awarded(ev) == 1 and len(jingles(ev)) == 1, str(ev))
f_award = next(r['f'] for r in rows if r['lv'] == 6)
check('jingle_on_the_award_frame', jingles(ev) and jingles(ev)[0] == f_award, '%s vs %d' % (jingles(ev), f_award))
check('stage_music_back_after_jingle', any(e[0] == 'bgm/m_stage1.adp' and f_award + 238 <= e[1] <= f_award + 241 for e in ev), str(ev))
check('no_repeat_awards_in_later_frames', all(r['lv'] == 6 for r in rows if r['f'] >= f_award), str(rows[-1]))

# ---- several crystals at once: the remainder is kept (from 30, 80 crystals: one life, 150 = 50 toward the next)
rows, ev, _ = run('5 CRYS 30\n100 ADDCRYS 80\n', 200, {'FP_LEVEL': 'dv1'})
check('threshold_crossing_keeps_remainder', at(rows, 100)['cr'] == 150 and at(rows, 100)['lv'] == 6 and lives_awarded(ev) == 1, str(at(rows, 100)))
rows, ev, _ = run('5 CRYS 30\n100 ADDCRYS 30\n', 200, {'FP_LEVEL': 'dv1'})
check('exact_threshold_gives_200', at(rows, 100)['cr'] == 200 and at(rows, 100)['lv'] == 6, str(at(rows, 100)))
# the user's example: 250 crystals from a fresh counter = one life + 50 toward the next (counter 150)
rows, ev, _ = run('100 ADDCRYS 250\n', 200, {'FP_LEVEL': 'dv1'})
check('250_crystals_one_life_plus_50', at(rows, 100)['cr'] == 200 - 50 and at(rows, 100)['lv'] == 6, str(at(rows, 100)))

# ---- several thresholds in one frame: two lives, one jingle start, the remainder kept, nothing more later
rows, ev, _ = run('100 ADDCRYS 450\n', 400, {'FP_LEVEL': 'dv1'})
check('multiple_thresholds_one_frame', at(rows, 100)['cr'] == 150 and at(rows, 100)['lv'] == 7 and lives_awarded(ev) == 2, '%s %s' % (at(rows, 100), ev))
check('multiple_thresholds_one_jingle_start', jingles(ev) == [100], str(jingles(ev)))
check('multiple_thresholds_stable_after', all(r['lv'] == 7 and r['cr'] == 150 for r in rows if r['f'] >= 100))
# a later award restarts the jingle (Media::play on channel 31)
rows, ev, _ = run('100 ADDCRYS 200\n150 ADDCRYS 200\n', 300, {'FP_LEVEL': 'dv1'})
check('later_award_restarts_jingle', jingles(ev) == [100, 150] and at(rows, 150)['lv'] == 7, str(jingles(ev)))

# ---- death / respawn: the act restarts with one life less and the act-start counter (RE 4311 / 4312, loop_death):
# lives earned in the act are kept (as the original's set_lives(lives - 1)), the counter goes back to 200
rows, ev, loads = run(PICK3 + '300 HP 0\n300 HURT 10\n', 700, {'FP_LEVEL': 'dv1', 'FP_CHAR': '0'})
rs = restarts(rows, 300)
after = [r for r in rows if rs and r['f'] >= rs[0]]
check('death_reloads_act', loads == ['boot', 'dv1', 'dv1'] and len(rs) == 1, '%s %s' % (loads, rs))
check('death_restores_act_start_counter', after and after[0]['cr'] == 200 and after[0]['lv'] == 5, str(after[:1]))
check('death_no_extra_award', lives_awarded(ev) == 1, str(ev))

# ---- act transition (AT1 -> AT2, Milla): the counter and lives carry over; a death in AT2 restores AT2's start value
rows, ev, loads = run('5 CRYS 40\n6 ADDCRYS 243\n8 TP 12400 1170\n10 300 R\n500 HP 0\n500 HURT 10\n', 900, {'FP_LEVEL': 'at1', 'FP_CHAR': '4'})
check('act_change_at1_at2', loads[:4] == ['boot', 'at1', 'at2', 'at2'], str(loads))
at2 = [r for r in rows if r['act'] == 2 and r['f'] < 500]
check('act_change_carries_counter_and_lives', at2 and at2[0]['cr'] == 197 and at2[0]['lv'] == 7, str(at2[:1]))
rs = restarts([r for r in rows if r['act'] == 2], 500)
re_ = [r for r in rows if rs and r['f'] >= rs[0]]
check('death_after_act_change_restores_its_start_counter', re_ and re_[0]['cr'] == 197 and re_[0]['lv'] == 6, '%s %s' % (rs, re_[:1]))
check('act_change_no_extra_award', lives_awarded(ev) == 2, str([e for e in ev if e[0] == 'life']))

# ---- stage transition by the level loader (DV1 -> RM1): the counter is carried, nothing is awarded on load
rows, ev, _ = run('5 CRYS 17\n100 LOAD rm1\n', 200, {'FP_LEVEL': 'dv1'})
check('stage_load_carries_counter', at(rows, 150)['cr'] == 17 and at(rows, 150)['lv'] == 5 and lives_awarded(ev) == 0, str(at(rows, 150)))

# ---- the 1-up monitor shares the award path (lives + 1 and the jingle) and leaves the counter alone
# (covered by the gameplay tests for the monitor itself; here only the counter's independence)
rows, ev, _ = run('5 CRYS 3\n100 ADDCRYS 1\n', 150, {'FP_LEVEL': 'dv1'})
check('counter_independent_of_lives', at(rows, 100)['cr'] == 2 and at(rows, 100)['lv'] == 5)

# ---- the save file (story) keeps the counter and lives; Continue restores them, and a death after Continue goes back to
# the file's counter (the act-start values are the file's: session 22 fix)
with tempfile.TemporaryDirectory() as d:
    sv = os.path.join(d, 'save.bin')
    rows, ev, _ = run('5 CRYS 40\n6 ADDCRYS 243\n600 TP 16000 2280\n620 1300 R\n', 1100, {'FP_LEVEL': 'dv4', 'FP_FILE': '0', 'FP_FILE_NEW': '1', 'FP_SAVE': sv})
    data = open(sv, 'rb').read() if os.path.exists(sv) else b''
    import struct
    used, ch, mode, last, lives, shield, counter = struct.unpack_from('<6BH', data, 9) if len(data) > 17 else (0,) * 7
    check('save_file_counter_and_lives', used == 1 and counter == 197 and lives == 7, 'used %d counter %d lives %d' % (used, counter, lives))
    rows, ev, _ = run('40 42 S\n200 201 J\n230 231 J\n280 281 J\n', 700, {'FP_SAVE': sv})
    play = [r for r in rows if r['mode'] == MODE_PLAY]
    check('continue_restores_counter_and_lives', play and play[0]['cr'] == 197 and play[0]['lv'] == 7 and lives_awarded(ev) == 0, str(play[:1]))
    f0 = play[0]['f'] if play else 0
    rows, ev, loads = run('40 42 S\n200 201 J\n230 231 J\n280 281 J\n%d CRYS 1\n%d HP 0\n%d HURT 10\n' % (f0 + 120, f0 + 130, f0 + 130), f0 + 500, {'FP_SAVE': sv})
    rs = restarts(rows, f0 + 130)
    re_ = [r for r in rows if rs and r['f'] >= rs[0]]
    check('death_after_continue_restores_file_counter', re_ and re_[0]['cr'] == 197 and re_[0]['lv'] == 6, '%s %s %s' % (loads, rs, re_[:1]))

# ---- Time Attack: the same rule (the counter is never written to a file)
rows, ev, _ = run('100 ADDCRYS 201\n', 200, {'FP_LEVEL': 'dv1', 'FP_TA': '1'})
check('time_attack_same_rule', at(rows, 100)['cr'] == 199 and at(rows, 100)['lv'] == 6, str(at(rows, 100)))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
