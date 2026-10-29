#!/usr/bin/env python3
"""Adventure mode, phase 2 groups C1 / C2 (the scenes of json frame 83, docs/ADVENTURE.md section 10) on the host build.
- inventory: each scene of the groups is ported (script, split story room st83<list> with its views and rows, story.txt) or
  says why not ('missing'); g4, triggers, next / SKIP SCENE / NEXT STAGE targets and characters are the original's (RE
  CutsceneList K -> frame 82 / g4 events 262-298, frame 83's pause rows 22337-22370, frame 8; RV build/orig_traces/adv3)
- the chains play in order, each scene exactly once, for Lilac, Carol and Milla, into the original's next scene or stage
- the stage-end triggers (frame 8): Jade Creek, Pangu Lagoon; never in Classic or Time Attack
- pause: SKIP SCENE and NEXT STAGE of every ported scene: one completion, the original's target, audio stopped
- save: the completions write the story position and seen flags into a used file
usage: test_adventure_arc3.py [data_dir]   (default: converted2; needs tools/import_story.py's output)"""
import json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
TAB = os.path.join(ROOT, 'tables', 'story')
C1 = ['st_bad_news', 'sg_brevon_plan', 'sm_zao_dinner', 'sg_intruders', 'sm_zao_help', 'st_magister_talk']
C2 = ['jail_plan', 'jail_fail', 'throne_apology', 'sg_brevon_torque', 'jc_cave', 'th_carol_help', 'tb_arrest', 'tb_outside_team',
      'st_mission', 'pl_final_message']
VARIANT = {'jc_cave_cm': 'jc_cave'}           # a scene's g1 variant (same g4, its own entry): the parent's CutsceneList index
# the original (RE / RV): g4 in frame 82 (CutsceneList K = list + 1 -> events 268-295), the step after the scene (RV), SKIP SCENE
# (frame 83's pause row A 3) and NEXT STAGE (A 4) targets, triggers (frame 8), characters (g1 mask)
EXPECT = {
    'st_bad_news':      dict(g4=0, next='scene:th_torque_revealed', skip='scene:th_torque_revealed', stage='fn1', trig=[], chr=7),
    'sg_brevon_plan':   dict(g4=1, next='scene:th_next_morning', skip='scene:th_next_morning', stage='fn1', trig=[], chr=7),
    'sm_zao_dinner':    dict(g4=4, next='scene:sg_intruders', skip='scene:sg_intruders', stage='sb1', trig=[], chr=7),
    'sg_intruders':     dict(g4=5, next='scene:sm_zao_help', skip='scene:sm_zao_help', stage='sb1', trig=[], chr=7),
    'sm_zao_help':      dict(g4=6, next='scene:zao_airship', skip='stage:sb1', stage='sb1', trig=[], chr=7),
    'st_magister_talk': dict(g4=8, next='scene:jail_plan', skip='scene:jail_plan', stage='jc1', trig=[], chr=7),
    'jail_plan':        dict(g4=9, next='scene:sg_brevon_torque', skip='scene:jail_fail', stage='jc1', trig=[], chr=7),
    'sg_brevon_torque': dict(g4=12, next='scene:jail_fail', skip='scene:jail_fail', stage='jc1', trig=[], chr=7),
    'jail_fail':        dict(g4=10, next='scene:throne_apology', skip='scene:throne_apology', stage='jc1', trig=[], chr=7),
    'throne_apology':   dict(g4=11, next='stage:jc1', skip='stage:jc1', stage='jc1', trig=[], chr=7),
    'jc_cave':          dict(g4=13, next='stage:tb1', skip='stage:tb1', stage='tb1', trig=['end:5'], chr=1),
    'jc_cave_cm':       dict(g4=13, next='scene:th_carol_help', skip='scene:th_carol_help', stage='th1', trig=['end:5'], chr=6),
    'th_carol_help':    dict(g4=14, next='stage:th1', skip='stage:th1', stage='th1', trig=[], chr=6),
    'tb_arrest':        dict(g4=17, next='scene:st_mission', skip='scene:st_mission', stage='pl1', trig=[], chr=1),
    'st_mission':       dict(g4=19, next='stage:pl1', skip='stage:pl1', stage='pl1', trig=[], chr=1),
    'pl_final_message': dict(g4=20, next='scene:sf_army', skip='stage:bg1', stage='bg1', trig=['end:7'], chr=1),
}

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
        if l.startswith(('story ', 'csx ', 'music ')): ev.append((fr, l))
    return ev, out

