#!/usr/bin/env python3
"""Inventory of a stage's active objects for porting: which classes are already ported (tools/import_frame.py
ENGINE_TYPES / ENGINE_TYPES_ID) and which are new, with counts and placements per act.

usage: tools/re/inventory.py <title> <frame index> [<frame index> ...]  > build/re/<stage>_inventory.md
(frame index n = build/frames/frame_<n+1>.json). Backdrops (level pieces) are left out; off-map instances (templates
created at run time) are counted separately."""
import json, os, sys, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from import_frame import engine_type

title, idx = sys.argv[1], [int(v) for v in sys.argv[2:]]
bd = json.load(open(os.path.join(ROOT, 'build', 'frames', 'backdrops.json')))
an = json.load(open(os.path.join(ROOT, 'build', 'frames', 'anims.json')))
frames = []
for i in idx:
    fr = json.load(open(os.path.join(ROOT, 'build', 'frames', 'frame_%d.json' % (i + 1))))
    frames.append(fr)
on = collections.defaultdict(lambda: collections.defaultdict(list)); off = collections.defaultdict(lambda: collections.defaultdict(int))
handles = collections.defaultdict(set)
for a, fr in enumerate(frames):
    for o in fr['objects']:
        key = '%s_%d' % (o['cls'], o['id'])
        if key in bd: continue
        if o.get('kind') == 'bg' and key not in an: continue
        handles[o['cls']].add(o['id'])
        if o['x'] is None or o['x'] < 0 or o['y'] < 0 or o['x'] > fr['width'] or o['y'] > fr['height']: off[o['cls']][a] += 1
        else: on[o['cls']][a].append((o['x'], o['y'], o['id']))

def place(c):
    out = []
    for a in range(len(frames)):
        p = on[c].get(a, [])
        if not p: continue
        if len(p) <= 12: out.append('A%d: %s' % (a + 1, ' '.join('%d,%d' % (x, y) for x, y, _ in p)))
        else: out.append('A%d: x %d..%d' % (a + 1, min(x for x, _, _ in p), max(x for x, _, _ in p)))
    return '; '.join(out)

print('# %s inventory (frame index %s)\n' % (title, ', '.join(map(str, idx))))
print('Counts are on-map placements; "off" = instances outside the level (templates for objects created at run time).\n')
for a, (i, fr) in enumerate(zip(idx, frames)):
    print('- Act %d (frame_%d.json, index %d): %d x %d px' % (a + 1, i + 1, i, fr['width'], fr['height']))
hdr = '| class | handles | ' + ' | '.join('A%d' % (a + 1) for a in range(len(frames))) + ' | off | placements |'
sep = '|' + '---|' * (len(frames) + 4)
for title2, want in (('Ported (engine type exists)', True), ('Not ported', False)):
    print('\n## %s\n\n%s\n%s' % (title2, hdr, sep))
    rows = []
    for c in sorted(set(on) | set(off)):
        ported = all(engine_type(c, h) is not None for h in handles[c])
        if ported != want: continue
        tot = sum(len(v) for v in on[c].values())
        rows.append((-tot, c))
    for _, c in sorted(rows):
        print('| %s | %s | %s | %s | %s |' % (c, ' '.join(map(str, sorted(handles[c]))),
              ' | '.join(str(len(on[c].get(a, []))) or '-' for a in range(len(frames))),
              sum(off[c].values()) or '-', place(c)))
