#!/usr/bin/env python3
"""Adventure mode story tests (session 25, src/game/story.c / cutscene.c, docs/ADVENTURE.md) on the host build.
- inventory: tables/story (scenes.json, dialog.json, scripts, rows.json) and the data dir's story/ are consistent
- the first arc plays in order, each scene exactly once: a new Adventure (from the real menus and from ADVNEW) -> the
  opening (frame 81 g4 2) -> Dragon Valley (Meet Lilac and Carol, frame 75 g4 0) with its character select -> DV1; the end
  of Dragon Valley (frame 8, I 1) -> Crash Site (75 g4 1) -> part 2 (14 g11) -> Meet Gong and Neera (76 g4 1) -> RM1
- the character select picks Lilac or Carol (g1 of the stages after it)
- never in Classic or Time Attack: their menu starts, a stage end, a restart, and a return to the menu after an Adventure
  run followed by another mode
- pause: resume continues the scene; SKIP SCENE / NEXT STAGE / QUIT, and START in the opening: the completion runs once,
  the next state is the original's (RV traces build/orig_traces/adv k*), voices and music stop
- save: the story position and the seen flags are written (save v5) and read back
usage: test_adventure.py [data_dir]   (default: converted2; the data dir needs tools/import_story.py's output)"""
import json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
TAB = os.path.join(ROOT, 'tables', 'story')
ARC = ['adv_opening', 'dv_meet', 'dv_crash', 'dv_crash2', 'rm_gong']

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
    ev = []                                   # (frame, line) of the story events
    fr = 0
    for l in out.splitlines():
        m = re.match(r'f (\d+) ', l)
        if m: fr = int(m.group(1)); continue
        if l.startswith(('story ', 'csx ', 'music ')): ev.append((fr, l))
    return ev, out

def taps(a, b, step=15, btn='J'):           # one press every step frames (dismisses each line once its arrow shows)
    return ''.join('%d %d %s\n' % (f, f + 1, btn) for f in range(a, b, step))

def starts(ev): return [l.split()[2] for f, l in ev if l.startswith('story start ')]
def completes(ev): return [(l.split()[2], int(l.split()[4])) for f, l in ev if l.startswith('story complete ')]
def dumps(ev): return [dict(zip(l.split()[1::2], l.split()[2::2])) for f, l in ev if l.startswith('story run ')]

# ------------------------------------------------------------------------------------------------------------ inventory
sc = json.load(open(os.path.join(TAB, 'scenes.json'), encoding='utf-8'))['scenes']
dlg = json.load(open(os.path.join(TAB, 'dialog.json'), encoding='utf-8'))
rows = json.load(open(os.path.join(TAB, 'rows.json'), encoding='utf-8'))
ids = [s['id'] for s in sc]
check('inventory_unique_ids', len(ids) == len(set(ids)), len(ids))
check('inventory_cutscene_list', sorted(set(s['list'] for s in sc if s.get('list') is not None)) == list(range(37)),
      'the original CutsceneList_42 has 37 entries (RE)')
check('inventory_dialog_objects', len(dlg['dialog']) == 21 and sum(len(v) for v in dlg["dialog"].values()) == 998,
      '%d objects' % len(dlg['dialog']))
check('inventory_captions', 'str_three_days_later_330' in dlg.get('captions', {}))
ported = [s for s in sc if os.path.isfile(os.path.join(TAB, 'scripts', s['id'] + '.scn'))]
check('inventory_first_arc_ported', [s['id'] for s in ported if s['id'] in ARC] == ARC, [s['id'] for s in ported])
bad = []
for s in ported:
    h = None
    for l in open(os.path.join(TAB, 'scripts', s['id'] + '.scn'), encoding='utf-8'):
        t = l.split()
        if t and t[0] == 'dialog': h = t[1]
        if len(t) >= 3 and t[1] == 'say':
            k = int(t[2])
            if h in (None, '-') or k >= len(dlg['dialog'][h]): bad.append((s['id'], k))
            elif s.get('lines') and not (s['lines'][0] <= k <= s['lines'][1] or k in s.get('also', [])): bad.append((s['id'], k, 'range'))
    if s['id'] not in rows or rows[s['id']]['json'] != s['room']: bad.append((s['id'], 'rows'))
