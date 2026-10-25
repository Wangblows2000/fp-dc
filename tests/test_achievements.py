#!/usr/bin/env python3
"""Host tests for the achievements (session 16; build/re/spec_achievements.md, src/game/save.c ach_earn, the hooks in
combat.c / serpentine.c / aqua_boss.c / rm_mantalith.c / relic_boss.c / rm_claw.c / chase.c / aqua.c / switches.c /
results.c, the pop-up in fe_gallery.inc). Every hook is checked under its condition and without it. Host trace lines:
'ach earn <k>' (0 -> 1), 'ach finish M A slash cube beam' (a boss finish moment), 'ach results stage <s> ms <t>' (results
D = 149), 'gal popup enter <n> next <0 menu / 1 stage select / 2 next stage>', 'gal toast <k>', 'gal toast done'.
usage: test_achievements.py [data_dir]   (default: converted2)"""
import os, struct, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
STAGES, FILES, CHARS, ACH = 15, 8, 4, 56

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

def make_save(cards=(), ach=None, ver=4, difficulty=1):
    """a save in the port's layout (save.c; tests/test_gallery.py): v4, or v3 without the gallery fields"""
    b = b'FPS' + bytes([ver]) + bytes([difficulty, 100, 100, 70, 1]) + bytes(FILES * (8 + STAGES * 7))
    b += bytes(6 * STAGES * CHARS) + bytes(CHARS) + bits(cards)
    if ver >= 4:
        a = [0] * ACH
        for k, v in (ach or {}).items(): a[k - 1] = v
        b += bytes(13) + bytes(a)
    return b + struct.pack('<H', crc16(b))

def parse_ach(d):
    assert d[:4] == b'FPS\x04' and crc16(d[:-2]) == struct.unpack('<H', d[-2:])[0] and len(d) == 1361, 'bad v4 save'
    p = 4 + 5 + FILES * (8 + STAGES * 7) + 6 * STAGES * CHARS + 4
    cb = d[p:p + 13]
    return [0] + list(d[p + 26:p + 26 + ACH]), {n for n in range(1, 101) if cb[(n - 1) >> 3] >> ((n - 1) & 7) & 1}

class Run:
    """one host run: earned achievements (in order), finish lines, results lines, per-frame events, save writes"""
    def __init__(self, script, frames, env, save=None):
        with tempfile.TemporaryDirectory() as d:
            sp = os.path.join(d, 'script.txt'); open(sp, 'w').write(script)
            e = dict(os.environ, FP_TRACE_SND='1', FP_TRACE_SID='1', FP_FE_TRACE='1')
            for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE'): e.pop(k, None)
            if save is not None:
                e['FP_SAVE'] = os.path.join(d, 'save.bin'); open(e['FP_SAVE'], 'wb').write(save)
            e.update(env)
            r = subprocess.run([BIN, DATA, str(frames), sp], capture_output=True, text=True, env=e)
            self.out = r.stdout
            self.save = open(e['FP_SAVE'], 'rb').read() if save is not None and os.path.exists(e['FP_SAVE']) else None
        self.earned, self.finish, self.results, self.ev, self.saves, self.idx, self.rows = [], [], [], [], [], {}, []
        pend = []
        for line in self.out.splitlines():
            t = line.split()
            if not t: continue
            if line.startswith('ach earn'): self.earned.append(int(t[2]))
            if line.startswith('ach finish'): self.finish.append(dict(zip(t[2::2], map(int, t[3::2]))))
            if line.startswith('ach results'): self.results.append((int(t[3]), int(t[5])))
            if t[0] == 'f':
                f = int(t[1]); self.ev += [(f, x) for x in pend]; pend = []
                self.rows.append(dict(zip(t[0::2], t[1::2])))
            elif t[0] in ('gal', 'sid', 'save') and not line.startswith('save cards'): pend.append(line)
            elif t[0] == 'fe': self.idx[int(t[1])] = int(t[3])
            elif t[0] == 'ach': pend.append(line)
        self.ev += [(frames, x) for x in pend]
    def has(self, x): return any(e == x for _, e in self.ev)
    def at(self, x): return next((f for f, e in self.ev if e == x), None)
    def writes(self): return [f for f, e in self.ev if e.startswith('save ')]

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
FIN = set(range(10, 18))

