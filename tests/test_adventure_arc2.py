#!/usr/bin/env python3
"""Adventure mode, phase 2 groups A + B (session 25b, docs/ADVENTURE.md section 10) on the host build:
- A, the Relic Maze aftermath (room 77): frame 8 after RM (I 2) starts Meet Milla for Lilac and Cave Collapse for Carol /
  Milla; after Aqua Tunnel (I 0) Milla's Hideout for Milla only; RM5's in-stage Adventure dialogue (Lilac / Carol)
- B, Treehouse -> Fortune Night (rooms 16, 70, 15, 78, 79, 36): Torque Revealed (three rooms) -> [Brevon's Plan, group C1]
  -> The Next Morning -> Gong's Help -> Spade Encounter -> FN1; frame 8 after FN (I 3): Robopanther Defeated ->
  [group C1]; Zao's Airship (room 79, then SB1's frame with g4 99) -> [the ambush, room 83 g4 7]
- each scene plays once, in order, for g1 0 / 2 / 4 where it plays; SKIP SCENE / NEXT STAGE go where the original goes
  (RE pause events per room); never outside an Adventure run; the story position / seen flags are saved
- sound RAM: a scene's set (the ids it plays that are not resident) plus the two voice slots stay <= 200 KB
A step into a scene of another group (st_bad_news, sg_brevon_plan, sm_zao_dinner, zao_airship_ambush) is checked as the
table's step; when that scene is in the data it must start.
usage: test_adventure_arc2.py [data_dir]   (default: converted2; tools/import_story.py + convert_audio.py story output)"""
import json, os, re, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
TAB = os.path.join(ROOT, 'tables', 'story')
MINE = ['rm_hideout', 'rm_meet_milla', 'rm_collapse', 'th_torque_revealed', 'th_torque_revealed2', 'th_torque_revealed3',
        'th_next_morning', 'np_gong_help', 'sm_spade', 'fn_panther_defeated', 'zao_airship', 'zao_airship2']
BUDGET = 200 * 1024

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

def taps(a, b, step=15, btn='J'): return ''.join('%d %d %s\n' % (f, f + 1, btn) for f in range(a, b, step))
def base(n): return n[:-2] if n[-2:] in ('_c', '_m') else n   # a character's variant of a scene (story.c variant())
def starts(ev, raw=False): return [(n if raw else base(n)) for n in (l.split()[2] for f, l in ev if l.startswith('story start ')) if n not in ('adv_opening', 'dv_meet')]
def completes(ev, raw=False): return [((l.split()[2] if raw else base(l.split()[2])), int(l.split()[4])) for f, l in ev
                                      if l.startswith('story complete ') and l.split()[2] not in ('adv_opening', 'dv_meet')]
def nexts(ev): return [l.split(None, 2)[2] for f, l in ev if l.startswith('story next ')]
def dumps(ev): return [dict(zip(l.split()[1::2], l.split()[2::2])) for f, l in ev if l.startswith('story run ')]
def sfx(ev): return [int(l.split()[-1]) for f, l in ev if l.startswith('story sfx ')]
def vopeak(ev): return [int(l.split()[-1]) for f, l in ev if l.startswith('story complete ')]

sc = {s['id']: s for s in json.load(open(os.path.join(TAB, 'scenes.json'), encoding='utf-8'))['scenes']}
table = [l.split() for l in open(os.path.join(DATA, 'story', 'story.txt'), encoding='utf-8') if l.strip() and l[0] != '#']
inl = os.path.join(DATA, 'story', 'inline.txt')
table += [l.split() for l in open(inl, encoding='utf-8') if l.strip() and l[0] != '#'] if os.path.isfile(inl) else []
intable = {t[0]: t for t in table}
ALL = [t[0] for t in table if t[0][:-2] in MINE or t[0] in MINE]       # my scenes and their character variants
def chr_of(i): m = sc[i].get('chr', 7); return 0 if m & 1 else 2 if m & 2 else 4
def after(cur, nxt):
    """the scene after cur: started when it is in the data, else the table names it and the run leaves to the menu"""
    return lambda ev: ((nxt in starts(ev) and cur in starts(ev) and starts(ev).index(nxt) > starts(ev).index(cur)) if nxt in intable
                       else 'scene:%s (not in the table)' % nxt in nexts(ev))

# ------------------------------------------------------------------------------------------------------------ inventory
for i in MINE:
    check('ported_%s' % i, os.path.isfile(os.path.join(TAB, 'scripts', i + '.scn')) and i in intable, 'script + story.txt')
