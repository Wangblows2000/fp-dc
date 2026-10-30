#!/usr/bin/env python3
"""
Developer tool (cutscene polish): the port's story scene next to the original's screenshots at the same scene times.

usage: story_cmp.py <trace.log> <scene id> <first> <end> <out dir> [--chr N] [--data DIR] [--every N] [--shift D] [--sheet K]
                    [--frames f,f,...] [--keep]

<trace.log> is an RV trace of tools/orig/story_rv.sh taken with STORY_SHOT=N (screenshots in <trace.log>_shots/f<frame>.png,
852 x 480: the original's 426 x 240 at 2x); [first, end) is the scene's window in it (its F events), as tools/story_rv2scn.py.
The host build plays the scene (ADVNEW <chr>, SCENE <id>) and presses A at the reader's presses of the trace ('A <f>' lines:
the port's segments end there, tools/story_rv2scn.py), so the port shows the same scene time as each shot. The port's frames
are dumped every --every frames (aligned to the trace's frames), each pair is written side by side (original left, port right,
426 x 240 each) into contact sheets of --sheet pairs: <out>/<id>_NN.png, and <out>/<id>.txt lists the pairs' mean absolute
difference (0-255), largest first. Host-only (never shipped).
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
from PIL import Image, ImageDraw
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log'); ap.add_argument('sid'); ap.add_argument('first', type=int); ap.add_argument('end', type=int); ap.add_argument('out')
    ap.add_argument('--chr', type=int, default=0)
    ap.add_argument('--data', default=os.path.join(ROOT, 'converted2'))
    ap.add_argument('--bin', default=os.path.join(ROOT, 'host', 'fpdc_host'), help='the host build to run (another checkout\'s: before / after)')
    ap.add_argument('--every', type=int, default=60)
    ap.add_argument('--shift', type=int, default=0, help='port frames later than the trace (calibration)')
    ap.add_argument('--sheet', type=int, default=6)
    ap.add_argument('--frames', default='', help='only these trace frames')
    ap.add_argument('--keep', action='store_true', help='keep the port frames (<out>/<id>_port)')
    ap.add_argument('--actors', default='', help='print the port\'s drawn actors at these trace frames (ACTORS)')
    ap.add_argument('--says', action='store_true', help='also list each line\'s start, trace frame against the port\'s (timing drift)')
    a = ap.parse_args()
    shots = a.log + '_shots'
    presses, box, bx = [], 0, {}                         # the reader's presses (A), and the idle ones (I) while the DialogBox shows (as
    for l in open(a.log, encoding='latin1'):     # tools/story_rv2scn.py: the port's segments end there)
        t = l.split()
        if len(t) > 9 and t[0] == 'O' and t[3].startswith('dialogbox_'): bx[t[2]] = int(t[9], 16) & 1; box = any(bx.values())
        if t and t[0] in ('A', 'I') and (t[0] == 'A' or box):
            f = int(t[1])
            if a.first <= f < a.end: presses.append(f)
    want = sorted(int(n[1:6]) for n in os.listdir(shots) if re.match(r'f\d{5}\.png$', n) and a.first <= int(n[1:6]) < a.end) if os.path.isdir(shots) else []
    if a.frames: want = [f for f in (int(v) for v in a.frames.split(',')) if a.first <= f < a.end]
    if not want: sys.exit('no shots in the window')
    # the host: SCENE at S0; trace frame t is host frame S0 + (t - first) + shift. Dumps happen on host frames % every == 0.
    S0 = 200
    while (S0 + a.shift - a.first) % a.every: S0 += 1
    port_of = lambda t: S0 + (t - a.first) + a.shift
    sc = '5 ADVNEW %d\n%d SCENE %s\n' % (a.chr, S0, a.sid)
    for p in presses: sc += '%d %d J\n' % (port_of(p), port_of(p) + 1)
    for t in (int(v) for v in a.actors.split(',') if v): sc += '%d ACTORS\n' % port_of(t)
    os.makedirs(a.out, exist_ok=True)
    dd = os.path.join(a.out, a.sid + '_port'); os.makedirs(dd, exist_ok=True)
    with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f: f.write(sc); scn = f.name
    env = dict(os.environ)
    for k in ('FP_LEVEL', 'FP_CHAR', 'FP_TA', 'FP_FILE', 'FP_FILE_NEW', 'FP_SAVE'): env.pop(k, None)
    r = subprocess.run([a.bin, a.data, str(port_of(max(want)) + 1), scn, dd, str(a.every)],
                       capture_output=True, text=True, env=env)
    os.unlink(scn)
    log = [l for l in r.stdout.splitlines() if l.startswith(('story ', 'csx begin'))]
    if a.actors: print('\n'.join(l for l in r.stdout.splitlines() if l.startswith('csxa')))
    if a.says:                                   # the original's line starts (DialogBox F 2, C 1: tools/story_rv2scn.py) and the port's
        orig, pv, port, fr = [], 0, [], 0
        for l in open(a.log, encoding='latin1'):
            t = l.split()
            if len(t) > 13 and t[0] == 'O' and t[3].startswith('dialogbox_') and a.first <= int(t[1]) < a.end:
                al = dict(kv.split('=', 1) for kv in t[13:] if '=' in kv)
                if int(t[9], 16) & 1 and al.get('F') == '2' and al.get('C') == '1': orig.append((int(t[1]), int(float(al.get('E', 0)))))
        for l in r.stdout.splitlines():
            if l.startswith('f '): fr = int(l.split()[1])
            elif l.startswith('csx say '): port.append((fr - S0 - a.shift + a.first, int(l.split()[2])))
        seen = set(); o2 = []
        for f, e in orig:
            if (e, f // 4) in seen or any(e == e2 and f - f2 < 3 for f2, e2 in o2): continue
            o2.append((f, e)); seen.add((e, f // 4))
        for k in range(max(len(o2), len(port))):
            print('line %s at %s / port %s at %s' % (o2[k][1] if k < len(o2) else '-', o2[k][0] if k < len(o2) else '-',
                                                   port[k][1] if k < len(port) else '-', port[k][0] if k < len(port) else '-'))
    pairs = []
    for t in want:
        if (port_of(t)) % a.every: continue
        pp = os.path.join(dd, 'f%05d.ppm' % port_of(t))
        if not os.path.exists(pp): continue
        o = Image.open(os.path.join(shots, 'f%05d.png' % t)).convert('RGB').resize((426, 240), Image.BOX)
        p = Image.open(pp).convert('RGB')
        d = float(np.abs(np.asarray(o, np.int16) - np.asarray(p, np.int16)).mean())
        pairs.append((t, o, p, d))
    for k in range(0, len(pairs), a.sheet):
        grp = pairs[k:k + a.sheet]
        im = Image.new('RGB', (852 + 4, len(grp) * 252), (255, 0, 255))
        dr = ImageDraw.Draw(im)
        for j, (t, o, p, d) in enumerate(grp):
            im.paste(o, (0, j * 252 + 12)); im.paste(p, (430, j * 252 + 12))
            dr.text((2, j * 252), '%s t %d (scene %d)  diff %.1f   ORIGINAL | PORT' % (a.sid, t, t - a.first, d), fill=(255, 255, 0))
        im.save(os.path.join(a.out, '%s_%02d.png' % (a.sid, k // a.sheet)))
    with open(os.path.join(a.out, a.sid + '.txt'), 'w') as f:
        f.write('\n'.join(log) + '\n')
        for t, o, p, d in sorted(pairs, key=lambda x: -x[3]): f.write('%d %.1f\n' % (t, d))
    if not a.keep: shutil.rmtree(dd)
    print('%s: %d pairs, %d sheets, mean diff %.1f' % (a.sid, len(pairs), (len(pairs) + a.sheet - 1) // a.sheet,
                                                       sum(x[3] for x in pairs) / max(1, len(pairs))))

if __name__ == '__main__':
    main()
