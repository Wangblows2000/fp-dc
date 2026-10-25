#!/usr/bin/env python3
"""Host tests for the gallery (original frame index 10, build/re/fe/spec_gallery.md; src/game/fe_gallery.inc) and the
save v4 fields it uses (opened cards, achievements). Route: Press Start -> title menu -> GALLERY (two Downs in the
console layout) -> gallery at frame 291 (hub wipe 51 frames), input after the tile fade-in (~35 frames).
The gallery prints 'gal ...' trace lines on the host (cards / dirs / ach / rows dumps on entry, card writes, art
load / free, music / sound plays, DATA marks, deletes, saves).
usage: test_gallery.py [data_dir]   (default: converted2)"""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS, ACH = 15, 8, 4, 56
PRE = '40 42 S\n200 201 D\n220 221 D\n240 241 J\n'   # GALLERY row (console layout: START, TIME ATTACK, GALLERY, OPTIONS)
CHEAT = '20 21 L\n30 31 L\n40 41 L\n50 51 R\n60 61 R\n70 71 R\n100 102 S\n260 261 D\n280 281 D\n300 301 J\n'

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x << 8
        for _ in range(8): c = ((c << 1) ^ 0x1021) & 0xffff if c & 0x8000 else (c << 1) & 0xffff
    return c

def bits(ids):
    b = bytearray(13)
    for n in ids: b[(n - 1) >> 3] |= 1 << ((n - 1) & 7)
    return bytes(b)

def make_save(cards=(), opened=(), ach=None, progress=(0, 0, 0, 0), rec=None, ver=4):
    """a save in the port's layout (save.c): v4, or v3 without the gallery fields"""
    rec = rec or {}
    b = b'FPS' + bytes([ver]) + bytes([1, 100, 100, 70, 1]) + bytes(FILES * (8 + STAGES * 7))
    b += b''.join(struct.pack('<IH', *rec.get((s, c), (0, 0))) for s in range(STAGES) for c in range(CHARS))
    b += bytes(progress) + bits(cards)
    if ver >= 4:
        a = [0] * ACH
        for k, v in (ach or {}).items(): a[k - 1] = v
        b += bits(opened) + bytes(a)
    return b + struct.pack('<H', crc16(b))

def parse(d):
    assert d[:4] == b'FPS\x04' and crc16(d[:-2]) == struct.unpack('<H', d[-2:])[0] and len(d) == 1361, 'bad v4 save'
    p = 4 + 5 + FILES * (8 + STAGES * 7)
    rec = {(s, c): struct.unpack_from('<IH', d, p + 6 * (s * CHARS + c)) for s in range(STAGES) for c in range(CHARS)}
    p += 6 * STAGES * CHARS + 4
    cb, ob, a = d[p:p + 13], d[p + 13:p + 26], list(d[p + 26:p + 26 + ACH])
    ids = lambda b: {n for n in range(1, 101) if b[(n - 1) >> 3] >> ((n - 1) & 7) & 1}
    return dict(rec=rec, cards=ids(cb), opened=ids(ob), ach=a)

def run(script, frames, save=None, env=None):
    """-> (events [(frame, text)], final save bytes or None, raw output). Lines printed during a frame's update come
    before that frame's 'f' row, so each event gets the frame of the next row."""
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, 'script.txt'); open(sp, 'w').write(script)
        sv = os.path.join(d, 'save.bin')
        if save is not None: open(sv, 'wb').write(save)
        e = dict(os.environ, FP_TRACE_SND='1', FP_TRACE_SID='1', FP_FE_TRACE='1', FP_SAVE=sv)
        for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW'): e.pop(k, None)
        e.update(env or {})
        r = subprocess.run([BIN, DATA, str(frames), sp], capture_output=True, text=True, env=e)
        out = r.stdout + r.stderr
        ev, pend = [], []
        for line in r.stdout.splitlines():
            t = line.split()
            if not t: continue
            if t[0] == 'f': ev += [(int(t[1]), x) for x in pend]; pend = []
            elif t[0] in ('gal', 'sid', 'music', 'save') or line.startswith('fe '): pend.append(line)
        ev += [(frames, x) for x in pend]
        data = open(sv, 'rb').read() if os.path.exists(sv) else None
        return ev, data, out

