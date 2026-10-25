#!/usr/bin/env python3
"""Host tests for the Relic Maze act 3 Claw sequence (src/game/rm_claw.c, build/re/spec_rm_claw.md): Lilac and Carol
trigger the shrine scene, reach the caravan and the Claw fight; the Claw can be beaten (host BOSSHP), the camera is
released afterwards; the engine wreck (PushableBlock3 dropped on CaravanEngine) stops the caravan; Milla never triggers
it; Carol enters without the bike (6198). Frame-exact behaviour needs the original traces of spec 14 (not recorded).
usage: test_rm_claw.py [data_dir]   (default: converted2)"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')

def run(script, frames, ch):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); e.update({'FP_LEVEL': 'rm3', 'FP_CHAR': ch, 'FP_DBG_CLAW': '1', 'FP_TRACE_SND': '1'})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, check=True, env=e)
    os.unlink(name)
    rows, claw, music = [], [], []
    for line in r.stdout.splitlines():
        t = line.split()
        if not t: continue
        if t[0] == 'f': rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2)})
        elif t[0] == 'claw':
            d = {'frame': int(t[1]), 'g': t[3], 'f': t[4]}
            for i in range(5, len(t) - 1, 2):
                try: d[t[i]] = float(t[i + 1])
                except ValueError: d[t[i]] = t[i + 1]
            claw.append(d)
        elif t[0] == 'music': music.append((t[1], int(t[2][1:])))
    return rows, claw, music

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# group flags in the debug line: g = shrine car drive wreck, f = rig intro hover lunge shoot defeat
def grp(c, k): return c['g'][k] == '1' if k < 4 else c['f'][k - 4] == '1'
SHRINE, CAR, DRIVE, WRECK, RIG, INTRO, HOVER, LUNGE, SHOOT, DEFEAT = range(10)
WALK = '10 TP 630 470\n20 60 R\n'      # onto the shrine ledge: the CameraTriggerBoss at (664, 376)

# ---- Lilac: shrine scene -> boost through the corridor -> caravan; then the arena, the fight, the defeat, the wreck
script = WALK + '800 TP 6330 520\n805 SETV others E 99999\n1100 BOSSHP 0\n1450 TP 6560 440\n1470 PB3 6676 490\n'
rows, claw, music = run(script, 1900, '0')
on = [c for c in claw if grp(c, SHRINE)]
check('lilac_trigger', bool(on) and on[0]['frame'] < 60, 'first %s' % (on[0]['frame'] if on else None))
check('lilac_shrine_timeline', any(c['D'] == 300 for c in claw), 'max D %s' % max(c['D'] for c in claw))
check('lilac_script_control', any(c['ctrl'] != -1 for c in on) and all(c['ctrl'] == -1 for c in claw if grp(c, CAR)))
check('lilac_dragon_boost', any(r['st'] in (3, 4) and 270 < r['f'] < 500 for r in rows))
car = [c for c in claw if grp(c, CAR)]
check('lilac_caravan', bool(car) and car[0]['frame'] < 800 and car[0]['nC'] == 4032, 'first %s' % (car[0]['frame'] if car else None))
check('lilac_chase_music', any(m == 'bgm/m_chase.adp' for m, _ in music), str(music))
fight = [c for c in claw if c['M'] == 1]
check('lilac_fight_starts', bool(fight) and any(grp(c, HOVER) for c in claw) and any(grp(c, SHOOT) for c in claw))
check('lilac_midboss_music', any(m == 'bgm/m_midboss.adp' for m, _ in music), str(music))
check('lilac_beams', any(c['beams'] >= 8 for c in claw))
check('lilac_arena_lock', any(c['camU'] == 1 for c in fight))
dfr = [c for c in claw if grp(c, DEFEAT)]
check('lilac_claw_defeated', bool(dfr) and any(c['Z'] == 21 for c in claw))
after = [c for c in claw if c['frame'] > (dfr[-1]['frame'] if dfr else 1e9)]
check('lilac_camera_released', bool(after) and after[0]['camU'] == 0 and after[0]['M'] == 0)
wr = [c for c in claw if grp(c, WRECK)]
check('lilac_engine_wreck', bool(wr) and any(c['eD'] == 41 for c in claw) and not grp(claw[-1], DRIVE))
check('lilac_outdoor_music', any(m == 'bgm/m_outdoor.adp' for m, _ in music), str(music))
check('lilac_wreck_right_bound', claw[-1]['nE'] > 8000, str(claw[-1]['nE']))

# ---- Carol: the same trigger, the scripted run and pad rides (M48) through the corridor, then the fight
rows, claw, music = run(WALK + '800 TP 6330 520\n805 SETV others E 99999\n', 1000, '2')
check('carol_trigger', any(grp(c, SHRINE) for c in claw))
check('carol_pad_rides', sum(1 for r in rows if r['st'] == 20 and 400 < r['f'] < 700) > 20)
check('carol_caravan', any(grp(c, CAR) for c in claw))
check('carol_fight_starts', any(c['M'] == 1 for c in claw) and any(grp(c, HOVER) for c in claw))

# ---- Carol on the bike enters RM3 on foot (6198: g1 = 2)
rows, claw, music = run('', 30, '3')
check('carol_bike_removed', rows[-1]['chr'] == 2 and rows[-1]['bike'] == 0, 'chr %s bike %s' % (rows[-1]['chr'], rows[-1]['bike']))

# ---- Milla: no trigger (6202 needs MV.A != 4); the rig still orbits
rows, claw, music = run(WALK + '60 TP 660 470\n', 400, '4')
check('milla_no_trigger', not any(grp(c, SHRINE) or grp(c, CAR) for c in claw) and all(c['ctrl'] == -1 for c in claw))
check('milla_rig_orbits', len({c['x'] for c in claw}) > 3)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