check('inventory_scripts_lines_and_rows', not bad, bad[:5])
st = os.path.join(DATA, 'story', 'story.txt')
inl = [s for s in ported if s.get('next') == 'back']           # in-stage scenes (session 25b): story/inline.txt, no room
check('data_story_table', os.path.isfile(st) and [l.split()[0] for l in open(st) if l.strip() and l[0] != '#'] == [s['id'] for s in ported if s not in inl],
      'run tools/import_story.py <data>')
if inl:
    it = os.path.join(DATA, 'story', 'inline.txt')
    check('data_inline_table', os.path.isfile(it) and [l.split()[0] for l in open(it) if l.strip() and l[0] != '#'] == [s['id'] for s in inl])
for s in ported:
    if s not in inl and not os.path.isdir(os.path.join(DATA, 'st%d' % s['room'])): check('data_room_st%d' % s['room'], False, 'missing')

# ------------------------------------------------------------------------------------------- the first arc, new Adventure
ev, out = run('5 ADVNEW 0\n' + taps(60, 5200) + '5300 STORY\n', 5400)
check('new_adventure_opening_first', starts(ev)[:2] == ['adv_opening', 'dv_meet'], starts(ev))
check('new_adventure_each_scene_once', all(starts(ev).count(x) == 1 for x in ('adv_opening', 'dv_meet')), starts(ev))
check('new_adventure_chsel_lilac', any(l == 'story chsel 0' for f, l in ev), [l for f, l in ev if 'chsel' in l])
d = dumps(ev)
check('new_adventure_reaches_dv1', d and d[-1]['lv'] == 'dv1' and d[-1]['mode'] == '1' and d[-1]['chr'] == '0' and d[-1]['gate'] == '1',
      d[-1] if d else out[-300:])
check('opening_lines_none_meet_lines', [l for f, l in ev if l.startswith('csx say')][:1] == ['csx say 7 seg 1'] or
      any(l.startswith('csx say 7 ') for f, l in ev), [l for f, l in ev if l.startswith('csx say')][:3])
# the character select as Carol (the cursor starts on the character the run started with)
ev, out = run('5 ADVNEW 2\n' + taps(60, 5200) + '5300 STORY\n', 5400)
d = dumps(ev)
check('chsel_carol', any(l == 'story chsel 2' for f, l in ev) and d and d[-1]['lv'] == 'dv1' and d[-1]['chr'] == '2', d[-1] if d else '')
# from the real menus (frontend.c HUB_EXIT_NEW_ADVENTURE: title -> START GAME -> file -> ADVENTURE)
ADV = '40 42 S\n200 201 J\n230 231 J\n260 261 J\n'
ev, out = run(ADV + '400 STORY\n', 420)
check('menu_adventure_starts_opening', starts(ev)[:1] == ['adv_opening'] and dumps(ev) and dumps(ev)[-1]['mode'] == '5', starts(ev))

# ------------------------------------------------------------------------------------- the end of Dragon Valley (frame 8)
POST = '5 ADVNEW 0\n10 LOAD dv4\n20 STORY\n30 STAGEEND\n'
ev, out = run(POST + taps(60, 9000) + '9100 STORY\n', 9200)
check('dv_end_chain_order', starts(ev)[1:] == ['dv_crash', 'dv_crash2', 'rm_gong'], starts(ev))
check('dv_end_chain_completions_once', [c for c in completes(ev) if c[0] != 'adv_opening'] == [('dv_crash', 0), ('dv_crash2', 0), ('rm_gong', 0)],
      completes(ev))