def taps(a, b, step=15, btn='J'):
    return ''.join('%d %d %s\n' % (f, f + 1, btn) for f in range(a, b, step))
def starts(ev): return [l.split()[2] for f, l in ev if l.startswith('story start ')]
def completes(ev): return [(l.split()[2], int(l.split()[4])) for f, l in ev if l.startswith('story complete ')]
def dumps(ev): return [dict(zip(l.split()[1::2], l.split()[2::2])) for f, l in ev if l.startswith('story run ')]

# ------------------------------------------------------------------------------------------------------------ inventory
sc = {s['id']: s for s in json.load(open(os.path.join(TAB, 'scenes.json'), encoding='utf-8'))['scenes']}
rows = json.load(open(os.path.join(TAB, 'rows.json'), encoding='utf-8'))
table = [l.split() for l in open(os.path.join(DATA, 'story', 'story.txt'), encoding='utf-8') if l.strip() and l[0] != '#'] \
    if os.path.isfile(os.path.join(DATA, 'story', 'story.txt')) else []
tab = {t[0]: t for t in table}
ported = [i for i in C1 + C2 + list(VARIANT) if os.path.isfile(os.path.join(TAB, 'scripts', i + '.scn'))]
check('inventory_groups_known', all(i in sc for i in C1 + C2), [i for i in C1 + C2 if i not in sc])
check('inventory_unported_say_why', all(sc[i].get('missing') for i in C1 + C2 if i not in ported),
      [i for i in C1 + C2 if i not in ported and not sc[i].get('missing')])
check('inventory_expectations_ported', all(i in ported for i in EXPECT), [i for i in EXPECT if i not in ported])
for i in ported:
    s, r = sc[i], rows.get(i, {})
    hdr = [l.split() for l in open(os.path.join(TAB, 'scripts', i + '.scn'), encoding='utf-8') if l[:5] in ('room ', 'scene')]
    lst = sc[VARIANT[i]]['list'] if i in VARIANT else s['list']
    check('room_split_%s' % i, s.get('frame') == 83 and s['room'] // 100 == 83 and s['room'] % 100 == lst and r.get('json') == s['room']
          and r.get('frame') == 83 and r.get('view') and ['room', 'st%d' % s['room']] in hdr, (s['room'], s.get('frame'), r.get('json')))
    check('data_room_%s' % i, os.path.isfile(os.path.join(DATA, 'st%d' % s['room'], 'st%d.lv2' % s['room'])), 'tools/import_story.py')
    check('data_table_%s' % i, i in tab and tab[i][1] == str(s['room']) and tab[i][2] == str(s['g4']), tab.get(i))
    x = EXPECT.get(i)
    if x:
        got = dict(g4=s['g4'], next=s['next'], skip=s.get('skip', s['next']), stage=s.get('stage'), trig=s.get('triggers', []), chr=s.get('chr', 7))
        check('original_steps_%s' % i, got == x, {k: (got[k], x[k]) for k in x if got[k] != x[k]})

def step_of(i, k):
    return EXPECT[i][k]
def expect_after(ev, i, step):
    """the run's state after scene i completed into step (scene:<id> starts if that scene is ported, else the story resets)"""
    kind, arg = step.split(':')
    if kind == 'stage':
        d = dumps(ev)
        return bool(d) and d[-1]['lv'] == arg and d[-1]['mode'] == '1' and d[-1]['run'] == '1'
    st = [sc.get(x, {}).get('sid_of', x) for x in starts(ev)]   # (a character variant of groups A / B, sid_of: its base scene)
    if arg in tab: return i in st and st.index(i) + 1 < len(st) and st[st.index(i) + 1] == arg
    # not ported in this data (another group's scene): story.c's step names no scene, the run ends (story_reset, menu)
    ls = [l for f, l in ev if l.startswith('story ') and not l.startswith('story next ')]   # (the host's step line, session 25b)
    k = next((n for n, l in enumerate(ls) if l.startswith('story complete %s ' % i)), None)
    return k is not None and k + 1 < len(ls) and ls[k + 1] == 'story reset'

