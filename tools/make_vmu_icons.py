#!/usr/bin/env python3
"""VMU art from the original images (game data; output is gitignored, never commit it).

usage: make_vmu_icons.py <images_dir> <out_dir> [preview.png]
Writes:
  <out_dir>/vmu_lcd.bin   'FVL1' u16 nframes, u16 ncrystal; nframes x 192 bytes (48x32 1bpp, row-major,
                          first byte = top-left, MSB = leftmost pixel, 1 = dark LCD pixel).
                          frames 0..ncrystal-1: crystal spin (ObjectCrystal_317) for title/menus;
                          then one life icon per character (GUIIcon_346 directions 0..4:
                          Lilac, Carol, Carol (bike), Milla, Torque/Spade) - the head part of the strip.
  <out_dir>/vmu_icon.bin  'FVI1' u16 nframes, u16 speed; u16 palette[16] (ARGB4444);
                          nframes x 512 bytes (32x32 4bpp, high nibble = left pixel): save-file icon
                          shown in the Dreamcast file manager (the crystal spin).
The LCD has no grey: shapes get a 1 px dark outline, dark shades are solid, mid shades a 2x2 checker
and light shades/highlights stay clear, so the silhouette and facets survive at this size."""
import os, struct, sys
from PIL import Image

CRYSTAL = [4752, 4913, 4914, 4915, 4916, 4917]          # objectcrystal_317 anim 0 dir 0
HEADS = [311, 112, 395, 507, 5116]                       # guiicon_346 dirs 0,1,2,4,5 (strip 64x12)

def load(src, i):
    return Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')

def lum(p):
    r, g, b, a = p
    return (0.299 * r + 0.587 * g + 0.114 * b) / 255.0

def to_1bpp(im, w=48, h=32, ox=None, oy=None, scale=1):
    """im: RGBA sprite -> 48x32 list of 0/1 rows, sprite scaled (nearest) and centred."""
    if scale != 1:
        im = im.resize((round(im.width * scale), round(im.height * scale)), Image.NEAREST)
    W, H = im.size
    ox = (w - W) // 2 if ox is None else ox
    oy = (h - H) // 2 if oy is None else oy
    px = im.load()
    solid = lambda x, y: 0 <= x < W and 0 <= y < H and px[x, y][3] > 0
    out = [[0] * w for _ in range(h)]
    for y in range(H):
        for x in range(W):
            if not solid(x, y): continue
            X, Y = x + ox, y + oy
            if not (0 <= X < w and 0 <= Y < h): continue
            edge = not (solid(x - 1, y) and solid(x + 1, y) and solid(x, y - 1) and solid(x, y + 1))
            l = lum(px[x, y])
            if edge or l < 0.30: v = 1
            elif l < 0.62: v = (X + Y) & 1
            else: v = 0
            out[Y][X] = v
    return out

def _dist(a, b):
    if (a[3] > 0) != (b[3] > 0): return 999
    if a[3] == 0: return 0
    return sum((x - y) ** 2 for x, y in zip(a[:3], b[:3])) ** 0.5

def lineart(im, S, ox, thr=130, dark=0.30):
    """Pixel art scaled by S as line art: a 1 px line where neighbouring source colours differ strongly
    (face/hair/eye boundaries), the outer outline, and very dark pixels filled."""
    W, H = im.size; px = im.load()
    g = lambda x, y: px[x, y] if 0 <= x < W and 0 <= y < H else (0, 0, 0, 0)
    out = [[0] * 48 for _ in range(32)]; oy = (32 - H * S) // 2
    for y in range(H):
        for x in range(W):
            p = g(x, y)
            if p[3] == 0: continue
            for dy in range(S):
                for dx in range(S):
                    v = (dx == 0 and _dist(p, g(x - 1, y)) > thr) or (dy == 0 and _dist(p, g(x, y - 1)) > thr) or \
                        (dx == S - 1 and g(x + 1, y)[3] == 0) or (dy == S - 1 and g(x, y + 1)[3] == 0) or lum(p) < dark
                    X, Y = x * S + dx + ox, y * S + dy + oy
                    if 0 <= X < 48 and 0 <= Y < 32: out[Y][X] = 1 if v else 0
    return out