def keys(seq, start=340, step=20):
    """'D J W100 X' -> script lines from frame `start`, one key every `step` frames (W<n>: wait n more frames)"""
    s, f = '', start
    for k in seq.split():
        if k[0] == 'W': f += int(k[1:]); continue
        s += '%d %d %s\n' % (f, f + 1, k); f += step
    return s, f

def gal(ev, prefix):
    return [(f, x) for f, x in ev if x.startswith('gal ' + prefix)]
def dump(ev, what):                  # the entry dumps: 'gal cards|dirs|ach|rows <digits>'
    for f, x in ev:
        t = x.split()
        if len(t) == 3 and t[:2] == ['gal', what] and len(t[2]) >= 10: return t[2]
    return None
def saves(xs):                       # platform save writes ('save <bytes> @<frame>'; not the boot line 'save cards n')
    return [x for x in xs if x.startswith('save ') and x.split()[1].isdigit()]

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- GALLERY row (RE 390) -> hub wipe with sound 56 -> frame index 10 with m_results (RE 1136, RV t_gal5)
ev, data, out = run(PRE, 340)
fe = [(f, int(x.split()[3])) for f, x in ev if x.startswith('fe ')]
t10 = next((f for f, i in fe if i == 10), None)
check('gallery_row_reaches_screen', t10 is not None and 285 <= t10 <= 295, 'frame %s' % t10)
check('gallery_entry_sound_56_wipe', (241, 'sid 56') in ev, str([x for x in ev if 'sid' in x[1]][-3:]))
check('gallery_music_m_results', any('music bgm/m_results.adp' in x for f, x in ev if f >= 285), str([x for x in ev if 'music' in x[1]]))
check('gallery_no_save_without_change', data is None)

# ---- pages wrap from the header (RE 1090/1091; RV Left from CARDS -> DATA)
s, f = keys('R R R L')
ev, data, out = run(PRE + s, f + 10)
check('pages_switch_and_wrap', [x for _, x in gal(ev, 'page')] == ['gal page 1', 'gal page 2', 'gal page 0', 'gal page 2'], str(gal(ev, 'page')))

# ---- open a card: flip (50 frames), sounds 32 then 40, records x20 = 2; saved once on leaving
s, f = keys('D J W100 X J')
ev, data, out = run(PRE + s, f + 60, make_save(cards=range(1, 6)))
o = [f for f, x in ev if x == 'gal open 1']; w = [f for f, x in ev if x == 'gal card 1 state 2']
sid = [(f, int(x.split()[1])) for f, x in ev if x.startswith('sid')]
check('card_flip_writes_2_after_50_frames', o and w and w[0] - o[0] == 49 and (o[0], 32) in sid and (w[0], 40) in sid, '%s %s' % (o, w))
sv = parse(data) if data else None
check('card_open_saved_on_leave', sv and sv['opened'] == {1} and sv['cards'] == set(range(1, 6)) and [x for _, x in ev].count('gal save') == 1)
# Close Card (C) writes 1: the card goes back face down, still taken
s, f = keys('D B X J')
ev, data, out = run(PRE + s, f + 60, make_save(cards=range(1, 6), opened=[1, 2]))
sv = parse(data) if data else None
check('close_card_writes_1', ('gal card 1 state 1' in [x for _, x in ev]) and sv and sv['opened'] == {2} and 1 in sv['cards'] and
      any(x == 'sid 443' for _, x in ev), str(sv and sv['opened']))

# ---- type-3 cards (44 = Schmup Stage) stay padlocked until one of achievements 1-3 is set (RE 996-1002)
nav44 = 'D D D D D R R R J'
s, f = keys(nav44)
ev, data, out = run(PRE + s, f + 60, make_save(cards=[44]))
d = dump(ev, 'dirs')
check('type3_card_locked', d and d[43] == '2' and not gal(ev, 'open'), '%s %s' % (d and d[43], gal(ev, 'open')))
ev, data, out = run(PRE + s, f + 60, make_save(cards=[44], ach={2: 2}))
d = dump(ev, 'dirs')
check('type3_card_unlocked_by_ach', d and d[43] == '1' and [x for _, x in gal(ev, 'open')] == ['gal open 44'])