# ---- 10-17: the finishing move on the boss's finish moment (AT4 4663-4671: the snake's O == 1, the frame of the kill;
# the host cheat BOSSHP 0 kills it on that frame). Each move is held across the kill frame; the other moves stay unearned.
AT4 = '60 TP 4952 254\n60 SETV others E 99999\n%d BOSSHP 0\n'
CLAW = '900 925 R\n918 930 RD\n930 932 J\n936 937 X\n'      # Carol: run, roll, ball jump, attack = the big slash (1800)
CASES = [('lilac_none', '0', '', 950, set()),
         ('lilac_rising_slash_10', '0', '940 950 U\n944 946 UX\n', 950, {10}),
         ('lilac_cyclone_11', '0', '930 932 J\n940 942 J\n', 950, {11}),
         ('lilac_boost_12', '0', '930 960 B\n', 950, {12}),
         ('carol_claw_no_big_slash', '2', CLAW, 939, set()),
         ('carol_wild_claw_13', '2', CLAW, 946, {13}),
         ('carol_wild_kick_14', '2', '940 941 B\n', 950, {14}),
         ('carol_bike_15', '3', '', 950, {15}),
         ('milla_none', '4', '', 950, set()),
         ('milla_phantom_block_16', '4', '880 940 X\n975 977 X\n', 985, {16}),
         ('milla_super_shield_17', '4', '870 930 X\n940 970 B\n', 975, {17})]
for name, ch, extra, kf, want in CASES:
    r = Run(AT4 % kf + extra, kf + 5, {'FP_LEVEL': 'at4', 'FP_CHAR': ch})
    got = set(r.earned) & FIN
    check('finish_' + name, len(r.finish) == 1 and got == want, '%s finish %s' % (sorted(got), r.finish))

# ---- DV4 (5581-5589: health 0, hitbox flag 6, G == 1) through the boss route; one finish frame, then the results:
# Dragon Valley Speedrunner (43), the pop-up after the wipe (frame 8 -> 9), A -> Relic Maze 1 (Classic routing)
DV4B = open(os.path.join(HERE, 'play', 'dv4_to_boss.txt')).read()
r = Run(DV4B + '4560 4561 J\n', 4700, {'FP_LEVEL': 'dv4', 'FP_SN_AUTOKILL': '1'})
check('dv4_finish_moment_once', len(r.finish) == 1 and not set(r.earned) & FIN, str(r.finish))
pe = next((f for f, e in r.ev if e.startswith('gal popup enter')), None)
check('dv4_speedrunner_43_popup_then_rm1', 43 in r.earned and pe and r.idx.get(pe) == 9 and r.rows[-1]['lv'] and r.rows[-1]['act'] == '1' and r.rows[-1]['mode'] == '1',
      'earned %s popup %s act %s' % (r.earned, pe, r.rows[-1]['act']))

# ---- RM5: the Mantalith (7118 / 7120-7128 at head I == 2), test_rm_mantalith's route (cheat kills after the intro)
MT = '30 TP 2690 500\n30 SETV others E 99999\n40 520 R\n' + ''.join('%d BOSSHP 0\n' % f for f in (1000, 1400, 1800, 2200, 2600))
r = Run(MT, 4800, {'FP_LEVEL': 'rm5', 'FP_CHAR': '0'})
check('rm5_finish_moment_once_42', len(r.finish) == 1 and 42 in r.earned and not set(r.earned) & FIN, '%s %s' % (r.earned, r.finish))
check('rm5_speedrunner_44', 44 in r.earned and r.results and r.results[0][0] == 2 and r.results[0][1] <= 480000, str(r.results))
check('rm5_no_preemptive_after_intro', 36 not in r.earned)
check('rm5_popup_then_fn1', r.has('gal popup enter 2 next 2') and r.has('gal toast 42') and r.idx[4799] == 9, str([e for _, e in r.ev if 'popup' in e]))
# 36 (6900): both arm weak points at A <= 0 while the intro group runs (L 61: the fight group is already on, the arms take
# hits: Lilac's attacks lower Hitbox2 during the intro); the cheat zeroes both arms in the intro
r = Run('30 TP 2690 500\n30 SETV others E 99999\n40 520 R\n' + ''.join('%d %d X\n' % (f, f + 2) for f in range(570, 790, 8)), 800,
        {'FP_LEVEL': 'rm5', 'FP_CHAR': '0', 'FP_DBG_MT': '1'})