def pack(bits):
    b = bytearray()
    for row in bits:
        for xb in range(0, 48, 8):
            v = 0
            for k in range(8): v = v << 1 | row[xb + k]
            b.append(v)
    return bytes(b)

def sparkle(bits, x, y, r):
    for d in range(-r, r + 1):
        for (X, Y) in ((x + d, y), (x, y + d)):
            if 0 <= X < 48 and 0 <= Y < 32: bits[Y][X] = 1

def main():
    src, out = sys.argv[1], sys.argv[2]
    prev = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(out, exist_ok=True)
    frames = []
    # crystal: 1.5x (11x21 -> 16x31), a sparkle that twinkles round it
    spk = [(10, 6, 2), (37, 10, 1), (38, 24, 2), (9, 25, 1), (12, 15, 1), (36, 17, 1)]
    for k, i in enumerate(CRYSTAL):
        bits = to_1bpp(load(src, i), scale=1.5)
        x, y, r = spk[k % len(spk)]
        sparkle(bits, x, y, r)
        frames.append(bits)
    nc = len(frames)
    for i in HEADS:
        head = load(src, i).crop((0, 0, 15, 12))       # head part of the GUIIcon strip
        frames.append(lineart(head, 2, 0))             # 30x24 on the left; the game draws "x NN" lives at the right
    data = b'FVL1' + struct.pack('<2H', len(frames), nc) + b''.join(pack(f) for f in frames)
    open(os.path.join(out, 'vmu_lcd.bin'), 'wb').write(data)
    # save-file icon: 32x32, 16 colours (ARGB4444), crystal spin (3 frames to keep the file small)
    pal = [0x0000]; idx = {}
    icons = []
    for i in CRYSTAL[::2]:
        im = load(src, i).resize((16, 31), Image.NEAREST)
        c = Image.new('RGBA', (32, 32), (0, 0, 0, 0)); c.paste(im, (8, 0), im)
        q = c.convert('RGB').quantize(15) if False else None
        icons.append(c)
    # shared palette from all frames
    allpx = {}
    for c in icons:
        for p in c.get_flattened_data():
            if p[3]: allpx[p[:3]] = allpx.get(p[:3], 0) + 1
    cols = sorted(allpx, key=lambda k: -allpx[k])[:15]
    def near(rgb): return min(range(len(cols)), key=lambda j: sum((a - b) ** 2 for a, b in zip(cols[j], rgb)))
    pal += [0xf000 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4) for r, g, b in cols]
    pal += [0] * (16 - len(pal))
    ib = bytearray()
    for c in icons:
        px = list(c.get_flattened_data())
        for y in range(32):
            for x in range(0, 32, 2):
                a, b = px[y * 32 + x], px[y * 32 + x + 1]
                ia = near(a[:3]) + 1 if a[3] else 0
                ibb = near(b[:3]) + 1 if b[3] else 0
                ib.append(ia << 4 | ibb)
    open(os.path.join(out, 'vmu_icon.bin'), 'wb').write(b'FVI1' + struct.pack('<2H', len(icons), 10) + struct.pack('<16H', *pal) + bytes(ib))
    print('vmu_lcd.bin: %d frames (%d crystal), vmu_icon.bin: %d frames, %d colours' % (len(frames), nc, len(icons), len(cols)))
    if prev:   # LCD look: dark pixels on a pale green panel, 4x
        P = Image.new('RGB', ((48 * 4 + 8) * len(frames), 32 * 4 + 8), (40, 40, 40))
        for k, f in enumerate(frames):
            for y in range(32):
                for x in range(48):
                    col = (30, 40, 60) if f[y][x] else (170, 200, 170)
                    for dy in range(4):
                        for dx in range(4):
                            P.putpixel((k * (48 * 4 + 8) + 4 + x * 4 + dx, 4 + y * 4 + dy), col)
        P.save(prev)

if __name__ == '__main__':
    main()