# ---- art (card 10 = Artwork frame 0): loaded on View, input ignored, freed on close (RE 1060/1061)
s, f = keys('D R R R R R R R R R J D J')
ev, data, out = run(PRE + s, f + 20, make_save(cards=[10], opened=[10]))
ld, fr = [f for f, x in ev if x == 'gal art load 0'], [f for f, x in ev if x == 'gal art free 0']
moves = [f for f, x in ev if x == 'sid 64' and ld and fr and ld[0] < f < fr[0]]
check('art_view_loads_and_frees', ld and fr and fr[0] > ld[0] and not moves, '%s %s moves %s' % (ld, fr, moves))
check('art_view_sound_443', [x for f, x in ev if ld and f in (ld[0], fr[0])].count('sid 443') == 2)
check('art_nothing_saved', data is not None and parse(data)['opened'] == {10} or data is None)

# ---- music cards: play / stop on channel 31 (RE 1042 / 1055); card 9 (1up jingle) once; card 19 is a voice clip
s, f = keys('D J J R R R R R R R R J W300 D J W80')
ev, data, out = run(PRE + s, f, make_save(cards=[1, 9, 19], opened=[1, 9, 19]))
xs = [x for _, x in ev]
check('music_card_plays_track', 'gal play bgm/m_title.adp loop' in xs and 'gal stop 1' in xs and 'music bgm/m_title.adp' in ' '.join(xs))
check('music_card_once_goes_idle', 'gal play bgm/m_1up.adp once' in xs and any(x.startswith('music once') for x in xs) and xs.count('gal idle') >= 1)
check('sound_card_plays_voice', 'gal play carol_awesome' in xs and xs.count('gal idle') == 2, str([x for x in xs if x.startswith('gal')][-6:]))
check('m_results_not_restarted', [x.split()[1] for x in xs if 'm_results' in x] == ['bgm/m_results.adp'])

# ---- DATA page: marks with the YES / NO panel (cursor starts on NO, RV), deletes applied on leaving, one save
rec = {(1, 0): (60000, 300), (2, 1): (90000, 250)}
s, f = keys('R R D J D J D D J D J D J D J J U U U U U J')
ev, data, out = run(PRE + s, f + 60, make_save(cards=[1, 2, 3], opened=[1], ach={3: 2, 42: 2}, rec=rec))
xs = [x for _, x in ev]
check('data_marks', [x for x in xs if x.startswith('gal mark') or x.startswith('gal unmark')] == ['gal mark 0', 'gal mark 2', 'gal mark 3', 'gal unmark 3'],
      str([x for x in xs if 'mark' in x]))
sv = parse(data) if data else None
check('data_delete_applied_on_leave', sv and all(sv['rec'][k][0] == 0 for k in rec) and sv['rec'][(1, 0)][1] == 300 and not sv['cards'] and not sv['opened'] and sv['ach'][2] == 2,
      str(sv and (sv['rec'][(1, 0)], sorted(sv['cards']))))
ent = xs.index('gal enter') if 'gal enter' in xs else 0
check('data_one_save_write', 'gal delete 1010' in xs and len(saves(xs[ent:])) == 1)
# DELETE ACHIEVEMENTS keeps 42 ("Milla Unlocked!", RE 1004)
s, f = keys('R R D D D D J D J U U U U U J')
ev, data, out = run(PRE + s, f + 60, make_save(cards=[1], ach={1: 2, 23: 2, 42: 2}))
sv = parse(data) if data else None
check('delete_achievements_keeps_42', sv and sv['ach'][41] == 2 and sv['ach'][0] == 0 and sv['ach'][22] == 0 and sv['cards'] == {1})

# ---- Card Collector on entry (RE 992): rows 0 (DV) and 2 (AT) complete -> achievements 23 / 25 = 1, saved, the
# "UNLOCKED" pop-up (sound 100) announces them (-> 2) and saves again; a second visit awards nothing
s, f = keys('J', start=380)
ev, data, out = run(PRE + s, f + 40, make_save(cards=list(range(1, 11)) + list(range(21, 31))))
xs = [x for _, x in ev]
sv = parse(data) if data else None
check('card_collector_rows', [x for x in xs if x.endswith('earned')] == ['gal ach 23 earned', 'gal ach 25 earned'] and 'gal toast 23' in xs and 'sid 100' in xs)
check('card_collector_announced_saved', sv and sv['ach'][22] == 2 and sv['ach'][24] == 2 and sum(sv['ach']) == 4 and len(saves(xs)) == 2)
ev2, data2, out2 = run(PRE + s, f + 40, data)
check('card_collector_once', not [x for _, x in ev2 if x.endswith('earned') or x.startswith('gal toast')] and data2 == data)