d = dumps(ev)
check('dv_end_chain_reaches_rm1', d and d[-1]['lv'] == 'rm1' and d[-1]['mode'] == '1' and d[-1]['run'] == '1', d[-1] if d else '')
check('dv_end_scene_lines', sum(1 for f, l in ev if l.startswith('csx say')) >= 50, sum(1 for f, l in ev if l.startswith('csx say')))
for ch in (2, 4):                                  # RV a75_g1_carol / a75_g1_milla: the same scenes for every g1
    ev2, _ = run(POST.replace('ADVNEW 0', 'ADVNEW %d' % ch) + taps(60, 9000) + '9100 STORY\n', 9200)
    check('dv_end_chain_chr%d' % ch, starts(ev2)[1:] == ['dv_crash', 'dv_crash2', 'rm_gong'] and dumps(ev2)[-1]['lv'] == 'rm1', starts(ev2))

# -------------------------------------------------------------------------------------------------------- gating
ev, out = run('5 MODE 1\n10 LOAD dv4\n30 STAGEEND\n60 STORY\n', 80)
check('classic_stage_end_no_scene', not starts(ev) and 'story stageend 2' in out and dumps(ev)[-1]['lv'] == 'rm1', starts(ev))
ev, out = run('5 MODE 0\n10 LOAD dv4\n30 STAGEEND\n60 STORY\n', 80, {'FP_TA': '1'})
check('time_attack_stage_end_no_scene', not starts(ev) and 'story stageend 0' in out, starts(ev))
ev, out = run('5 MODE 2\n10 LOAD dv4\n30 STAGEEND\n60 STORY\n', 80)
check('stale_adventure_flag_no_scene', not starts(ev) and dumps(ev)[-1]['gate'] == '0', 'fe_mode 2 without an Adventure run')
CLASSIC = '40 42 S\n200 201 J\n230 231 J\n260 261 D\n280 281 J\n380 381 J\n'
ev, out = run(CLASSIC + '700 STORY\n', 720)
check('menu_classic_no_scene', not starts(ev) and dumps(ev)[-1]['lv'] == 'dv1' and dumps(ev)[-1]['gate'] == '0', starts(ev))
TA = '40 42 S\n200 201 D\n220 221 J\n330 331 J\n470 471 J\n490 491 D\n520 521 J\n630 631 J\n'
ev, out = run(TA + '900 STORY\n', 920)
check('menu_time_attack_no_scene', not starts(ev) and dumps(ev)[-1]['gate'] == '0', dumps(ev)[-1] if dumps(ev) else '')
# Classic stage: pause RESTART and a death restart never reach the story
ev, out = run(CLASSIC + '700 701 S\n715 716 D\n730 731 J\n900 STAGEEND\n920 STORY\n', 940)
check('classic_restart_then_end_no_scene', not starts(ev) and dumps(ev)[-1]['gate'] == '0', starts(ev))
# an Adventure run, pause QUIT to the menu (dv_meet panel 1 row 2), then a Classic game and its stage end: no scene
QUIT = '5 ADVNEW 0\n' + '2200 2201 S\n2215 2216 D\n2225 2226 D\n2240 2241 J\n2320 STORY\n'
ev, out = run(QUIT + '2400 MODE 1\n2410 LOAD dv4\n2430 STAGEEND\n2460 STORY\n', 2480)
d = dumps(ev)
check('quit_to_menu_ends_run', d and d[0]['mode'] == '0' and d[0]['run'] == '0' and 'story reset' in out, d[0] if d else '')
check('after_quit_classic_no_scene', starts(ev) == ['adv_opening', 'dv_meet'] and d[-1]['lv'] == 'rm1' and d[-1]['gate'] == '0' and
      not [c for c in completes(ev) if c[0] != 'adv_opening'], (starts(ev), completes(ev), d[-1]['lv']))