mt = [dict(zip(l.split()[2::2], l.split()[3::2])) for l in r.out.splitlines() if l.startswith('mt ')]
h2 = [float(m['h2']) for m in mt if m['L'] == '61' and m['grp'][2] == '1']
check('rm5_arms_damageable_in_intro', h2 and min(h2) < h2[0] and 36 not in r.earned, 'h2 %s..%s' % (h2[:1], min(h2) if h2 else None))
r = Run('30 TP 2690 500\n30 SETV others E 99999\n40 520 R\n600 BOSSHP 0\n', 610, {'FP_LEVEL': 'rm5', 'FP_CHAR': '0'})
check('rm5_preemptive_strike_36', r.earned == [36], str(r.earned))

# ---- 18 Switch Skipper (DV2 5098 / 5099, INFERRED reading): into the zone right of the closed door (no plate pressed)
r = Run('58 TP 3060 1120\n', 120, {'FP_LEVEL': 'dv2'})
check('switch_skipper_18', r.earned == [18], str(r.earned))
r = Run('58 TP 3060 1120\n', 120, {'FP_LEVEL': 'dv2', 'FP_CHAR': '2'})
check('switch_skipper_no_character_test', r.earned == [18], str(r.earned))
PUSH = '1 140 L\n200 215 L\n215 222 LJ\n222 280 L\n320 566 R\n596 597 L\n800 830 R\n830 845 RJ\n845 1000 R\n'   # block on the plate, out of the pit, through
r = Run(PUSH, 1000, {'FP_LEVEL': 'dv2', 'FP_SPAWN_X': '2895', 'FP_SPAWN_Y': '520'})
check('switch_door_opened_no_18', max(float(x['x']) for x in r.rows) > 3200 and 18 not in r.earned, 'max x %s %s' % (max(float(x['x']) for x in r.rows), r.earned))
r = Run('58 TP 2900 1120\n', 300, {'FP_LEVEL': 'dv2'})
check('switch_plate_no_18', not r.earned, str(r.earned))

# ---- 19 Claw Bypass (RM3 6272): the engine wrecked while the Claw is alive; not after beating it (test_rm_claw's route)
WALK = '10 TP 630 470\n20 60 R\n'
r = Run(WALK + '800 TP 6560 440\n805 SETV others E 99999\n820 PB3 6676 490\n', 1000, {'FP_LEVEL': 'rm3', 'FP_CHAR': '0'})
check('claw_bypass_19', r.earned == [19], str(r.earned))
r = Run(WALK + '800 TP 6330 520\n805 SETV others E 99999\n1100 BOSSHP 0\n1450 TP 6560 440\n1470 PB3 6676 490\n', 1900, {'FP_LEVEL': 'rm3', 'FP_CHAR': '0', 'FP_DBG_CLAW': '1'})
wreck = any(l.startswith('claw ') and l.split()[3][3] == '1' for l in r.out.splitlines())
check('claw_beaten_then_wreck_no_19', wreck and 19 not in r.earned, 'wreck %s %s' % (wreck, r.earned))

# ---- 33 Rolling Stones (DV4 5453-5460): the boulder passes the view (the respawn branch); not on a normal chase
r = Run('1 300 R\n5 SETV others E 99999\n400 TP 2400 2200\n', 450, {'FP_LEVEL': 'dv4'})
check('rolling_stones_33', r.earned == [33], str(r.earned))
r = Run('1 1200 R\n', 1200, {'FP_LEVEL': 'dv4'})
check('chase_no_33', 33 not in r.earned, str(r.earned))

# ---- 34 Push Block KO (RM2 5955): the pushable block on the Eye Golem's eye
r = Run('10 TP 2450 150\n10 SETV others E 99999\n100 PB 2429 200\n', 115, {'FP_LEVEL': 'rm2'})
check('push_block_ko_34', r.earned == [34], str(r.earned))
r = Run('10 TP 2450 150\n10 SETV others E 99999\n', 115, {'FP_LEVEL': 'rm2'})
check('eye_golem_no_block_no_34', not r.earned, str(r.earned))

