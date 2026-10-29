#!/usr/bin/env python3
"""Original menu/hint font (Data/font.bmp: 256x60, 8x20 cells, 32 glyphs per row from ' ') -> sprite
bank ui/font (one frame per glyph, black = transparent). usage: import_font.py <font.bmp> <out_dir>
The Windows (GOG) 1.21.5 Data/font.bmp is a 256x2280 sheet: the same three rows of ASCII 32-127 first, then accented
Latin, kana and kanji for the other languages (observed, session 25: the user's build stopped with "texture too large
256x4096"). The port uses the 96 ASCII glyphs: only the top 256x60 is kept, whichever file it is."""
import os, struct, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert import write_tex
src, out = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGBA')
if im.width < 256 or im.height < 60: sys.exit('%s: %dx%d is not the Freedom Planet font (256x60 or larger)' % (src, im.width, im.height))
im = im.crop((0, 0, 256, 60))          # the 96 ASCII glyphs (3 rows of 32, 8x20)
px = im.load()
for y in range(im.height):
    for x in range(im.width):
        r, g, b, a = px[x, y]
        if r < 8 and g < 8 and b < 8: px[x, y] = (0, 0, 0, 0)
os.makedirs(out, exist_ok=True)
write_tex(im, os.path.join(out, 'font.tex'))
n = 96
data = bytearray(b'FPS2' + struct.pack('<4H', n, 1, 256, 64))
for i in range(n):
    data += struct.pack('<4H2h', (i % 32) * 8, (i // 32) * 20, 8, 20, 0, 0)
data += b'font'.ljust(16, b'\0') + struct.pack('<2H2Bh', 0, n, 0, 0, -1)
data += struct.pack('<%dH' % n, *range(n))
open(os.path.join(out, 'font.spr'), 'wb').write(data)
print('font: %d glyphs' % n)
