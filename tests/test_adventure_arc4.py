#!/usr/bin/env python3
"""Adventure phase 2, groups D + E (session 26, docs/ADVENTURE.md 11) on the host build:
- inventory: the D / E scenes of tables/story/scenes.json have their scripts, rows (a split room's views), permanent scene
  numbers (tables/story/sids.json) and rooms in the data dir
- Jiang River: frame 8 after Sky Battalion (I 4) -> SB2's frame with g4 99 (jiang_river, the falling airship) -> the river
  (frame 79 g4 0: jiang_river2 for Lilac, jiang_river2c for Carol / Milla, RV r36_g99 / r36_g99c) -> Talk With Magister
- Thermal Base: frame 8 after TB (I 6): Lilac -> Torturing Lilac (80 g4 0) -> Brevon Encounter (80 g4 1, her variant);
  Carol / Milla -> Brevon Encounter (80 g4 1)
- the Adventure epilogue: ending.c's space scene -> 81 g4 3 -> 82 g4 21 -> 15 g4 14 -> the credits, each part once; the
  Classic ending keeps its hold (no scene)
- pause: SKIP SCENE / NEXT STAGE targets (RE / RV), one completion, audio stopped
- never in Classic / Time Attack, and no half-played or skipped scene after a restart / quit
usage: test_adventure_arc4.py [data_dir]   (default: converted2 with tools/import_story.py's output)"""
import json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
TAB = os.path.join(ROOT, 'tables', 'story')
MINE = ['zao_airship_ambush', 'jiang_river', 'jiang_river2', 'jiang_river2c', 'jiang_river2m', 'tb_torture', 'tb_brevon_l', 'tb_brevon',
        'tb_team_spade', 'sf_army', 'sf_army_l', 'sf_camp', 'sf_camp_l', 'ending', 'ending_m', 'ending2', 'ending3']

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + str(info) + ']' if info != '' else ''))
    fails += 0 if cond else 1

def run(script, frames, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ, FP_TRACE_SND='1')
    for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE'): e.pop(k, None)
    e.update(env or {})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    out = r.stdout + r.stderr
    ev, fr = [], 0
    for l in out.splitlines():
        m = re.match(r'f (\d+) ', l)
        if m: fr = int(m.group(1)); continue
        if l.startswith(('story ', 'csx ', 'music ', 'end st ', 'ending ')): ev.append((fr, l))
    return ev, out

def taps(a, b, step=15, btn='J'): return ''.join('%d %d %s\n' % (f, f + 1, btn) for f in range(a, b, step))
def starts(ev): return [l.split()[2] for f, l in ev if l.startswith('story start ')]
def completes(ev): return [(l.split()[2], int(l.split()[4])) for f, l in ev if l.startswith('story complete ')]
def dumps(ev): return [dict(zip(l.split()[1::2], l.split()[2::2])) for f, l in ev if l.startswith('story run ')]
def end_states(ev): return [int(l.split()[2]) for f, l in ev if l.startswith('end st ')]
def ported(i): return os.path.isfile(os.path.join(TAB, 'scripts', i + '.scn'))

# ------------------------------------------------------------------------------------------------------------ inventory
sc = {s['id']: s for s in json.load(open(os.path.join(TAB, 'scenes.json'), encoding='utf-8'))['scenes']}
rows = json.load(open(os.path.join(TAB, 'rows.json'), encoding='utf-8'))
dlg = json.load(open(os.path.join(TAB, 'dialog.json'), encoding='utf-8'))['dialog']
check('inventory_mine_ported', all(i in sc and ported(i) for i in MINE), [i for i in MINE if i not in sc or not ported(i)])
def rows_ok(i):                                   # a split room (scenes.json 'frame'): rows.json json = the room, frame, its views
    r = rows.get(i, {})
    if 'frame' in sc[i]: return r.get('json') == sc[i]['room'] and r.get('frame') == sc[i]['frame'] and r.get('view')
    return r.get('json') == sc[i]['room'] and 'view' not in r