# ---- 35 All Bone, No Brain (RM5 7117): the gem defeated while HEAD B > 0 (its own back strike, 6842). Lilac rides its
# back (the strike), the gem at 25 HP; a gem beaten directly, or a strike that does not finish it, does not count
BONE = '10 TP 700 1430\n10 SETV others E 99999\n245 TP 620 1340\n'
r = Run(BONE + '250 BOSSHP 25\n', 400, {'FP_LEVEL': 'rm5'})
check('all_bone_no_brain_35', r.earned == [35], str(r.earned))
r = Run(BONE + '250 BOSSHP 0\n', 400, {'FP_LEVEL': 'rm5'})
check('bone_golem_beaten_directly_no_35', not r.earned, str(r.earned))
r = Run(BONE, 400, {'FP_LEVEL': 'rm5', 'FP_TRACK': '357:-1'})
gem = [float(l.split()[11]) for l in r.out.splitlines() if l.startswith('ob ')]
check('bone_golem_strike_survived_no_35', gem and min(gem) == 45 and not r.earned, 'gem %s %s' % (min(gem) if gem else None, r.earned))

# ---- 38 Treasure Hunter (4189): Milla digs a treasure spot (AT1, RM1); not elsewhere, not Lilac
DIG = '60 TP %d %d\n61 SETV others E 9999\n80 260 D\n100 102 X\n' + ''.join('%d %d X\n' % (f, f + 2) for f in range(110, 250, 10))
r = Run(DIG % (10544, 1090), 300, {'FP_LEVEL': 'at1', 'FP_CHAR': '4'})
check('treasure_hunter_38_at1', r.earned == [38], str(r.earned))
r = Run(DIG % (2312, 1080), 300, {'FP_LEVEL': 'rm1', 'FP_CHAR': '4'})
check('treasure_hunter_38_rm1', r.earned == [38], str(r.earned))
r = Run(DIG % (10300, 1090), 300, {'FP_LEVEL': 'at1', 'FP_CHAR': '4'})
check('dig_off_spot_no_38', not r.earned, str(r.earned))
r = Run(DIG % (10544, 1090), 300, {'FP_LEVEL': 'at1', 'FP_CHAR': '0'})
check('lilac_no_38', not r.earned, str(r.earned))

# ---- 43-45 speedrunners (D = 149: the stage time ML.Z <= 5 / 8 / 5 min, any mode): DV4 path B with the results started
# so that the time is exactly 300000 ms (18000 frames) or one frame more
for start, ms, ok in ((17947, 300000, True), (17948, 300016, False)):
    r = Run('5 TP 16000 2280\n5 SETV others E 99999\n%d 18600 R\n' % start, 18450, {'FP_LEVEL': 'dv4'})
    check('dv_speedrunner_%d_ms' % ms, r.results == [(1, ms)] and (43 in r.earned) == ok, '%s %s' % (r.results, r.earned))
r = Run(AT4 % 18100, 19600, {'FP_LEVEL': 'at4', 'FP_CHAR': '4'})
check('at_speedrunner_over_5_min_no_45', r.results and r.results[0][0] == 0 and r.results[0][1] > 300000 and 45 not in r.earned, '%s %s' % (r.results, r.earned))
r = Run(AT4 % 950, 1700, {'FP_LEVEL': 'at4', 'FP_CHAR': '4'})
check('at_speedrunner_45', r.results and r.results[0][1] <= 300000 and 45 in r.earned and 25 not in r.earned, '%s %s' % (r.results, r.earned))
# Time Attack too; the pop-up then continues to the stage select (frame 5)
r = Run('5 TP 16000 2280\n20 700 R\n560 561 J\n', 620, {'FP_LEVEL': 'dv4', 'FP_TA': '1'})
check('time_attack_speedrunner_popup_to_stage_select', 43 in r.earned and r.has('gal popup enter 1 next 1') and r.idx[619] == 5, 'idx %s' % r.idx.get(619))
# no difficulty test (RE): a Casual save earns it as well
r = Run('5 TP 16000 2280\n20 700 R\n', 520, {'FP_LEVEL': 'dv4'}, make_save(difficulty=3))
check('casual_difficulty_earns_43', 43 in r.earned, str(r.earned))