# ------------------------------------------------------------------------------------------------------------ pause
# resume: START opens (sound 63), START again closes; the scene goes on and finishes
ev, out = run(POST + taps(60, 290) + '300 301 S\n310 STORY\n340 341 S\n350 STORY\n' + taps(380, 9000) + '9100 STORY\n', 9200)
d = dumps(ev)
check('pause_opens', d[1]['paused'] == '1' and d[1]['scene'] == 'dv_crash', d[1])
check('pause_resume_goes_on', d[2]['paused'] == '0' and starts(ev)[1:] == ['dv_crash', 'dv_crash2', 'rm_gong'] and d[-1]['lv'] == 'rm1', starts(ev))
# SKIP SCENE (row 1) in dv_crash: RV k75_g1_skip -> frame 14 g4 11 (dv_crash2); completion once, then the next scene
SKIP = POST + '300 301 S\n330 331 D\n360 361 J\n440 STORY\n'
ev, out = run(SKIP, 460)
c = completes(ev)
check('skip_scene_completion_once', [x for x in c if x[0] == 'dv_crash'] == [('dv_crash', 1)], c)
check('skip_scene_next_state', starts(ev)[-1] == 'dv_crash2' and dumps(ev)[-1]['scene'] == 'dv_crash2', starts(ev))
check('skip_scene_stops_audio', any(l.startswith('music stop') for f, l in ev if 360 < f < 430) or 'snd stopall' in out)
check('skip_scene_seen_flag', int(dumps(ev)[-1]['seen'], 16) >> 2 & 1 == 1, dumps(ev)[-1]['seen'])
# NEXT STAGE (row 2, panel 2): RV k75_g1_row2 -> RM1, skipping the rest of the chain
ev, out = run(POST + '300 301 S\n330 331 D\n345 346 D\n360 361 J\n440 STORY\n', 460)
check('next_stage_row', completes(ev)[-1] == ('dv_crash', 2) and dumps(ev)[-1]['lv'] == 'rm1' and starts(ev)[-1] == 'dv_crash', completes(ev))
# SKIP SCENE in dv_meet: RV k75_g0_skip700 -> the scene jumps to its character select (no completion)
ev, out = run('5 ADVNEW 0\n2200 2201 S\n2215 2216 D\n2230 2231 J\n2300 STORY\n2310 2311 J\n2400 STORY\n' + taps(2420, 2700, 30) + '2800 STORY\n', 2820)
d = dumps(ev)
check('meet_skip_to_chsel', d[0]['chsel'] == '1' and d[0]['scene'] == 'dv_meet' and not [x for x in completes(ev) if x[0] == 'dv_meet'][:0], d[0])
check('meet_skip_then_dv1', any(l == 'story chsel 0' for f, l in ev) and d[-1]['lv'] == 'dv1', d[-1])
# START in the opening (panel 0): RV k81_g2_start -> frame 75 g4 0 at once
ev, out = run('5 ADVNEW 0\n200 201 S\n260 STORY\n', 280)
check('opening_start_skips', completes(ev)[:1] == [('adv_opening', 1)] and starts(ev) == ['adv_opening', 'dv_meet'], completes(ev))
# no double completion: SKIP pressed while the scene would end anyway, and two confirms
ev, out = run(SKIP.replace('360 361 J', '360 361 J\n362 363 J\n364 365 S'), 460)
check('no_double_completion', [x for x in completes(ev) if x[0] == 'dv_crash'] == [('dv_crash', 1)] and starts(ev).count('dv_crash2') == 1, completes(ev))

# ------------------------------------------------------------------------------------------------------------ save
with tempfile.TemporaryDirectory() as td:
    sv = os.path.join(td, 'save.bin')
    # a used Adventure file (slot 1): the post-DV chain with a file; its completions write position and seen flags
    env = {'FP_SAVE': sv, 'FP_FILE': '1'}
    run('600 TP 16000 2280\n620 1300 R\n', 1100, dict(env, FP_LEVEL='dv4', FP_FILE_NEW='1'))   # DV4's results make slot 1 used
    ev, out = run('5 MODE 2\n6 ADVNEW 0\n10 LOAD dv4\n30 STAGEEND\n300 301 S\n330 331 D\n360 361 J\n440 STORY\n', 460, env)
    d = dumps(ev)
    check('save_story_position', d and d[-1]['pos'] == str(ARC.index('dv_crash') + 1 + 0), d[-1] if d else '')
    ev, out = run('5 STORY\n', 10, env)
    d = dumps(ev)
    check('save_seen_read_back', d and int(d[0]['seen'], 16) >> 2 & 1 == 1, d[0] if d else '')
    raw = open(sv, 'rb').read()
    check('save_v5_size', raw[3] == 5 and len(raw) == 1377, '%d bytes v%d' % (len(raw), raw[3]))

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
