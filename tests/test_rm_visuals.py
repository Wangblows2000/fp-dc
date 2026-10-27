#!/usr/bin/env python3
"""Host test (session 20): Relic Maze grabbable gimmicks show no red boxes.

The hang sensors (GimmickHangingMovingSensor / Sensor2 / IdleSwitch / IdleMover: OT 260 / 262 / 263 / 265) are collision
boxes whose image is a solid red editor rectangle; every constructor calls set_visible(0) (RE create_gimmickhanging*). The
port drew them over the moving bars, propeller platforms and handles. Registry flag OD_HIDDEN now hides them (they still
collide). Checks: every such object in RM1-RM4 is hidden, and Lilac still grabs an RM2 handle.
usage: test_rm_visuals.py [data_dir]"""
import subprocess, sys, os, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, '..', 'host', 'fpdc_host')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'converted2')
fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

def run(level, script, frames, track):
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
        f.write(script); name = f.name
    e = dict(os.environ); [e.pop(k, None) for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE', 'FP_TRACK')]
    e.update({'FP_LEVEL': level, 'FP_CHAR': '0', 'FP_TRACK': track})
    r = subprocess.run([BIN, DATA, str(frames), name], capture_output=True, text=True, env=e)
    os.unlink(name)
    return r.stdout

TR = '260:-1,262:-1,263:-1,265:-1'
for lv in ('rm1', 'rm2', 'rm3', 'rm4'):
    out = run(lv, '', 3, TR)
    obs = [l.split() for l in out.splitlines() if l.startswith('ob 2 ')]
    shown = [o for o in obs if o[o.index('hd') + 1] != '1']
    check('%s_hang_sensors_hidden' % lv, not shown, '%d sensors, %d drawn' % (len(obs), len(shown)))

# RM2 1968,824: the moving bar 2 with its sensor; Lilac jumps into it and hangs (state ST_HANG in the 'f' rows: hang flag)
out = run('rm2', '3 SETV others E 99999\n3 TP 1993 880\n', 60, '')
hang = [l for l in out.splitlines() if l.startswith('f ') and ' st ' in l]
st = [int(l.split()[l.split().index('st') + 1]) for l in hang]
check('rm2_handle_still_grabbed', st and st[-1] == 11, str(sorted(set(st))))   # 11 = ST_HANG
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
