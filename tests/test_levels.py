#!/usr/bin/env python3
"""Host tests for session 13: the level table and the reusable systems (title cards per level / character / load order /
restart, Milla's hang animation, the Aqua Tunnel results backdrop, the object registry against the importer, switches,
checkpoints). Frame-exact behaviour stays with the trace comparers (tools/orig/cmp_*); these guard the integration.
usage: test_levels.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile, glob, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))

def run(script, frames, env, dump=None, every=1):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); e.pop('FP_LEVEL', None); e.pop('FP_CHAR', None); e.update(env)
    args = [BIN, DATA, str(frames), name] + ([dump, str(every)] if dump else [])
    r = subprocess.run(args, capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows = []
    for line in r.stdout.splitlines():
        t = line.split()
        if t and t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
    return rows, r.stdout + '\n' + r.stderr

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- title cards (RE spec_stage_cards): the bank's card images per level. LevelCardIntro2 frame 3 "Aqua Tunnel" is
# 168x39, frame 0 "Dragon Valley" 166x41; LevelCardIntro3 "STAGE 1" 100x16 (both stages are STAGE 1 on their routes)
CARD = {'at': 'stage 100x16 name 168x39', 'dv': 'stage 100x16 name 166x41'}
def cards(out): return [l.split(' ', 2)[1:] for l in out.splitlines() if l.startswith('card ')]
for ch, nm in (('0', 'lilac'), ('2', 'carol'), ('4', 'milla')):
    for lv in ('dv1', 'at1'):
        _, out = run('', 5, {'FP_LEVEL': lv, 'FP_CHAR': ch, 'FP_CARD_DBG': '1'})
        c = cards(out)
        check('card_%s_%s' % (lv, nm), c == [[lv, CARD[lv[:2]]]], str(c))
# load orders in one run (state reuse): AT -> DV -> AT and DV -> AT -> DV
for first, seq in (('at1', ['dv1', 'at1']), ('dv1', ['at1', 'dv2', 'at3'])):
    script = ''.join('%d LOAD %s\n' % (100 * (k + 1), d) for k, d in enumerate(seq))
    _, out = run(script, 100 * len(seq) + 20, {'FP_LEVEL': first, 'FP_CHAR': '4', 'FP_CARD_DBG': '1'})
    c = cards(out)
    want = [[d, CARD[d[:2]]] for d in [first] + seq]
    check('card_load_order_%s_%s' % (first, '_'.join(seq)), c == want, str(c))
# restart from the pause menu reloads the act and shows its card again (RE 4881 -> loop_restart)
_, out = run('200 201 S\n210 211 D\n230 231 J\n', 320, {'FP_LEVEL': 'at1', 'FP_CHAR': '4', 'FP_CARD_DBG': '1'})
c = cards(out)
check('card_after_restart_at1', c == [['at1', CARD['at']]] * 2, str(c))
# an act change has no card (RE 4526 / 5047) but the next act's bank still holds its own card
_, out = run('5 TP 12400 1170\n10 300 R\n', 320, {'FP_LEVEL': 'at1', 'FP_CHAR': '4', 'FP_CARD_DBG': '1'})
c = cards(out)
check('card_act_change_at1_at2', [x[0] for x in c] == ['at1', 'at2'] and c[1][1] == CARD['at'], str(c))

# ---- Milla's hang (RE 2461: OTHERS.Y 41 -> her skin animation 15); Lilac and Carol keep their own hang animations
def hang_rows(ch):
    rows, _ = run('60 TP 5615 1000\n', 140, {'FP_LEVEL': 'dv1', 'FP_CHAR': ch})
    idle = rows[40]['an']; hang = [r for r in rows if r['st'] == 11]
    return idle, hang[2:]   # the first frames of the state still end the previous animation
idle, hang = hang_rows('4')
check('milla_hang_uses_hang_anim', bool(hang) and all(r['oy'] == 41 for r in hang) and all(r['an'] != idle for r in hang),
      'idle an %s, hang an %s oy %s' % (idle, sorted({r['an'] for r in hang}), sorted({r['oy'] for r in hang})))
for ch, nm in (('0', 'lilac'), ('2', 'carol')):
    idle, hang = hang_rows(ch)
    check('%s_hang_anim_kept' % nm, bool(hang) and len({r['an'] for r in hang}) == 1 and hang[0]['an'] != idle, 'idle %s hang %s' % (idle, sorted({r['an'] for r in hang})))

# ---- results backdrop: the clear card's panels are drawn in Aqua Tunnel too (AT4 snake defeat) and in DV4
def results_panel_delta(env, script, frames):
    d = tempfile.mkdtemp()
    try:
        rows, _ = run(script, frames, env, dump=d, every=10)
        on = [int(r['f']) for r in rows if r['res'] >= 3]
        if not on: return None
        f1 = (on[0] + 40) // 10 * 10; f0 = max(0, (on[0] - 200) // 10 * 10)
        from PIL import Image, ImageChops, ImageStat
        a = Image.open(os.path.join(d, 'f%05d.ppm' % f1)).crop((120, 80, 310, 135)); b = Image.open(os.path.join(d, 'f%05d.ppm' % f0)).crop((120, 80, 310, 135))
        return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3
    finally: shutil.rmtree(d)
dv = results_panel_delta({'FP_LEVEL': 'dv4'}, '5 TP 16000 2280\n20 700 R\n', 520)
at = results_panel_delta({'FP_LEVEL': 'at4', 'FP_CHAR': '4'}, '60 TP 4952 254\n60 SETV others E 99999\n950 BOSSHP 0\n', 1700)
check('results_panel_drawn_dv4', dv is not None and dv > 20, str(dv))
check('results_panel_drawn_at4', at is not None and at > 20, str(at))

# ---- object registry (src/game/objdefs.c) covers every engine type the importer can place
from import_frame import ENGINE_TYPES, ENGINE_TYPES_ID
_, out = run('', 1, {'FP_DUMP_OBJDEFS': '1'})
defined = {int(l.split()[1]) for l in out.splitlines() if l.startswith('grp ') and l.split()[7] == '1'}
missing = sorted((set(ENGINE_TYPES.values()) | set(ENGINE_TYPES_ID.values())) - defined)
check('registry_has_every_importer_type', not missing, str(missing))

# ---- level table: every act chains to the next of its stage and the last one ends in the results
for lv, x, y, nxt in (('dv1', None, None, 'dv2'), ('at1', 12400, 1170, 'at2'), ('at2', 11450, 300, 'at3')):
    if x is None: continue
    _, out = run('5 TP %d %d\n10 300 R\n' % (x, y), 320, {'FP_LEVEL': lv, 'FP_CHAR': '4'})
    loads = [l.split('[', 1)[1].split(']', 1)[0] for l in out.splitlines() if l.startswith('load[')]
    check('next_act_%s' % lv, loads[-1:] == [nxt], str(loads))

# ---- checkpoints (level data, AT1 4196): passing X 7600 then dying restarts at the checkpoint
rows, _ = run('5 TP 7590 1080\n40 HP 0\n40 HURT 10\n', 520, {'FP_LEVEL': 'at1', 'FP_CHAR': '4'})
xs = [r['x'] for r in rows[300:]]
check('at1_checkpoint_restart', xs and 7500 < xs[0] < 7700, 'x after restart %s' % (xs[0] if xs else None))

# ---- DV1 is still completable by input alone (the route found by the host search, tests/play/route_dv1_search.txt)
_, out = run(open(os.path.join(HERE, 'play', 'route_dv1_search.txt')).read(), 14100, {'FP_LEVEL': 'dv1'})
loads = [l.split('[', 1)[1].split(']', 1)[0] for l in out.splitlines() if l.startswith('load[')]
check('dv1_route_by_input_reaches_dv2', 'dv2' in loads, str(loads))

# ---- menu scenes (UI importer): the Time Attack preview masks (solid quick backdrops, kind 2) and the translucent
# layers of the semi-transparent images (kind 3: the bottom bar 135, the record panel 87)
def ui_kinds(path):
    d = open(path, 'rb').read(); n = struct.unpack_from('<H', d, 8)[0]; off = 12; out = []
    for _ in range(n):
        cls = d[off:off + 24].split(b'\0')[0].decode(); kind, nent, pad = d[off + 29], d[off + 30], d[off + 31]; off += 32
        off += 12 if kind == 2 else 6 * nent
        out.append((cls, kind, pad))
    return out
import struct
k = ui_kinds(os.path.join(DATA, 'ui', 'tasel_6.ui'))
check('tasel_preview_masks', sum(1 for c, kd, _ in k if kd == 2) >= 10, str(sum(1 for c, kd, _ in k if kd == 2)))
check('tasel_translucent_layers', {(c, a) for c, kd, a in k if kd == 3} >= {('~backdrop', 135), ('~recordback', 87)}, str({(c, a) for c, kd, a in k if kd == 3}))
k = ui_kinds(os.path.join(DATA, 'ui', 'chsel_7.ui'))
check('chsel_translucent_bar', ('~backdrop', 3, 135) in k)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
