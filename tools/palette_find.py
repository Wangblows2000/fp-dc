#!/usr/bin/env python3
"""Find images whose opaque pixels mostly use the palette of a reference image.
usage: palette_find.py <img_dir> <ref_ids,comma> <threshold> <out.txt>"""
import sys, os
from PIL import Image
src = sys.argv[1]; refs = [int(x) for x in sys.argv[2].split(',')]
thr = float(sys.argv[3]); out = sys.argv[4]
pal = set()
for r in refs:
    im = Image.open(os.path.join(src, 'img_%d.png' % r)).convert('RGBA')
    pal |= {p[:3] for p in im.getdata() if p[3] > 0}
hits = []
n = len([f for f in os.listdir(src) if f.endswith('.png')])
for i in range(n):
    im = Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')
    if im.width > 128 or im.height > 128: continue
    px = [p[:3] for p in im.getdata() if p[3] > 0]
    if len(px) < 150: continue
    frac = sum(1 for p in px if p in pal) / len(px)
    if frac >= thr: hits.append((i, im.width, im.height, round(frac, 3)))
with open(out, 'w') as f:
    for h in hits: f.write('%d %d %d %.3f\n' % h)
print(len(hits))
