#!/usr/bin/env python3
"""Contact sheet of Assets.dat image ids with labels. usage: contact_ids.py out.png scale id..."""
import sys
from PIL import Image, ImageDraw
out, sc, ids = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = [(i, Image.open('extracted/linux/images/img_%s.png' % i).convert('RGBA')) for i in ids]
W = 1400; x = y = 0; rowh = 0; pos = []
for i, im in ims:
    w, h = im.width * sc, im.height * sc + 12
    if x + w > W: x = 0; y += rowh + 6; rowh = 0
    pos.append((x, y)); x += max(w, 40) + 8; rowh = max(rowh, h)
sheet = Image.new('RGBA', (W, y + rowh + 6), (70, 70, 90, 255)); d = ImageDraw.Draw(sheet)
for (i, im), (x, y) in zip(ims, pos):
    d.text((x, y), str(i), fill=(255, 255, 0, 255))
    big = im.resize((im.width * sc, im.height * sc), Image.NEAREST)
    sheet.alpha_composite(big, (x, y + 12))
sheet.save(out)