# ------------------------------------------------------------------------------------------------- the chains, each scene once
def chain(first, order, frames, ch):
    ev, out = run('3 MODE 2\n4 ADVNEW %d\n5 SCENE %s\n' % (ch, first) + taps(60, frames - 100) + '%d STORY\n' % (frames - 20), frames,
                  {'FP_CHAR': str(ch)} if ch else None)
    # (ADVNEW starts the opening: SCENE replaces it at once)
    st = [x for x in starts(ev) if x != 'adv_opening' and x != 'dv_meet']
    return ev, st
for ch in (0, 2, 4):
    if all(i in ported for i in ('st_magister_talk', 'jail_plan', 'sg_brevon_torque', 'jail_fail', 'throne_apology')):
        order = ['st_magister_talk', 'jail_plan', 'sg_brevon_torque', 'jail_fail', 'throne_apology']
        ev, st = chain('st_magister_talk', order, 7000, ch)
        c = [x for x in completes(ev) if x[0] in order]
        d = dumps(ev)
        check('jail_chain_order_chr%d' % ch, st == order, st)
        check('jail_chain_once_chr%d' % ch, c == [(x, 0) for x in order], c)
        check('jail_chain_to_jc1_chr%d' % ch, d and d[-1]['lv'] == 'jc1' and d[-1]['mode'] == '1' and d[-1]['gate'] == '1', d[-1] if d else '')
for ch in (0, 2, 4):
    order = ['sm_zao_dinner', 'sg_intruders', 'sm_zao_help']
    if all(i in ported for i in order):
        ev, st = chain('sm_zao_dinner', order, 4500, ch)
        c = [x for x in completes(ev) if x[0] in order]
        check('zao_chain_order_chr%d' % ch, st[:3] == order, st)
        check('zao_chain_once_chr%d' % ch, c == [(x, 0) for x in order], c)
        check('zao_chain_next_chr%d' % ch, expect_after(ev, 'sm_zao_help', EXPECT['sm_zao_help']['next']), st)
for i in ('st_bad_news', 'sg_brevon_plan'):
    if i in ported:
        ev, st = chain(i, [i], 3000, 0)
        check('single_%s_once' % i, [x for x in completes(ev) if x[0] == i] == [(i, 0)], completes(ev))
        check('single_%s_next' % i, expect_after(ev, i, step_of(i, 'next')), (starts(ev), completes(ev)))

# ------------------------------------------------------------- the end of Jade Creek (frame 8, I 5 -> frame 82 g4 13 by g1)
POST = '3 MODE 2\n4 ADVNEW %d\n10 LOAD jc5\n20 STORY\n30 STAGEEND\n'
for ch, order, lv in ((0, ['jc_cave'], 'tb1'), (2, ['jc_cave_cm', 'th_carol_help'], 'th1'), (4, ['jc_cave_cm', 'th_carol_help'], 'th1')):
    if all(i in ported for i in order):
        ev, out = run(POST % ch + taps(60, 3600) + '3700 STORY\n', 3720, {'FP_CHAR': str(ch)} if ch else None)
        st = [x for x in starts(ev) if x not in ('adv_opening', 'dv_meet')]
        d = dumps(ev)
        check('jc_end_chain_chr%d' % ch, st == order and [x for x in completes(ev) if x[0] in order] == [(x, 0) for x in order], (st, completes(ev)))
        check('jc_end_next_stage_chr%d' % ch, d and d[-1]['lv'] == lv and d[-1]['mode'] == '1' and d[-1]['chr'] == str(ch), d[-1] if d else '')
# the end of Pangu Lagoon (frame 8, I 7 with g1 0 -> frame 82 g4 20)
if 'pl_final_message' in ported:
    ev, out = run('3 MODE 2\n4 ADVNEW 0\n10 LOAD pl6\n20 STORY\n30 STAGEEND\n' + taps(60, 3200) + '3300 STORY\n', 3320)
    st = [x for x in starts(ev) if x not in ('adv_opening', 'dv_meet')]
    check('pl_end_scene', st[:1] == ['pl_final_message'] and [x for x in completes(ev) if x[0] == 'pl_final_message'] == [('pl_final_message', 0)], (st, completes(ev)))
    check('pl_end_next', expect_after(ev, 'pl_final_message', EXPECT['pl_final_message']['next']), st)
