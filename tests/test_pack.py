#!/usr/bin/env python3
"""Disc packs (tools/pack_disc.py + src/resources/pack.c) on the host build:
- a packed copy of the data plays identically to the loose files (Time Attack route + act change);
- the resident-Lilac path is taken on DV1 -> DV2 (load profile: 4 files skipped);
- a corrupted or truncated pack fails cleanly (no crash, the level reports missing data)."""
import os, shutil, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..')
BIN = os.path.join(ROOT, 'host', 'fpdc_host'); SRC = os.path.join(ROOT, 'converted2')
fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else '')); fails += 0 if cond else 1

def run(data, script, frames, env=None):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f: f.write(script); sn = f.name
    e = dict(os.environ, FP_TRACE_SND='1', FP_TRACE_SID='1'); e.pop('FP_LEVEL', None); e.update(env or {})
    p = subprocess.run([BIN, data, str(frames), sn], capture_output=True, text=True, env=e); os.unlink(sn)
    return p

with tempfile.TemporaryDirectory() as d:
    for x in ('dv1', 'dv2', 'dv3', 'dv4', 'ui', 'sfx'): shutil.copytree(os.path.join(SRC, x), os.path.join(d, x))
    shutil.copy(os.path.join(SRC, 'levels.txt'), d); os.symlink(os.path.abspath(os.path.join(SRC, 'bgm')), os.path.join(d, 'bgm'))
    subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'pack_disc.py'), d], check=True, capture_output=True)
    check('packs_written', all(os.path.exists(os.path.join(d, x + '.pak')) for x in ('dv1', 'dv2', 'ui', 'sfx')) and not os.path.exists(os.path.join(d, 'dv1')))
    TA = '40 42 S\n200 201 D\n220 221 J\n330 331 J\n470 471 J\n490 491 D\n520 521 J\n630 631 J\n900 1300 R\n950 960 J\n'
    a, b = run(SRC, TA, 1400), run(d, TA, 1400)
    check('time_attack_route_identical', a.stdout == b.stdout and b.returncode == 0, 'rc %d' % b.returncode)
    ACT = '5 TP 14600 980\n7 HP 5\n10 400 R\n'
    a, b = run(SRC, ACT, 400, {'FP_LEVEL': 'dv1'}), run(d, ACT, 400, {'FP_LEVEL': 'dv1'})
    check('act_change_identical', a.stdout == b.stdout and 'act 2' in b.stdout.replace('act 2.', 'act 2'), 'rc %d' % b.returncode)
    l2 = [l for l in b.stderr.splitlines() if l.startswith('load[dv2]')]
    check('resident_lilac_on_act_change', l2 and 'skipped 4 (' in l2[0], l2[0] if l2 else 'no dv2 load line')
    # corruption: flip bytes inside the first compressed entry of dv1.pak; truncate dv2.pak
    p = os.path.join(d, 'dv1.pak'); raw = bytearray(open(p, 'rb').read())
    for i in range(600, 700): raw[i] ^= 0x5a
    open(p, 'wb').write(raw)
    c = run(d, '', 10, {'FP_LEVEL': 'dv1'})
    check('corrupt_pack_fails_cleanly', c.returncode >= 0 and ('corrupt entry' in c.stderr or 'load failed' in c.stderr), 'rc %d %s' % (c.returncode, c.stderr.strip().splitlines()[-1:] if c.stderr else ''))
    p = os.path.join(d, 'dv2.pak'); open(p, 'r+b').truncate(1000)
    c = run(d, '', 10, {'FP_LEVEL': 'dv2'})
    check('truncated_pack_fails_cleanly', c.returncode >= 0 and ('bad entry' in c.stderr or 'invalid' in c.stderr or 'load failed' in c.stderr), 'rc %d' % c.returncode)
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
