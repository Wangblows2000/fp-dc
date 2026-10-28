#!/usr/bin/env python3
"""Hurt-state recovery (session 21d, user request): the hurt state (M8) ends on landing as in the original (RE / RV), or at
PHYS_HURT_MAX_T = 180 frames (3 s) after the first hit of a hurt sequence at the latest, airborne or not (INFERRED vs the
original, which only ends it on landing). Checks for Lilac, Carol and Milla: a ground hit recovers on landing; a long airborne
hit (TB3's open shaft) returns control at 180 frames; a hit during the hurt state does not restart the 3 s; HP 0 still kills.
usage: test_hurt.py [data_dir]   (default: converted2)"""
import os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
CHARS = ((0, 'lilac'), (2, 'carol'), (4, 'milla'))
ST_HURT, ST_DEAD = 5, 6
fails = 0

def run(level, ch, frames, script):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': str(ch)})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    rows = []
    for l in r.stdout.splitlines():
        t = l.split()
        if t and t[0] == 'f':
            rows.append({t[i]: float(t[i + 1]) for i in range(0, len(t) - 1, 2) if t[i + 1].lstrip('-').replace('.', '', 1).isdigit()})
    return rows

def check(name, cond, info=''):
    global fails
    print('%s %s  [%s]' % ('PASS' if cond else 'FAIL', name, info))
    if not cond: fails += 1

def hurt_frames(rows, f0):
    return [int(r['f']) for r in rows if r['f'] >= f0 and r['st'] == ST_HURT]

for ch, nm in CHARS:
    # ground hit (DV1 start area): knocked up, recovers on landing (RE: M8 ends on landing + ~5 frames)
    rows = run('dv1', ch, 200, '30 HURT 5\n')
    h = hurt_frames(rows, 30)
    check('ground_hit_recovers_on_landing_%s' % nm, h and len(h) < 90 and rows[-1]['st'] != ST_HURT, 'hurt %d frames' % len(h))
    # long airborne hit (TB3's shaft, x 6000 from y 40): before session 21d the state lasted ~250 frames, until landing
    rows = run('tb3', ch, 400, '3 TP 6000 40\n10 HURT 5\n')
    h = hurt_frames(rows, 10)
    air = [r for r in rows if r['f'] == (h[-1] + 1 if h else -1)]
    check('airborne_hurt_capped_3s_%s' % nm, h and 170 <= len(h) <= 181 and h[-1] <= 10 + 180 and air and air[0]['gnd'] == 0,
          'hurt %d frames, last %s, airborne after %s' % (len(h), h[-1] if h else None, air[0]['gnd'] == 0 if air else None))
    # a second hit while still hurt (and still in the air) keeps the first hit's timer
    rows = run('tb3', ch, 400, '3 TP 6000 40\n10 HURT 5\n100 SETV others E 0\n101 HURT 5\n')
    h = hurt_frames(rows, 10)
    check('rehit_does_not_extend_%s' % nm, h and h[-1] <= 10 + 180, 'last hurt frame %s' % (h[-1] if h else None))
    # death still overrides recovery
    rows = run('dv1', ch, 120, '30 HP 1\n32 HURT 5\n')
    check('death_overrides_%s' % nm, any(r['st'] == ST_DEAD for r in rows), '')

print('%d failures' % fails)
sys.exit(1 if fails else 0)
