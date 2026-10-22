#!/usr/bin/env python3
"""Render a recovered frame layout to PNG for verification.
usage: render_frame.py <frames_dir> <images_dir> <frame_no> <out.png> [scale] [--solid]
Backdrop pieces: image placed at (x - hotspot_x, y - hotspot_y). Active objects: their
first animation frame (anim 0 dir 0) if known, else a magenta marker.
--solid tints pieces whose obstacle type (field 0xac & 3) == 1 red, ladders (==3) blue,
and draws non-obstacle pieces dimmed, to test the collision-type hypothesis."""
import json, sys, os
from PIL import Image, ImageDraw, ImageEnhance
fd, imgdir, n, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
scale = float(sys.argv[5]) if len(sys.argv) > 5 and not sys.argv[5].startswith('-') else 0.25
solid_mode = '--solid' in sys.argv
fr = json.load(open(os.path.join(fd, 'frame_%d.json' % n)))
bd = json.load(open(os.path.join(fd, 'backdrops.json')))
an = json.load(open(os.path.join(fd, 'anims.json')))
W, H = int(fr['width'] * scale), int(fr['height'] * scale)
canvas = Image.new('RGBA', (W, H), (70, 110, 160, 255))
cache = {}
def img(i):
    if i not in cache:
        p = os.path.join(imgdir, 'img_%d.png' % i)
        meta = [int(v, 0) for v in open(os.path.join(imgdir, 'img_%d_meta.txt' % i)).read().split()]
        hx, hy = [v - 65536 if v > 32767 else v for v in meta[:2]]
        cache[i] = (Image.open(p).convert('RGBA'), hx, hy)
    return cache[i]
def first_frame(key):
    a = an.get(key, {}).get('anims')
    if not a: return None
    for ds in a:
        if ds:
            d = ds.get('0') or next(iter(ds.values()))
            if d['frames'] and d['frames'][0] is not None: return d['frames'][0]
    return None
order = sorted(range(len(fr['objects'])), key=lambda k: (fr['objects'][k]['layer'] or 0, k))
dr = ImageDraw.Draw(canvas)
for k in order:
    o = fr['objects'][k]
    key = '%s_%d' % (o['cls'], o['id'])
    if o['x'] is None: continue
    if key in bd:
        b = bd[key]
        if b['image'] is None: continue
        im, hx, hy = img(b['image'])
        if solid_mode:
            t = (b['fields'].get('0xac') or 0) & 3
            if t == 1: im = Image.blend(im, Image.new('RGBA', im.size, (255, 0, 0, 255)), 0.45).putalpha(im.getchannel('A')) or Image.composite(Image.blend(im, Image.new('RGBA', im.size, (255, 0, 0, 255)), 0.45), Image.new('RGBA', im.size, (0, 0, 0, 0)), im.getchannel('A'))
            elif t == 3: im = Image.composite(Image.blend(im, Image.new('RGBA', im.size, (0, 0, 255, 255)), 0.5), Image.new('RGBA', im.size, (0, 0, 0, 0)), im.getchannel('A'))
            else:
                a = im.getchannel('A').point(lambda v: v // 3); im = im.copy(); im.putalpha(a)
    else:
        ff = first_frame(key)
        if ff is None:
            x, y = o['x'] * scale, o['y'] * scale
            dr.rectangle((x - 1, y - 1, x + 1, y + 1), fill=(255, 0, 255, 255)); continue
        im, hx, hy = img(ff)
    x, y = (o['x'] - hx) * scale, (o['y'] - hy) * scale
    if scale != 1:
        w, h = max(1, round(im.width * scale)), max(1, round(im.height * scale))
        im = im.resize((w, h), Image.NEAREST)
    canvas.alpha_composite(im, (int(round(x)), int(round(y)))) if 0 <= x < W and 0 <= y < H else canvas.paste(im, (int(round(x)), int(round(y))), im)
canvas.convert('RGB').save(out)
print(out, W, H)
