#!/usr/bin/env python3
"""Render image ids enlarged with hotspot (meta[0],meta[1]) marked.
usage: strip.py <img_dir> <out.png> <ids: a-b,c,...> [scale] [cols]"""
import sys
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
ids = []
for part in sys.argv[3].split(','):
    if '-' in part:
        a, b = map(int, part.split('-')); ids += range(a, b + 1)
    else: ids.append(int(part))
scale = int(sys.argv[4]) if len(sys.argv) > 4 else 2
cols = int(sys.argv[5]) if len(sys.argv) > 5 else 10
ims = [Image.open(f'{src}/img_{i}.png').convert('RGBA') for i in ids]
W = max(i.width for i in ims); H = max(i.height for i in ims)
W = max(W, 40)
rows = (len(ids) + cols - 1) // cols
s = Image.new('RGB', (cols * W * scale, rows * (H * scale + 12)), (30, 30, 60))
d = ImageDraw.Draw(s)
for k, (i, im) in enumerate(zip(ids, ims)):
    x, y = (k % cols) * W * scale, (k // cols) * (H * scale + 12)
    m = [int(v, 0) for v in open(f'{src}/img_{i}_meta.txt').read().split()]
    big = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    s.paste(big, (x, y + 12), big)
    d.text((x + 1, y), f'{i} h{m[0]},{m[1]} a{m[2]},{m[3]}', fill=(255, 255, 0))
    d.rectangle((x + m[0]*scale - 1, y + 12 + m[1]*scale - 1, x + m[0]*scale + 1, y + 12 + m[1]*scale + 1), fill=(255, 0, 0))
s.save(out)