for r in sorted({sc[i]['room'] for i in ALL if i in sc and sc[i].get('next') != 'back'}):
    check('room_st%d_built' % r, os.path.isfile(os.path.join(DATA, 'st%d' % r, 'st%d.lv2' % r)))
check('scene_sounds_converted', os.path.isdir(os.path.join(DATA, 'story', 'sfx')) and
      len([f for f in os.listdir(os.path.join(DATA, 'story', 'sfx')) if f.endswith('.wav')]) > 0, 'story/sfx (convert_audio.py story)')

# ------------------------------------------------------------------------------------- A: frame 8 after Relic Maze / AT
def stage_end(level, ch, frames=6000, extra=''):
    return run('5 ADVNEW %d\n10 LOAD %s\n20 STORY\n30 STAGEEND\n' % (ch, level) + taps(60, frames - 200) + extra + '%d STORY\n' % (frames - 100), frames)
ev, out = stage_end('rm5', 0, 5000)
check('rm_end_lilac_meet_milla', starts(ev)[:1] == ['rm_meet_milla'] and completes(ev)[:1] == [('rm_meet_milla', 0)], (starts(ev), completes(ev)))
check('meet_milla_then_bad_news', after('rm_meet_milla', 'st_bad_news')(ev), nexts(ev))
for ch in (2, 4):
    ev, out = stage_end('rm5', ch, 6000)
    check('rm_end_chr%d_collapse_then_meet_milla' % ch, starts(ev)[:2] == ['rm_collapse', 'rm_meet_milla'] and
          completes(ev)[:2] == [('rm_collapse', 0), ('rm_meet_milla', 0)], (starts(ev), completes(ev)))
    check('collapse_chr%d_variants' % ch, starts(ev, True)[:2] == ['rm_collapse' + ('_m' if ch == 4 else ''), 'rm_meet_milla_' + 'cm'[ch == 4]],
          starts(ev, True))
    check('collapse_chr%d_then_bad_news' % ch, after('rm_meet_milla', 'st_bad_news')(ev), nexts(ev))
ev, out = stage_end('at4', 4, 3000)
check('at_end_milla_hideout', starts(ev)[:1] == ['rm_hideout'] and completes(ev)[:1] == [('rm_hideout', 0)], (starts(ev), completes(ev)))
HIDEOUT_NEXT = sc['rm_hideout'].get('next', '')
check('hideout_next_step', HIDEOUT_NEXT in nexts(ev) or ('%s (not in the table)' % HIDEOUT_NEXT) in nexts(ev), (HIDEOUT_NEXT, nexts(ev)))

# --------------------------------------------------------------------------------- B: the chains, each scene once
def chain(first, ch, frames):
    return run('5 ADVNEW %d\n10 SCENE %s\n' % (ch, first) + taps(60, frames - 200) + '%d STORY\n' % (frames - 100), frames)
for ch in (0, 2, 4):
    ev, out = chain('th_torque_revealed', ch, 6000)
    s = starts(ev)
    tr = ['th_torque_revealed'] if ch == 4 else ['th_torque_revealed', 'th_torque_revealed2', 'th_torque_revealed3']   # RV b15_g11_m: Milla's has no flashback
    check('torque_revealed_rooms_chr%d' % ch, s[:len(tr)] == tr and [c[0] for c in completes(ev)][:len(tr)] == tr, s)
    check('torque_revealed_then_brevon_plan_chr%d' % ch, after(tr[-1], 'sg_brevon_plan')(ev), nexts(ev))
    ev, out = chain('th_next_morning', ch, 9000)
    d = dumps(ev)
    check('next_morning_to_fn1_chr%d' % ch, starts(ev) == ['th_next_morning', 'np_gong_help', 'sm_spade'] and
          [c[0] for c in completes(ev)] == ['th_next_morning', 'np_gong_help', 'sm_spade'] and d and d[-1]['lv'] == 'fn1' and d[-1]['mode'] == '1',
          (starts(ev), d[-1] if d else ''))
    ev, out = stage_end('fn6', ch, 4500)
    check('fn_end_panther_defeated_chr%d' % ch, starts(ev)[:1] == ['fn_panther_defeated'] and completes(ev)[:1] == [('fn_panther_defeated', 0)],
          (starts(ev), completes(ev)))
    check('panther_then_zao_dinner_chr%d' % ch, after('fn_panther_defeated', 'sm_zao_dinner')(ev), nexts(ev))
    ev, out = chain('zao_airship', ch, 6000)
    check('zao_airship_two_rooms_chr%d' % ch, starts(ev)[:2] == ['zao_airship', 'zao_airship2'] and
          [c[0] for c in completes(ev)][:2] == ['zao_airship', 'zao_airship2'], starts(ev))
    check('zao_airship_then_ambush_chr%d' % ch, after('zao_airship2', 'zao_airship_ambush')(ev), nexts(ev))

