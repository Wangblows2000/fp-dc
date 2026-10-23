#!/usr/bin/env python3
"""Port credit on the title screen (intentional change, not from the original).
The copyright backdrop (image 16622, 258x20) has two lines in a 6-row italic font: white strokes with a dark
shadow at (-1, +1); rows 0-2 of each glyph lean 1 px right. Line 2 is replaced with CREDIT, built from the
image's own glyphs; N and W do not occur in it and are drawn in the same style.
usage (library): patch(im) -> new RGBA image.   self-test: title_credit.py <img_16622.png> <out.png>"""
import sys
from PIL import Image

CREDIT = 'DREAMCASTED BY WANGBLOWSME'
IMAGE_ID = 16622
WHITE, SHADOW = (248, 252, 248, 255), (8, 0, 0, 255)
# upright glyph sources in the original: (line top y, x0, x1)
SRC = {'D': (13, 116, 121), 'R': (13, 54, 59), 'E': (13, 109, 114), 'A': (13, 88, 93), 'M': (13, 194, 200),
       'C': (4, 111, 116), 'S': (13, 209, 214), 'T': (13, 41, 45), 'B': (13, 61, 66), 'Y': (4, 51, 56),
       'G': (13, 179, 185), 'L': (4, 30, 34), 'O': (13, 68, 73)}
DRAWN = {'N': ['##..##', '###.##', '######', '##.###', '##..##', '##..##'],
         'W': ['##...##', '##...##', '##.#.##', '#######', '###.###', '##...##']}

def _white(im, x, y):
    W, H = im.size
    if not (0 <= x < W and 0 <= y < H): return False
    p = im.getpixel((x, y)); return p[3] and p[0] > 128

def _glyph(im, ch):
    if ch in DRAWN: return [[c == '#' for c in row] for row in DRAWN[ch]]
    y0, a, b = SRC[ch]
    return [[_white(im, x + (1 if r < 3 else 0), y0 + r) for x in range(a, b + 1)] for r in range(6)]

def patch(im):
    im = im.convert('RGBA').copy()
    W, H = im.size
    glyphs = [None if c == ' ' else _glyph(im, c) for c in CREDIT]
    width = sum(6 if g is None else len(g[0]) + 1 for g in glyphs) - 1
    for y in range(12, H):
        for x in range(W): im.putpixel((x, y), (0, 0, 0, 0))
    x, y0 = (W - width) // 2, 13
    white = set()
    for g in glyphs:
        if g is None: x += 6; continue
        for r in range(6):
            for c, on in enumerate(g[r]):
                if on: white.add((x + c + (1 if r < 3 else 0), y0 + r))
        x += len(g[0]) + 1
    for (px, py) in white:
        s = (px - 1, py + 1)
        if s not in white and 0 <= s[0] < W and s[1] < H: im.putpixel(s, SHADOW)
    for (px, py) in white: im.putpixel((px, py), WHITE)
    return im

if __name__ == '__main__':
    out = patch(Image.open(sys.argv[1]))
    out.save(sys.argv[2])
