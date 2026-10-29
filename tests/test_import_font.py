#!/usr/bin/env python3
"""tools/import_font.py with both font.bmp layouts (session 25): the Linux 256x60 sheet and the Windows (GOG) 256x2280 one
(ASCII rows first, then other languages). A synthetic sheet stands in for the game's file: the font texture must be 256x64
and the same for the same top 60 rows, whatever follows below. usage: test_import_font.py"""
import os, subprocess, sys, tempfile
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, '..', 'tools', 'import_font.py')
fails = 0
def check(name, cond):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name); fails += 0 if cond else 1
with tempfile.TemporaryDirectory() as t:
    top = Image.new('RGB', (256, 60))
    for i in range(96):
        x, y = (i % 32) * 8, (i // 32) * 20
        for k in range(8): top.putpixel((x + k % 8, y + 2 + i % 16), (200, 220, 255))
    small = os.path.join(t, 'small.bmp'); top.save(small)
    big = Image.new('RGB', (256, 2280), (90, 90, 90)); big.paste(top, (0, 0))
    bigp = os.path.join(t, 'big.bmp'); big.save(bigp)
    r1 = subprocess.run([sys.executable, TOOL, small, os.path.join(t, 'a')], capture_output=True, text=True)
    r2 = subprocess.run([sys.executable, TOOL, bigp, os.path.join(t, 'b')], capture_output=True, text=True)
    check('linux_256x60', r1.returncode == 0)
    check('windows_256x2280', r2.returncode == 0)
    if r1.returncode == 0 and r2.returncode == 0:
        a = open(os.path.join(t, 'a', 'font.tex'), 'rb').read(); b = open(os.path.join(t, 'b', 'font.tex'), 'rb').read()
        check('same_texture', a == b)
        check('size_256x64', a[4:8] == bytes([0, 1, 64, 0]))
    tiny = os.path.join(t, 'tiny.bmp'); Image.new('RGB', (100, 20)).save(tiny)
    check('rejects_other_image', subprocess.run([sys.executable, TOOL, tiny, os.path.join(t, 'c')], capture_output=True).returncode != 0)
print('%d failures' % fails)
sys.exit(1 if fails else 0)