# ---- 23-25 Card Collector (AT4 4662 / DV4 5580 / RM5 7119 at D = 149): the stage's 10 cards, taken at any time. AT4's
# three cards (28-30) are taken in this run (the port commits them at D = 150: results.c commits first, as the spec says)
CARDS = '5 SETV others E 99999\n10 TP 3786 165\n20 TP 2946 877\n30 TP 1346 1933\n' + AT4 % 950
r = Run(CARDS, 1700, {'FP_LEVEL': 'at4', 'FP_CHAR': '4'}, make_save(cards=range(21, 28)))
a, c = parse_ach(r.save) if r.save else ([0] * 57, set())
check('card_collector_25_with_this_runs_cards', 25 in r.earned and c == set(range(21, 31)) and a[25] == 1 and a[45] == 1,
      '%s %s' % (r.earned, sorted(c)))
r = Run(CARDS.replace('30 TP 1346 1933\n', ''), 1700, {'FP_LEVEL': 'at4', 'FP_CHAR': '4'}, make_save(cards=range(21, 28)))
check('card_collector_needs_all_10', 25 not in r.earned and 45 in r.earned, str(r.earned))
r = Run('5 TP 16000 2280\n20 700 R\n', 520, {'FP_LEVEL': 'dv4'}, make_save(cards=range(1, 11)))
check('card_collector_23_dv', sorted(r.earned) == [23, 43], str(r.earned))
r = Run(MT, 4800, {'FP_LEVEL': 'rm5', 'FP_CHAR': '0'}, make_save(cards=range(11, 21)))
check('card_collector_24_rm', 24 in r.earned and 23 not in r.earned and 25 not in r.earned, str(r.earned))

# ---- the pop-up (frame 8 -> 9): after the results wipe, waits for A without a time limit (RV > 7 s), writes 2, one save,
# then the stage flow goes on (Classic: RM1); a reload with the save announces nothing again
r = Run('5 TP 16000 2280\n20 700 R\n1200 1201 J\n', 1260, {'FP_LEVEL': 'dv4'}, make_save())
w = r.writes(); pe = next((f for f, e in r.ev if e.startswith('gal popup enter')), None); done = r.at('gal toast done')
check('popup_after_results_save', len(w) == 2 and pe and w[0] < pe, 'writes %s popup %s' % (w, pe))
check('popup_waits_for_confirm', pe and done == 1201 and all(r.idx[f] == 9 for f in range(pe, 1201)) and r.at('sid 100') == pe + 1, 'popup %s done %s' % (pe, done))
check('popup_writes_2_one_save', w[1:] == [1201], str(w))
a, _ = parse_ach(r.save) if r.save else ([0] * 57, set())
check('popup_saved_announced', a[43] == 2 and sum(a) == 2, str([k for k in range(1, 57) if a[k]]))
check('popup_continues_to_rm1', r.rows[-1]['act'] == '1' and r.rows[-1]['mode'] == '1')
r2 = Run('5 TP 16000 2280\n20 700 R\n', 600, {'FP_LEVEL': 'dv4'}, r.save)
check('persisted_no_second_popup', not r2.earned and not any('popup' in e for _, e in r2.ev) and r2.rows[-1]['act'] == '1' and r2.rows[-1]['mode'] == '1',
      '%s %s' % (r2.earned, r2.rows[-1]['act']))
# several pending (more than 3: two pages); an old v3 save is converted and rewritten as v4
r = Run('5 TP 16000 2280\n20 700 R\n560 561 J\n600 601 J\n', 640, {'FP_LEVEL': 'dv4'}, make_save(ach={2: 1, 5: 1, 7: 1}))
check('popup_two_pages', r.has('gal toast 2') and r.has('gal toast 43') and r.writes()[1:] == [601] and parse_ach(r.save)[0][43] == 2, str([e for _, e in r.ev if 'toast' in e]))
r = Run('5 TP 16000 2280\n20 700 R\n560 561 J\n', 600, {'FP_LEVEL': 'dv4'}, make_save(cards=[1], ver=3))
check('v3_save_converted', r.save and parse_ach(r.save)[0][43] == 2 and parse_ach(r.save)[1] == {1})

# ---- the gallery's Card Collector pop-up waits for confirm as well (no 150-frame auto-advance any more)
PRE = '40 42 S\n200 201 D\n220 221 D\n240 241 J\n'
r = Run(PRE, 800, {}, make_save(cards=range(1, 11)))
check('gallery_popup_waits', r.has('gal toast 23') and not r.has('gal toast done') and len(r.writes()) == 1, str(r.writes()))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