check('inventory_rows', all(rows_ok(i) for i in MINE if i in sc), [i for i in MINE if i in sc and not rows_ok(i)])
sids = json.load(open(os.path.join(TAB, 'sids.json'), encoding='utf-8'))['sid']
def sid_ok(i):                                   # a base scene has its own number; a character variant names its base (sid_of)
    return (i in sids) != bool(sc[i].get('sid_of')) and (i in sids or sc[i]['sid_of'] in sids and sc[sc[i]['sid_of']]['g4'] == sc[i]['g4'])
check('inventory_sids', all(sid_ok(i) for i in MINE if i in sc), [i for i in MINE if i in sc and not sid_ok(i)])
check('inventory_sf_panel3', all(sc[i]['panel'] == 3 for i in ('sf_army', 'sf_army_l', 'sf_camp', 'sf_camp_l')),
      'RV k81_g0 / k81_g1: START opens no pause (RE 20866)')
check('dialog_3989_paragraphs', len(dlg['3989']) == 111 and dlg['3989'][83][2] == 'li_49_01' and dlg['3989'][48] == dlg['3989'][47],
      'RE: 111 Text::add_line calls (47-49 the same line); RV e81_g3: E 83 = li_49_01')
st = os.path.join(DATA, 'story', 'story.txt')
table = {l.split()[0]: l.split() for l in open(st) if l.strip() and l[0] != '#'} if os.path.isfile(st) else {}
check('data_story_table_mine', all(i in table for i in MINE), 'run tools/import_story.py <data>')
for i in MINE:
    if i not in sc: continue
    d = 'st%d' % sc[i]['room']
    if not os.path.isfile(os.path.join(DATA, d, d + '.lv2')): check('data_room_%s' % d, False, 'missing')
check('stage_rooms_unchanged', all(not os.path.exists(os.path.join(DATA, 'sb2', 'sb2_ens.spr')) or
      b's2252_0_0\0' not in open(os.path.join(DATA, 'sb2', 'sb2_ens.spr'), 'rb').read() for _ in [0]),
      'SB2 (json 37) is also a story room: its stage build has no story rows')

# ----------------------------------------------------------------------------------------- Jiang River (frame 8, I 4)
SB = '5 ADVNEW %d\n10 LOAD sb1\n20 STORY\n30 STAGEEND\n'
for ch, river in ((0, 'jiang_river2'), (2, 'jiang_river2c'), (4, 'jiang_river2m')):
    ev, out = run(SB % ch + taps(80, 4200) + '4300 STORY\n', 4400)
    s = [x for x in starts(ev) if x != 'adv_opening']
    check('jiang_chain_chr%d' % ch, s[:2] == ['jiang_river', river], s)
    c = [x for x in completes(ev) if x[0] != 'adv_opening']
    check('jiang_completions_once_chr%d' % ch, c[:2] == [('jiang_river', 0), (river, 0)] and len({x[0] for x in c}) == len(c), c)
    check('jiang_then_magister_chr%d' % ch, s[2:3] == ['st_magister_talk'], s)   # Talk With Magister (C1, frame 82 g4 8; RE 19864)
ev, out = run(SB % 0 + '100 STORY\n', 120)
check('jiang_part1_in_sb2_frame', dumps(ev) and dumps(ev)[-1]['scene'] == 'jiang_river' and dumps(ev)[-1]['lv'] == 'st3719', dumps(ev)[-1:])
# pause: SKIP SCENE -> the river (RE 9761); NEXT STAGE -> JC1 (RE 9762)
ev, out = run(SB % 0 + '200 201 S\n230 231 D\n260 261 J\n340 STORY\n', 360)
check('jiang_skip_to_river', [x for x in completes(ev) if x[0] == 'jiang_river'] == [('jiang_river', 1)] and starts(ev)[-1] == 'jiang_river2', (completes(ev), starts(ev)))
check('jiang_skip_stops_audio', 'snd stopall' in out or any(l.startswith('music stop') for f, l in ev if f > 260))
ev, out = run(SB % 0 + '200 201 S\n230 231 D\n245 246 D\n260 261 J\n340 STORY\n', 360)
check('jiang_next_stage_jc1', completes(ev)[-1] == ('jiang_river', 2) and dumps(ev)[-1]['lv'] == 'jc1', (completes(ev), dumps(ev)[-1:]))
ev, out = run(SB % 0 + '800 801 S\n830 831 D\n845 846 D\n860 861 J\n940 STORY\n', 960)
check('river_next_stage_jc1', completes(ev)[-1] == ('jiang_river2', 2) and dumps(ev)[-1]['lv'] == 'jc1', (completes(ev), dumps(ev)[-1:]))