# a step into a g4 whose content depends on g1 names one variant: the character's own plays (story.c chr_variant)
for named, ch, want in (('jc_cave', 2, 'jc_cave_cm'), ('jc_cave_cm', 0, 'jc_cave'), ('tb_arrest', 2, 'tb_outside_team'),
                        ('tb_outside_team', 0, 'tb_arrest'), ('tb_arrest', 4, 'tb_outside_team')):
    if named in tab and want in tab:
        ev, out = run('3 MODE 2\n4 ADVNEW %d\n5 SCENE %s\n40 STORY\n' % (ch, named), 60, {'FP_CHAR': str(ch)} if ch else None)
        check('variant_%s_chr%d' % (named, ch), [x for x in starts(ev) if x != 'adv_opening'][:1] == [want], starts(ev))

# ------------------------------------------------------------------------------------------------------------ pause
# START, then row 1 (SKIP SCENE) or row 2 (NEXT STAGE), A; the 50-frame wipe, then the completion (RE 22337-22370)
for i in [x for x in ported if x in EXPECT]:
    for row, how, key in ((1, 1, 'skip'), (2, 2, 'stage')):
        keys = '300 301 S\n330 331 D\n' + ('345 346 D\n' if row == 2 else '') + '360 361 J\n362 363 J\n364 365 S\n'
        ch = 0 if EXPECT[i]['chr'] & 1 else 2
        ev, out = run('3 MODE 2\n4 ADVNEW %d\n5 SCENE %s\n' % (ch, i) + keys + '440 STORY\n', 460, {'FP_CHAR': str(ch)} if ch else None)
        c = [x for x in completes(ev) if x[0] == i]
        tgt = EXPECT[i][key] if key == 'skip' else 'stage:' + EXPECT[i]['stage']
        check('pause_%s_%s_once' % (key, i), c == [(i, how)], completes(ev))
        check('pause_%s_%s_target' % (key, i), expect_after(ev, i, tgt), (tgt, starts(ev), dumps(ev)[-1] if dumps(ev) else ''))
        check('pause_%s_%s_audio_stopped' % (key, i), any(l.startswith('music stop') for f, l in ev if 360 < f < 430) or 'snd stopall' in out)

# -------------------------------------------------------------------------------------------------------- gating
for lv in ('jc5', 'pl6', 'th3'):
    ev, out = run('5 MODE 1\n10 LOAD %s\n30 STAGEEND\n60 STORY\n' % lv, 80)
    check('classic_%s_end_no_scene' % lv, not starts(ev) and dumps(ev) and dumps(ev)[-1]['gate'] == '0', starts(ev))
    ev, out = run('5 MODE 0\n10 LOAD %s\n30 STAGEEND\n60 STORY\n' % lv, 80, {'FP_TA': '1'})
    check('time_attack_%s_end_no_scene' % lv, not starts(ev), starts(ev))

# ------------------------------------------------------------------------------------------------------------ save
if 'jail_plan' in ported:
    with tempfile.TemporaryDirectory() as td:
        sv = os.path.join(td, 'save.bin')
        env = {'FP_SAVE': sv, 'FP_FILE': '1'}
        run('600 TP 16000 2280\n620 1300 R\n', 1100, dict(env, FP_LEVEL='dv4', FP_FILE_NEW='1'))   # DV4's results make slot 1 used
        ev, out = run('5 MODE 2\n6 ADVNEW 0\n10 SCENE jail_plan\n300 301 S\n330 331 D\n360 361 J\n440 STORY\n', 460, env)
        # the saved position and the seen flag use the scene's permanent number (tables/story/sids.json), not its table row
        pos = json.load(open(os.path.join(HERE, '..', 'tables', 'story', 'sids.json'), encoding='utf-8'))['sid']['jail_plan'] + 1
        d = dumps(ev)
        check('save_story_position', d and d[-1]['pos'] == str(pos), d[-1] if d else '')
        ev, out = run('5 STORY\n', 10, env)
        d = dumps(ev)
        check('save_seen_read_back', d and int(d[0]['seen'], 16) >> (pos - 1) & 1 == 1, d[0] if d else '')

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
