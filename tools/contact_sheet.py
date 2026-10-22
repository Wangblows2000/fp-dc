#!/usr/bin/env python3
"""Make contact sheets of extracted images for visual identification.
usage: contact_sheet.py <img_dir> <out_dir> <start> <end> [per_sheet] [cell]"""
import sys, os
from PIL import Image, ImageDraw
src, out, a, b = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
per = int(sys.argv[5]) if len(sys.argv) > 5 else 400
cell = int(sys.argv[6]) if len(sys.argv) > 6 else 64
os.makedirs(out, exist_ok=True)
cols = 20
for s in range(a, b, per):
    ids = range(s, min(s + per, b))
    rows = (len(ids) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell, rows * (cell + 10)), (60, 0, 60))
    d = ImageDraw.Draw(sheet)
    for k, i in enumerate(ids):
        p = os.path.join(src, 'img_%d.png' % i)
        if not os.path.exists(p): continue
        im = Image.open(p).convert('RGBA'); im.thumbnail((cell, cell))
        x, y = (k % cols) * cell, (k // cols) * (cell + 10)
        sheet.paste(im, (x, y + 10), im)
        d.text((x + 1, y), str(i), fill=(255, 255, 0))
    sheet.save(os.path.join(out, 'sheet_%05d.png' % s))