# ---------------------------------------------------------------------------------------- Thermal Base (frame 8, I 6)
TB = '5 ADVNEW %d\n10 LOAD tb6\n20 STORY\n30 STAGEEND\n'
ev, out = run(TB % 0 + taps(80, 9000) + '9100 STORY\n', 9200)
s = [x for x in starts(ev) if x != 'adv_opening']
want = ['tb_torture', 'tb_brevon_l'] + (['tb_arrest'] if ported('tb_arrest') else [])   # (C2's Lilac's Arrest, 82 g4 17)
check('tb_lilac_chain', s[:len(want)] == want, s)
check('tb_lilac_completions_once', [x for x in completes(ev) if x[0] != 'adv_opening'][:2] == [('tb_torture', 0), ('tb_brevon_l', 0)], completes(ev))
for ch in (2, 4):
    ev, out = run(TB % ch + taps(80, 4600) + '4700 STORY\n', 4800)
    s = [x for x in starts(ev) if x != 'adv_opening']
    check('tb_chr%d_brevon_only' % ch, s[:1] == ['tb_brevon'] and 'tb_torture' not in s, s)
    if ported('tb_outside_team'):                # C2's Outside Thermal Base (82 g4 17, g1 != 0), then the snowfields (81 g4 0)
        check('tb_chr%d_then_outside_team_sf_army' % ch, s[1:3] == ['tb_outside_team', 'sf_army'], s)

# Enter Team Spade: frame 8 after Trap Hideout (I 8) for Carol / Milla -> 82 g4 16 -> TB1 (RV r82_g16c / r82_g16m); Lilac: no scene
for ch in (2, 4):
    ev, out = run('5 ADVNEW %d\n10 LOAD th3\n20 STORY\n30 STAGEEND\n' % ch + '700 STORY\n', 720)
    d = dumps(ev)
    check('team_spade_chr%d' % ch, [x for x in starts(ev) if x != 'adv_opening'] == ['tb_team_spade'] and completes(ev)[-1] == ('tb_team_spade', 0)
          and d and d[-1]['lv'] == 'tb1', (starts(ev), completes(ev), d[-1:]))
ev, out = run('5 ADVNEW 2\n10 LOAD th3\n20 STORY\n30 STAGEEND\n200 201 S\n230 231 D\n260 261 J\n340 STORY\n', 360)
check('team_spade_skip_tb1', completes(ev)[-1] == ('tb_team_spade', 1) and dumps(ev)[-1]['lv'] == 'tb1', (completes(ev), dumps(ev)[-1:]))

# ---------------------------------------------------------------------------- Zao's airship ambushed (82 g4 7 -> SB1)
ev, out = run('5 ADVNEW 0\n200 SCENE zao_airship_ambush\n' + taps(260, 900) + '1000 STORY\n', 1020)
d = dumps(ev)
check('ambush_then_sb1', completes(ev)[-1] == ('zao_airship_ambush', 0) and d and d[-1]['lv'] == 'sb1' and d[-1]['run'] == '1', (completes(ev), d[-1:]))
if ported('zao_airship2'):                         # group B's Zao's Airship (SB1's frame, g4 99) ends into frame 82 g4 7 (RV)
    ev, out = run('5 ADVNEW 0\n200 SCENE zao_airship2\n' + taps(260, 2600) + '2700 STORY\n', 2720)
    s = [x for x in starts(ev) if x != 'adv_opening']
    check('zao_airship2_then_ambush', s[:2] == ['zao_airship2', 'zao_airship_ambush'] and dumps(ev)[-1]['lv'] == 'sb1', (s, dumps(ev)[-1:]))

