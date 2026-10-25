#!/usr/bin/env python3
"""
Parallax textures that palettize.py left as ARGB1555 (a --pal4 set with images of more than 15 colours: Fortune Night's
night sky and mall backdrops, session 17) become PAL8 in the free top of palette group 2, the character group.

Group 2 holds the playing character's palette: pal2.pal (Lilac), pal2c.pal (Carol), pal2m.pal (Milla); the runtime loads
one of them (game.c game_load_level). Their used entries (the count field) are below `base`; entries base..255 get the
parallax colours in all three files, so the parallax looks the same with every character and Lilac's resident banks keep
their bytes (only the palette files change). When the parallax needs more colours than the free top holds, its colours
are reduced by a median cut over all its pixels (reported); otherwise the conversion is lossless.

usage: par_top.py <level_dir> <regex>     (e.g. '_p[xw]'; only textures still in ARGB1555 are taken)
"""
import os, re, struct, sys
from PIL import Image

def main():
    d, rx = sys.argv[1], re.compile(sys.argv[2])
    files = []
    for f in sorted(os.listdir(d)):
        if not f.endswith('.tex') or not rx.search(f): continue
        raw = open(os.path.join(d, f), 'rb').read()
        w, h, fmt, sw, sh, _ = struct.unpack_from('<6H', raw, 4)
        if fmt == 0: files.append((f, w, h, sw, sh, struct.unpack_from('<%dH' % (w * h), raw, 16)))
    if not files: return
    pals = {}
    for nm in ('pal2.pal', 'pal2c.pal', 'pal2m.pal'):
        p = os.path.join(d, nm)
        if os.path.exists(p):
            raw = open(p, 'rb').read(); g, cnt = struct.unpack_from('<2H', raw, 4)
            pals[nm] = (cnt, list(struct.unpack_from('<256H', raw, 8)))
    base = max(c for c, _ in pals.values())
    room = 256 - base
    hist = {}
    for f, w, h, sw, sh, px in files:
        for q in px:
            if q & 0x8000: hist[q] = hist.get(q, 0) + 1
    cols = sorted(hist)
    if len(cols) <= room:
        pal = cols; remap = {q: q for q in cols}
    else:
        # median cut (PIL) over the colour histogram: an image of every colour repeated by its weight (capped)
        seq = []
        for q in cols: seq += [q] * min(hist[q], 64)
        im = Image.new('RGB', (len(seq), 1))
        im.putdata([(((q >> 10) & 31) << 3, ((q >> 5) & 31) << 3, (q & 31) << 3) for q in seq])
        qi = im.quantize(colors=room, method=Image.Quantize.MEDIANCUT)
        pl = qi.getpalette()[:3 * room]
        pal = sorted({0x8000 | ((pl[3 * i] >> 3) << 10) | ((pl[3 * i + 1] >> 3) << 5) | (pl[3 * i + 2] >> 3) for i in range(len(pl) // 3)})
        def rgb(q): return ((q >> 10) & 31, (q >> 5) & 31, q & 31)
        remap = {q: min(pal, key=lambda r: sum((a - b) ** 2 for a, b in zip(rgb(q), rgb(r)))) for q in cols}
        print('par_top: %d parallax colours reduced to %d (group 2 top %d..255)' % (len(cols), len(pal), base))
    index = {c: base + i for i, c in enumerate(pal)}
    for f, w, h, sw, sh, px in files:
        idx = bytes(index[remap[q]] if q & 0x8000 else 0 for q in px)
        open(os.path.join(d, f), 'wb').write(b'FPTX' + struct.pack('<6H', w, h, 1, sw, sh, 2) + idx)
    for nm, (cnt, p) in pals.items():
        for i, c in enumerate(pal): p[base + i] = c
        open(os.path.join(d, nm), 'wb').write(b'FPPL' + struct.pack('<2H', 2, base + len(pal)) + struct.pack('<256H', *p))
    print('par_top: %d textures, %d colours at group 2 entries %d..%d (%s)' % (len(files), len(pal), base, base + len(pal) - 1, ', '.join(sorted(pals))))

if __name__ == '__main__':
    main()