# ------------------------------------------------- the boot sound set survives a scene (session 25b fix: phase 1's scene sets
# named shared ids, 8 = the jump among them, and the next stage's set load freed them)
with tempfile.TemporaryDirectory() as td:
    miss = os.path.join(td, 'miss.txt')
    ev, out = run('5 ADVNEW 0\n10 SCENE sm_spade\n200 201 S\n230 231 D\n245 246 D\n260 261 J\n500 501 J\n560 561 J\n620 621 X\n700 STORY\n', 720,
                  {'FP_SID_MISS': miss, 'FP_TRACE_SID': '1'})
    m = open(miss).read().split() if os.path.exists(miss) else []
    d = dumps(ev)
    check('boot_sounds_after_scene', d and d[-1]['lv'] == 'fn1' and 'sid 8' in out and not [x for x in m if x in ('8', '21', '56')], m[:10])

# ------------------------------------------------------------------------------------------------------------ gating
for lv, ch in (('rm5', 0), ('rm5', 2), ('at4', 4), ('fn6', 0)):
    ev, out = run('5 MODE 1\n10 LOAD %s\n30 STAGEEND\n60 STORY\n' % lv, 80, {'FP_CHAR': str(ch)})
    check('classic_%s_end_no_scene' % lv, not starts(ev) and dumps(ev)[-1]['gate'] == '0', starts(ev))
    ev, out = run('5 MODE 0\n10 LOAD %s\n30 STAGEEND\n60 STORY\n' % lv, 80, {'FP_TA': '1', 'FP_CHAR': str(ch)})
    check('time_attack_%s_end_no_scene' % lv, not starts(ev), starts(ev))
ev, out = run('5 MODE 1\n10 SCENE th_next_morning\n60 STORY\n', 80)
check('classic_scene_command_refused', 'th_next_morning' not in starts(ev), starts(ev))

# ------------------------------------------------------------------------------------- pause: SKIP SCENE / NEXT STAGE
# (scene, its SKIP SCENE step, its NEXT STAGE level): RE pause events of each room (docs/ADVENTURE.md section 10)
PAUSE = [(i, sc[i].get('skip') or sc[i]['next'], sc[i].get('stage')) for i in ALL if sc[i].get('next') != 'back']
for i, sk, stg in PAUSE:
    ch = chr_of(i)
    pre = '5 ADVNEW %d\n10 SCENE %s\n' % (ch, i)
    if intable[i][4:] and 'panel=3' in intable[i]:          # no pause menu (RV k69_g99_skip): START, Down, A change nothing
        ev, out = run(pre + '200 201 S\n230 231 D\n260 261 J\n300 STORY\n', 320)
        d = dumps(ev)
        check('no_pause_%s' % i, d and d[-1]['paused'] == '0' and d[-1]['scene'] == i and not completes(ev, True), (d[-1] if d else '', completes(ev, True)))
        continue
    ev, out = run(pre + '200 201 S\n230 231 D\n260 261 J\n400 STORY\n', 420)
    c = [x for x in completes(ev, True) if x[0] == i]
    check('skip_%s' % i, c == [(i, 1)] and nexts(ev)[-1:] in ([sk], ['%s (not in the table)' % sk]), (c, nexts(ev), sk))
    if sk.startswith('scene:') and sk[6:] in intable:
        check('skip_%s_starts_next' % i, base(starts(ev, True)[-1]) == sk[6:], starts(ev, True))
    d = dumps(ev)
    check('skip_%s_seen' % i, d and int(d[-1]['seen'], 16) >> [t[0] for t in table].index(i) & 1 == 1, d[-1]['seen'] if d else '')
    if stg:
        ev, out = run(pre + '200 201 S\n230 231 D\n245 246 D\n260 261 J\n400 STORY\n', 420)
        d = dumps(ev)
        check('next_stage_%s' % i, [x for x in completes(ev, True) if x[0] == i] == [(i, 2)] and d and d[-1]['lv'] == stg and d[-1]['mode'] == '1',
              (completes(ev, True), d[-1]['lv'] if d else ''))
    ev, out = run(pre + '200 201 S\n230 231 D\n260 261 J\n262 263 J\n264 265 S\n400 STORY\n', 420)
    check('skip_%s_once' % i, [x for x in completes(ev, True) if x[0] == i] == [(i, 1)], completes(ev, True))