# ------------------------------------------------------------------------------- the snowfields (81 g4 0 -> 81 g4 1 -> BG)
for ch, army, camp in ((0, 'sf_army_l', 'sf_camp_l'), (2, 'sf_army', 'sf_camp'), (4, 'sf_army', 'sf_camp')):
    ev, out = run('5 ADVNEW %d\n200 SCENE %s\n' % (ch, army) + taps(260, 9500) + '9600 STORY\n', 9620)   # (entered from C2's scenes: their step names sf_army, the variant by g1)
    d = dumps(ev)
    check('snowfields_chain_chr%d' % ch, [x for x in starts(ev) if x != 'adv_opening'] == [army, camp] and
          [x for x in completes(ev) if x[0] != 'adv_opening'] == [(army, 0), (camp, 0)] and d and d[-1]['lv'] == 'bg1', (starts(ev), completes(ev), d[-1:]))
# panel 3: START opens no pause in the snowfields (RV k81_g0_skp2 / k81_g1_skp2 at F 300 and 1500; RE 20866), the scene plays on
for ch, army in ((0, 'sf_army_l'), (2, 'sf_army')):
    ev, out = run('5 ADVNEW %d\n200 SCENE sf_army\n' % ch + '400 401 S\n430 431 D\n460 461 J\n540 STORY\n', 560)
    d = dumps(ev)
    check('snowfields_start_ignored_chr%d' % ch, not [x for x in completes(ev) if x[0] == army] and d and d[-1]['scene'] == army
          and d[-1]['paused'] == '0', (completes(ev), d[-1:]))

# ------------------------------------------------------------------------------------------------- the epilogue (E)
END = '5 ADVNEW %d\n30 ENDING\n'
ev, out = run(END % 0 + taps(1300, 12500) + '12600 STORY\n', 12700)
check('epilogue_chain', starts(ev)[1:] == ['ending', 'ending2', 'ending3'], starts(ev))
check('epilogue_completions_once', [x for x in completes(ev) if x[0] != 'adv_opening'] == [('ending', 0), ('ending2', 0), ('ending3', 0)], completes(ev))
check('epilogue_then_credits', 3 in end_states(ev) and dumps(ev)[-1]['mode'] == '4', (sorted(set(end_states(ev))), dumps(ev)[-1:]))
check('epilogue_after_space_scene', any(l.startswith('end st 1 b 1110') for f, l in ev) and 2 not in end_states(ev), 'no Classic hold in Adventure')
for ch, first in ((2, 'ending'), (4, 'ending_m')):  # RV e81_g3c: Carol's is Lilac's; e81_g3m: Milla's part 1 differs
    ev, out = run(END % ch + taps(1300, 4400) + '4500 STORY\n', 4520)
    check('epilogue_chr%d' % ch, starts(ev)[1:3] == [first, 'ending2'], starts(ev))
