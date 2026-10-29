#!/usr/bin/env python3
"""Host tests for the Dragon Valley fixes (session 25): the DV1 Wretchnid's invisible pellets and the DV2 geysers ("blowholes").

RE: Frames::event_func_4773 / 4856-4860 (pellets added to the shooter's layer), col_4_20 / col_20_20 (pellet vs the character
sensor / the background), 4961-4969 (geyser burst, sound 44, dust / bubbles, launch, fast phase), on_frame_22..24_init (the
plate is in q13 only). RV: original traces build/orig_traces/dv_fixes (rv1.sh / rv2.sh): wn_pel (DV1 TP 14330 960, no input),
gey_off (DV2 TP 340 1040), gey_on (DV2 TP 448 1040, on the first geyser), tp_hurt (tests/orig/hurt_turretus.txt). The values
below are copied from those traces. Frame mapping: trace sample s = host frame s - 2 (wn_pel) / s + 1 (gey_*).
Every check fails on the port before the fix (pellets on layer 0 behind the arena's dvbackwall, whole-box pellet hit one frame
early, no geyser effects, repeated geyser impulses).
usage: test_dv_fixes.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.environ.get('FP_TEST_BIN', os.path.join(HERE, '..', 'host', 'fpdc_host'))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
OT_CRYSTAL, OT_EXPLODE, OT_GEYSER, OT_WR_WEAK, OT_WR_BARRIER, OT_PELLET = 2, 100, 55, 58, 60, 101
CH = {0: 'lilac', 2: 'carol', 4: 'milla'}

def run(level, frames, script, ch=0, track=None, extra=None, args=()):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script.replace(';', '\n') + '\n'); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_TRACK', 'FP_TRACE_SID')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch), 'FP_TRACE_SID': '1'})
    if track: e['FP_TRACK'] = track
    if extra: e.update(extra)
    r = subprocess.run([BIN, DATA, str(frames), name] + list(args), capture_output=True, text=True, env=e)
    os.unlink(name)
    rows, obs, sids, pend = {}, [], {}, []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f':
            d = {t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i] not in ('f',)}
            fr = int(t[1]); rows[fr] = d
            if pend: sids[fr] = pend; pend = []
        elif t[0] == 'sid': pend.append(int(t[1]))   # belongs to the next 'f' row
        elif t[0] == 'ob':
            d = {t[i]: t[i + 1] for i in range(4, len(t) - 1, 2)}
            obs.append((int(t[1]), int(t[2]), {k: (float(v) if k != 'fl' else int(v, 16)) for k, v in d.items()}))
    return rows, obs, sids

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1
def snd_frames(sids, sid): return [f for f in sorted(sids) if sid in sids[f]]

# ================================================================ DV1: the Wretchnid's pellets
# RV wn_pel: the first four shots (samples 200 / 214 / 228 / 242 = host 198 / 212 / 226 / 240), six positions each
RV_SHOTS = {198: [(14414, 870), (14410, 873), (14406, 877), (14403, 882), (14399, 886), (14396, 891)],
            212: [(14418, 845), (14415, 849), (14412, 854), (14409, 858), (14406, 864), (14404, 869)],    # dir 21: origin (-15, +23)
            226: [(14414, 829), (14410, 832), (14406, 836), (14403, 841), (14399, 845), (14396, 850)],
            240: [(14414, 846), (14410, 849), (14406, 853), (14403, 858), (14399, 862), (14396, 867)]}
WN = '60 TP 14330 960'
for ch in (0, 2, 4):
    rows, obs, sids = run('dv1', 300, WN, ch, track='%d:-1' % OT_PELLET)
    pel = [d for f, t, d in obs if t == OT_PELLET]
    # the root cause: obj_spawn_pellet left o->layer 0, drawn before the play layer's pieces (the arena's dvbackwall covers it)
    # RE 4855-4860: charge 74, five shots 14 frames apart (RV 160 / 199 213 227 241 255 in trace frames)
    shots = snd_frames(sids, 28)
    hurt = next((f for f in sorted(rows) if f > 1 and rows[f]['hp'] < rows[f - 1]['hp']), None)
    check('wn_pellets_on_the_turret_layer_%s' % CH[ch], pel and all(d['lay'] == 2 and d['an'] >= 0 for d in pel) and
          snd_frames(sids, 74)[:1] == [159] and shots[:5] == [198, 212, 226, 240, 254],
          '%d rows, layers %s, charge %s shots %s' % (len(pel), sorted({d['lay'] for d in pel}), snd_frames(sids, 74)[:1], shots[:5]))
    # col_4_20: the pellet that hits is destroyed with explosion02 and sound 17; the others die on the ground (col_20_20, 17) or
    # off the view: none is left 60 frames after the last shot
    near = [d for f, t, d in obs if t == OT_PELLET and f == hurt and abs(d['x'] - rows[hurt]['x']) < 16 and abs(d['y'] - rows[hurt]['y'] + 24) < 30]
    left = [d for f, t, d in obs if t == OT_PELLET and f == 299]
    check('wn_pellet_hits_and_is_removed_%s' % CH[ch], hurt is not None and hurt - shots[0] == 21 and 17 in sids.get(hurt, []) and not near and not left and
          len(snd_frames(sids, 17)) >= 5, 'hurt %s, 17 at %s, left %d' % (hurt, snd_frames(sids, 17), len(left)))
    if ch == 0:
        pos = {}
        for f, t, d in obs:
            if t == OT_PELLET: pos.setdefault(f, set()).add((int(d['x']), int(d['y'])))
        bad = [(f0 + k, p) for f0, tr in RV_SHOTS.items() for k, p in enumerate(tr) if p not in pos.get(f0 + k, set())]
        check('wn_pellet_paths_match_rv', not bad, str(bad[:4]))
        # RV wn_pel: the first shot passes 2 px in front of Lilac's sensor at trace f219 and hits at f220 (21 frames after the
        # shot); the 16x16 box with the unfloored X hit one frame early. The pellet's mask (a 12-px disc) is the visible sprite.
        check('wn_pellet_hit_frame_rv', hurt == 219 and hurt - shots[0] == 21, 'shot %s hurt %s' % (shots[:1], hurt))

# the pellet is drawn: frame 206 rendered with and without the pellets (KILLT 101 at 206) differs inside the pellet's box
with tempfile.TemporaryDirectory() as dd:
    def frame(script, sub):
        os.makedirs(os.path.join(dd, sub))
        rows, obs, _ = run('dv1', 207, script, 0, track='%d:-1' % OT_PELLET, args=(os.path.join(dd, sub), '206'))
        data = open(os.path.join(dd, sub, 'f00206.ppm'), 'rb').read()
        return rows, obs, data[-426 * 240 * 3:]
    r1, o1, a = frame(WN, 'a')
    r2, o2, b = frame(WN + ';206 KILLT %d' % OT_PELLET, 'b')
    p = [d for f, t, d in o1 if f == 206 and t == OT_PELLET]
    cx, cy = r1[206]['cx'], r1[206]['cy']
    diff = 0
    for d in p:
        sx, sy = int(d['x'] - cx), int(d['y'] - cy)
        for y in range(max(0, sy - 6), min(240, sy + 6)):
            for x in range(max(0, sx - 6), min(426, sx + 6)):
                i = 3 * (426 * y + x); diff += a[i:i + 3] != b[i:i + 3]
    check('wn_pellet_visible_over_the_arena_wall', p and diff >= 40 and r2[206]['cx'] == cx, '%d pellets, %d px differ' % (len(p), diff))

# the same hit rule on a Turretus (tests/orig/hurt_turretus.txt; RV tp_hurt / hurt_turretus.snd: shot 186, hit 17 25 at 189)
rows, obs, sids = run('dv1', 200, open(os.path.join(HERE, 'orig', 'hurt_turretus.txt')).read(), 0, track='%d:-1' % OT_PELLET,
                      extra={'FP_SPAWN_X': '0', 'FP_OBJ_PRETICK': '2'})
check('turretus_pellet_shot_and_hit_frames_rv', snd_frames(sids, 28)[:1] == [186] and snd_frames(sids, 25)[:1] == [189] and
      all(d['lay'] == 2 for f, t, d in obs if t == OT_PELLET), 'shot %s hit %s' % (snd_frames(sids, 28)[:1], snd_frames(sids, 25)[:1]))

# defeatable by Lilac (jump + whip), Carol (jump-claw), Milla (jump + phantom cube); the unlock (4871) gives 10 crystals on the
# railing's layer (they were on layer 0, behind the dvbackwall) and removes the barriers
for ch, keys, step in ((0, ('J', 'X'), 20), (2, ('JX',), 8), (4, ('J', 'B'), 24)):
    s = ['60 TP 14400 990'] + ['%d %d %s' % (f + 6 * i, f + 6 * i + 1, k) for f in range(100, 2000, step) for i, k in enumerate(keys)]
    rows, obs, sids = run('dv1', 2000, ';'.join(s), ch, track='%d:-1,%d:-1,%d:-1' % (OT_WR_WEAK, OT_WR_BARRIER, OT_CRYSTAL))
    A = {f: d['A'] for f, t, d in obs if t == OT_WR_WEAK}
    kill = min((f for f in A if A[f] <= 0), default=None)
    bars = max((f for f, t, d in obs if t == OT_WR_BARRIER), default=None)
    f0 = min((f for f, t, d in obs if t == OT_CRYSTAL and abs(d['x'] - 14432) < 4), default=None)
    crys = [d for f, t, d in obs if t == OT_CRYSTAL and abs(d['x'] - 14432) < 4 and f == f0]
    check('wn_defeated_unlock_%s' % CH[ch], kill is not None and bars is not None and bars < 1999 and len(crys) >= 9 and   # (one may be taken in its first frame)
          {(int(d['x']), int(d['y'])) for d in crys} <= {(14432, y + o) for y in (776, 824, 872, 920, 968) for o in (12, 36)} and
          all(d['lay'] == 2 for d in crys), 'kill %s barriers until %s crystals %d layers %s' % (kill, bars, len(crys), sorted({d['lay'] for d in crys})))

# ================================================================ DV2: the geysers
# RV gey_off: 30 dusts per eruption (one a frame while the plate rises at 182 <= E <= 240, the plate then leaves the view + 100),
# 66 frames each, rising 1 px a frame from the plate's y + 9; eruptions every 150 frames with sound 44
RV_DUST_Y0 = [1071, 1063, 1054, 1046, 1038, 1030, 1021, 1013, 1005, 997, 990, 982, 974, 967, 960, 952, 945, 938, 932, 925, 919, 913,
              907, 901, 896, 890, 885, 881, 876, 872]
def dust_rows(obs):
    by = {}
    for f, t, d in obs:
        if t == OT_EXPLODE and abs(d['x'] - 448) < 40: by.setdefault(f, []).append(d)
    return by
for ch in (0, 2, 4):
    rows, obs, sids = run('dv2', 420, '60 TP 340 1040', ch, track='%d:-1' % OT_EXPLODE)
    by = dust_rows(obs)
    dust = {f: [d for d in v if d['lay'] == 1] for f, v in by.items()}
    check('gey_burst_sound_44_every_150_%s' % CH[ch], snd_frames(sids, 44) == [89, 239, 389], str(snd_frames(sids, 44)))
    if ch == 0:
        exp = {}
        for c0 in (89, 239):                          # trace samples; host frame = sample + 1
            for s in range(c0, c0 + 96): exp[s + 1] = min(s - c0 + 1, 30, c0 + 95 - s)
        got = {f: len(dust.get(f, [])) for f in exp}
        bad = [(f, got[f], exp[f]) for f in exp if got[f] != exp[f]]
        check('gey_dust_count_matches_rv', not bad, str(bad[:5]))
        ymin = [min(d['y'] for d in dust.get(s + 1, [{'y': 0}])) for s in range(89, 119)]
        check('gey_dust_spawn_y_matches_rv', ymin == RV_DUST_Y0, str([(a, b) for a, b in zip(ymin, RV_DUST_Y0) if a != b][:4]))
    # move_back: the dust is drawn behind the plate layer's backdrops (layer 1 pass; RV screenshots gey_off / gey_on f150)
    ans = {}
    for v in by.values():
        for d in v: ans[d['an']] = ans.get(d['an'], 0) + 1
    dan = max(ans, key=ans.get) if ans else None            # the dust animation (the 4961 burst is a few frames of another)
    check('gey_dust_behind_the_play_layer_%s' % CH[ch], ans and all(d['lay'] == 1 for v in by.values() for d in v if d['an'] == dan) and
          sum(1 for v in dust.values() if v) >= 200, 'frames with dust %d' % sum(1 for v in dust.values() if v))

# RV gey_on: Lilac on the geyser: one impulse per eruption, G = -13.8 at E 182 then plain gravity (-13.55, -13.3 ...), apex
# 674.7, back on the rim and launched again 150 frames later (the port re-applied 4968 on every overlapping frame)
for ch in (0, 2, 4):
    rows, obs, sids = run('dv2', 420, '60 TP 448 1040', ch)
    fs = sorted(rows)
    launches = [f for a, f in zip(fs, fs[1:]) if rows[f]['vy'] <= -13 and rows[a]['vy'] > -13]
    seqs = [[round(rows[f + k]['vy'], 3) for k in range(4)] for f in launches]
    apex = [min(rows[f]['y'] - 14 for f in range(l, l + 100)) for l in launches[:2]]
    check('gey_one_impulse_per_eruption_%s' % CH[ch], len(launches) == 3 and launches[1] - launches[0] == 150 and launches[2] - launches[1] == 150 and
          all(s == [-13.8, -13.55, -13.3, -13.05] for s in seqs), 'launch frames %s vy %s' % (launches, seqs[:1]))
    if ch == 0:
        check('gey_apex_matches_rv', apex and abs(apex[0] - 674.7) < 1.0, 'apex %s (RV 674.7)' % apex)
    else:
        check('gey_apex_%s' % CH[ch], len(apex) == 2 and all(abs(a - 674.7) < 1.5 for a in apex), str(apex))

# restart / act change: the eruption effects are per act state (LOAD mid-eruption: no stale dust, the next eruption works)
rows, obs, sids = run('dv2', 400, '60 TP 340 1040;100 LOAD dv2;160 TP 340 1040', 0, track='%d:-1' % OT_EXPLODE)
by = dust_rows(obs)
after = [f for f in by if 101 <= f <= 160 and any(d['lay'] == 1 for d in by[f])]
check('gey_restart_clears_dust', 98 in by and not after and len(snd_frames(sids, 44)) >= 2, 'dust frames after the reload %s, 44 at %s' % (after[:3], snd_frames(sids, 44)))

# the importer ties the dust row (effectsdust_437 anim 4) to the geyser plates, so DV3 (no pushable block) has it too
rep = os.path.join(DATA, 'dv3', 'dv3_report.txt')
if os.path.exists(rep):
    check('dv3_bank_has_the_geyser_dust', "'dust'" in open(rep).read())

print('%d failures' % fails)
sys.exit(1 if fails else 0)