# ------------------------------------------------------------------------------------------------------------ save
with tempfile.TemporaryDirectory() as td:
    sv = os.path.join(td, 'save.bin')
    env = {'FP_SAVE': sv, 'FP_FILE': '1'}
    run('600 TP 16000 2280\n620 1300 R\n', 1100, dict(env, FP_LEVEL='dv4', FP_FILE_NEW='1'))   # DV4's results make slot 1 used
    ev, out = run('5 MODE 2\n6 ADVNEW 0\n10 SCENE sm_spade\n200 201 S\n230 231 D\n260 261 J\n400 STORY\n', 420, env)
    k = [t[0] for t in table].index('sm_spade')
    d = dumps(ev)
    check('save_position_after_sm_spade', d and d[-1]['pos'] == str(k + 1), d[-1] if d else '')
    ev, out = run('5 STORY\n', 10, env)
    d = dumps(ev)
    check('save_seen_sm_spade_read_back', d and int(d[0]['seen'], 16) >> k & 1 == 1, d[0] if d else '')

# ------------------------------------------------------------------------------------------- sound RAM per scene
# the scene's set (story sfx: the ids it plays that are not resident, from story/sfx) + the two voice slots (vopeak)
worst = []
for i in ALL:
    if sc[i].get('next') == 'back': continue                 # the in-stage scene: below
    for ch in (chr_of(i),):
        ev, out = run('5 ADVNEW %d\n10 SCENE %s\n' % (ch, i) + taps(60, 5800) + '5900 STORY\n', 6000)
        st_ = [l for f, l in ev if l.startswith(('story start ', 'story sfx '))]
        own = next((int(st_[k - 1].split()[-1]) for k in range(1, len(st_)) if st_[k].startswith('story start %s ' % i)
                    and st_[k - 1].startswith('story sfx ')), 0)   # the set loaded just before the scene's start line
        vp = [int(l.split()[-1]) for f, l in ev if l.startswith('story complete %s ' % i)]
        tot = own + (vp[0] if vp else 0)
        worst.append((tot, i, ch))
        check('sound_ram_%s_chr%d' % (i, ch), vp and tot <= BUDGET, 'set %d + voices %s = %d bytes' % (own, vp, tot))
# ------------------------------------------------------------------------- RM5's in-stage Adventure dialogue (6864)
RM5 = '5 ADVNEW %d\n10 LOAD rm5\n100 TP 2600 900\n110 400 R\n' + taps(420, 2000) + '2100 STORY\n'
for ch in (0, 2, 4):
    ev, out = run(RM5 % ch, 2200)
    d = dumps(ev); s_ = starts(ev, True)
    if ch == 4:                                              # g1 < 4 only (RE 6864): Milla gets the Classic scene
        check('rm5_inline_milla_classic', not s_ and d and d[-1]['mode'] == '1', s_)
        continue
    want = 'rm5_spade' + ('_c' if ch == 2 else '')
    check('rm5_inline_chr%d' % ch, s_ == [want] and completes(ev) == [('rm5_spade', 0)] and 'story next back' in out, (s_, completes(ev)))
    m = re.findall(r'f (\d+) x ([\d.]+) y ([\d.]+)', out)
    after_ = [(int(f), float(x), float(y)) for f, x, y in m if int(f) > [f_ for f_, l in ev if l.startswith('story complete')][0] + 2] if completes(ev) else []
    check('rm5_inline_back_to_stage_chr%d' % ch, d and d[-1]['mode'] == '1' and d[-1]['lv'] == 'rm5' and after_ and abs(after_[0][1] - 3453) < 1,
          after_[:1])
    st_ = [l for f, l in ev if l.startswith(('story start ', 'story sfx '))]
    own = next((int(st_[k - 1].split()[-1]) for k in range(1, len(st_)) if st_[k].startswith('story start %s ' % want)
                and st_[k - 1].startswith('story sfx ')), 0)
    vp = [int(l.split()[-1]) for f, l in ev if l.startswith('story complete %s ' % want)]
    worst.append((own + (vp[0] if vp else 0), want, ch))
    check('sound_ram_%s_chr%d' % (want, ch), vp and own + vp[0] <= BUDGET, 'set %d + voices %s' % (own, vp))
ev, out = run((RM5 % 0).replace('5 ADVNEW 0\n', '5 MODE 1\n'), 2200)
check('rm5_classic_no_inline', not starts(ev, True), starts(ev, True))
print('largest scene sound RAM: %s' % ((max(worst),) if worst else '-'))
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