# Classic: the space scene, the hold, the credits; no scene (Classic is unchanged)
ev, out = run('5 MODE 1\n30 ENDING\n' + '1300 STORY\n', 1400)
check('classic_ending_no_epilogue', not starts(ev) and 2 in end_states(ev) and 3 in end_states(ev), (starts(ev), sorted(set(end_states(ev)))))
# pause in part 1: SKIP SCENE -> part 3 (RE 20870), NEXT STAGE -> the credits (RE 20869)
P = '1300 1301 S\n1330 1331 D\n%s1360 1361 J\n1440 STORY\n'
ev, out = run(END % 0 + P % '' , 1460)
check('epilogue_skip_to_farewell', [x for x in completes(ev) if x[0] == 'ending'] == [('ending', 1)] and starts(ev)[-1] == 'ending3', (completes(ev), starts(ev)))
ev, out = run(END % 0 + P % '1345 1346 D\n', 1460)
check('epilogue_next_stage_credits', completes(ev)[-1] == ('ending', 2) and 3 in end_states(ev) and dumps(ev)[-1]['mode'] == '4', (completes(ev), dumps(ev)[-1:]))
# the epilogue's panel is frame 2 (no QUIT, RV k81_g3_row2): after the credits a new Classic run's ending has no epilogue
ev, out = run(END % 0 + P % '1345 1346 D\n' + ''.join('%d %d J\n%d STORY\n' % (f, f + 1, f + 15) for f in range(1470, 9000, 30)), 9100)
d = dumps(ev)
check('epilogue_credits_then_menu', any(x['mode'] == '0' and x['run'] == '0' for x in d) and 'story reset' in out, d[-1:] if d else out[-300:])
# ------------------------------------------------------------------------------------------------------- gating
ev, out = run('5 MODE 1\n10 LOAD sb1\n30 STAGEEND\n60 STORY\n', 80)
check('classic_sb_end_no_scene', not starts(ev) and dumps(ev)[-1]['lv'] == 'jc1', (starts(ev), dumps(ev)[-1:]))
ev, out = run('5 MODE 0\n10 LOAD tb6\n30 STAGEEND\n60 STORY\n', 80, {'FP_TA': '1'})
check('time_attack_tb_end_no_scene', not starts(ev), starts(ev))
ev, out = run('5 MODE 2\n10 LOAD sb1\n30 STAGEEND\n60 STORY\n', 80)
check('stale_adventure_flag_no_scene', not starts(ev) and dumps(ev)[-1]['gate'] == '0', dumps(ev)[-1:])
ev, out = run('5 MODE 2\n30 ENDING\n1300 STORY\n', 1320)
check('stale_flag_ending_no_epilogue', not starts(ev) and 2 in end_states(ev), (starts(ev), sorted(set(end_states(ev)))))
# an Adventure run quit to the menu (dv_meet's panel 1 QUIT), then Classic: the SB / TB ends and the ending start no scene
QUIT = '5 ADVNEW 0\n' + '2200 2201 S\n2215 2216 D\n2225 2226 D\n2240 2241 J\n2320 STORY\n'
ev, out = run(QUIT + '2400 MODE 1\n2410 LOAD sb1\n2430 STAGEEND\n2460 STORY\n', 2480)
d = dumps(ev)
check('quit_then_classic_sb_no_scene', 'jiang_river' not in starts(ev) and d[-1]['gate'] == '0' and d[-1]['lv'] == 'jc1', (starts(ev), d[-1:]))
ev, out = run(QUIT + '2400 MODE 1\n2410 LOAD tb6\n2430 STAGEEND\n2460 STORY\n', 2480)
check('quit_then_classic_tb_no_scene', not [x for x in starts(ev) if x.startswith('tb_')], starts(ev))
ev, out = run(QUIT + '2400 MODE 1\n2410 ENDING\n3700 STORY\n', 3720)
check('quit_then_classic_ending_no_epilogue', not [x for x in starts(ev) if x.startswith('ending')] and 2 in end_states(ev), (starts(ev), sorted(set(end_states(ev)))))

# ------------------------------------------------------------------------------------------------------------- save
with tempfile.TemporaryDirectory() as td:
    sv = os.path.join(td, 'save.bin')
    env = {'FP_SAVE': sv, 'FP_FILE': '1'}
    run('600 TP 16000 2280\n620 1300 R\n', 1100, dict(env, FP_LEVEL='dv4', FP_FILE_NEW='1'))   # DV4's results make slot 1 used
    ev, out = run('5 MODE 2\n6 ADVNEW 0\n10 LOAD sb1\n30 STAGEEND\n200 201 S\n230 231 D\n260 261 J\n340 STORY\n', 360, env)
    d = dumps(ev)                                  # (the position and the seen flags use the permanent scene numbers)
    check('save_story_position_jiang', d and d[-1]['pos'] == str(sids['jiang_river'] + 1), d[-1] if d else '')
    ev, out = run('5 STORY\n', 10, env)
    d = dumps(ev)
    check('save_seen_jiang', d and int(d[0]['seen'], 16) >> sids['jiang_river'] & 1 == 1, d[0] if d else '')

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