# ---- v3 save (session 13): taken cards become face down, no achievements; opening then leaving writes v4
v3 = make_save(cards=[1, 2, 3, 15], progress=(2, 0, 0, 0), ver=3)
s, f = keys('D J W100 X J')
ev, data, out = run(PRE + s, f + 60, v3)
c = dump(ev, 'cards')
check('v3_migrated_face_down', c and [i + 1 for i, v in enumerate(c) if v != '0'] == [1, 2, 3, 15] and set(c) <= {'0', '1'} and dump(ev, 'ach') == '0' * 56)
sv = parse(data) if data else None
check('v3_rewritten_as_v4', sv and sv['cards'] == {1, 2, 3, 15} and sv['opened'] == {1} and sv['ach'] == [0] * ACH)

# ---- the title cheat, then the gallery: every card taken (face down), type-3 cards open (achievements 1-3), all rows
ev, data, out = run(CHEAT, 400)
d, r, a = dump(ev, 'dirs'), dump(ev, 'rows'), dump(ev, 'ach')
check('cheat_then_gallery_all_unlocked', d == '1' * 100 and r == '1203456879' and a and a[:3] == '222', '%s %s %s' % (d, r, a))

# ---- invalid saves are ignored (empty gallery) and not overwritten by leaving
bad = bytearray(make_save(cards=range(1, 31), opened=[1])); bad[30] ^= 0xff
s, f = keys('X J')
ev, data, out = run(PRE + s, f + 60, bytes(bad))
check('damaged_save_ignored_not_overwritten', dump(ev, 'cards') == '0' * 100 and data == bytes(bad) and not saves([x for _, x in ev]))
short = make_save(cards=[1])[:700]
ev, data, out = run(PRE + s, f + 60, short)
check('partial_save_ignored_not_overwritten', dump(ev, 'cards') == '0' * 100 and data == short)

# ---- a card picked up in play keeps the others' opened state (pickup = max(old, 1)); AT1's first card is id 21
ev, data, out = run('5 TP 6282 1357\n40 TP 12400 1170\n45 300 R\n', 340, make_save(cards=[1, 2], opened=[1]), {'FP_LEVEL': 'at1', 'FP_CHAR': '4'})
sv = parse(data) if data else None
check('pickup_keeps_opened', sv and sv['cards'] == {1, 2, 21} and sv['opened'] == {1}, str(sv and (sorted(sv['cards']), sorted(sv['opened']))))

# ---- reference only: the user's PC records.dat (read in place, never copied into the port's data; pass / fail only)
REC = '/home/claude/fprun_user/records.dat'
if os.path.exists(REC):
    raw = open(REC, 'rb').read()
    if raw[:10] == b'CNC ARRAY\0':
        X, Y, Z, fl = struct.unpack_from('<4i', raw, 14); v = struct.unpack_from('<%di' % (X * Y * Z), raw, 30)
        R = lambda x, y: v[(y - 1) * X + (x - 1)]
        cards = [n for n in range(1, 101) if R(20, n) >= 1]; opened = [n for n in range(1, 101) if R(20, n) >= 2]
        ach = {k: min(2, R(2, k)) for k in range(1, 57)}
        prog = (min(255, R(1, 40)), min(255, R(1, 42)), min(255, R(1, 43)), min(255, max(R(1, 41), R(1, 44))))
        ev, data, out = run(PRE, 340, make_save(cards=cards, opened=opened, ach=ach, progress=prog))
        c, d, a, r = dump(ev, 'cards'), dump(ev, 'dirs'), dump(ev, 'ach'), dump(ev, 'rows')
        exp_c = ''.join(str(min(2, R(20, n))) for n in range(1, 101))
        check('reference_user_records_cards', c == exp_c)
        check('reference_user_records_no_padlock_after_clear', d is not None and '2' not in d)   # spec 6: all 100 unlocked
        check('reference_user_records_rows_all_shown', r is not None and 'L' not in r)
        check('reference_user_records_achievements', a == ''.join(str(ach[k]) for k in range(1, 57)) and not gal(ev, 'toast'))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
