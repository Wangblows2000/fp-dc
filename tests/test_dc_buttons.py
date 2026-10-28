#!/usr/bin/env python3
"""Dreamcast button labels (D-110, session 22): the original's A / B / C (jump / attack / special) are the pad's A / B / X,
and every player-facing "C" is shown as X.
Checks: the pad mapping in src/platform/plat_dc.c (X = special, B = attack, A = jump); no "C" button hint left in the
port's own strings; the gallery hint transform (tools/import_gallery.py dc_labels) on the original's strings; Milla's HUD
glyph edit (tools/import_frame.py dc_hud_c_to_x) on the original images (only the glyph cell changes, no new colour, an X
shape); and, when the gallery data was regenerated, its hints.
usage: test_dc_buttons.py [data_dir]   (default: converted2; images from extracted/linux/images when present)"""
import os, re, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, 'tools'))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'converted2')
IMAGES = os.path.join(ROOT, 'extracted', 'linux', 'images')

fails = 0
def check(name, cond, info=''):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name + ('  [' + info + ']' if info else ''))
    fails += 0 if cond else 1

# ---- the pad: the action the original labels C (special) is on X; B (attack) on B; A (jump) on A
src = open(os.path.join(ROOT, 'src', 'platform', 'plat_dc.c')).read()
pad = dict(re.findall(r'if \(b & CONT_([A-Z]+)\) held \|= (BTN_[A-Z]+);', src))
check('pad_x_is_special', pad.get('X') == 'BTN_SPECIAL', str(pad))
check('pad_b_is_attack', pad.get('B') == 'BTN_ATTACK', str(pad))
check('pad_a_is_jump', pad.get('A') == 'BTN_JUMP', str(pad))
# the gallery closes a card with special (so "X - Close Card" is the X button)
gal = open(os.path.join(ROOT, 'src', 'game', 'fe_gallery.inc')).read()
check('gallery_close_is_special', re.search(r'special = p & BTN_SPECIAL', gal) and 'gal_card_action(10 * Y.D + Y.col + 1, !jump)' in gal)

# ---- the port's own player-facing strings: no "C" button hint (fe_text / ui text calls)
hints = []
for f in glob.glob(os.path.join(ROOT, 'src', '**', '*.[ch]'), recursive=True) + glob.glob(os.path.join(ROOT, 'src', '**', '*.inc'), recursive=True):
    for ln in open(f, errors='replace'):
        for s in re.findall(r'fe_text\([^"]*"([^"]*)"', ln):
            if re.search(r'(^|[^A-Za-z])C( -|:| to | button)', s): hints.append((os.path.basename(f), s))
check('no_c_hint_in_port_strings', not hints, str(hints))

# ---- gallery hints (ListWarnings): "C - Close Card" -> "X - Close Card", same length, nothing else touched
from import_gallery import dc_labels
orig = ['A - Play                           C - Close Card', 'A - Stop                           C - Close Card',
        'A - View                           C - Close Card', 'Press A to open this card!', 'Clear Jade Creek in 8 minutes',
        'Destroy a Chopper with a single Dragon Boost as Lilac']
new = [dc_labels(s) for s in orig]
check('gallery_hint_c_to_x', new[:3] == [s.replace('C - Close', 'X - Close') for s in orig[:3]] and all(len(a) == len(b) for a, b in zip(orig, new)), str(new[:3]))
check('gallery_other_text_unchanged', new[3:] == orig[3:], str(new[3:]))
gt = os.path.join(DATA, 'gal', 'gal_text.txt')
if os.path.exists(gt):
    lines = open(gt).read().splitlines()
    if any('C - Close Card' in l for l in lines):
        print('NOTE gallery data not regenerated yet (tools/import_gallery.py, ASSET_PIPELINE "Session 22"): %s' % gt)
    else:
        check('gallery_data_hints_x', sum('X - Close Card' in l for l in lines) == 3)

# ---- Milla's HUD panel: the C glyph redrawn as X (GUIScoreTimeRings_345 direction 4 frames)
if os.path.isdir(IMAGES):
    from PIL import Image
    from import_frame import dc_hud_c_to_x, DC_X_GLYPH
    for i in (2483, 2484, 2485, 2486, 2803, 2804, 2805, 2806, 4235):
        a = Image.open(os.path.join(IMAGES, 'img_%d.png' % i)).convert('RGBA')
        b = dc_hud_c_to_x(a)
        pa, pb = a.load(), b.load()
        changed = [(x, y) for y in range(a.height) for x in range(a.width) if pa[x, y] != pb[x, y]]
        inside = all(90 <= x <= 98 and 0 <= y <= 13 for x, y in changed)
        newcol = {pb[x, y] for x, y in changed if pb[x, y][3]} - {pa[x, y] for y in range(a.height) for x in range(a.width)}
        shape = all((pb[91 + gx, 2 + gy][3] == 255 and pb[91 + gx, 2 + gy] != pb[90, 4]) == (c == 'X')
                    for gy, row in enumerate(DC_X_GLYPH) for gx, c in enumerate(row))
        check('milla_hud_%d_c_to_x' % i, changed and inside and not newcol and shape and a.getbbox() == b.getbbox(),
              '%d px changed, inside %s, new colours %s, shape %s' % (len(changed), inside, newcol, shape))
else:
    print('NOTE %s not present: HUD glyph checks skipped' % IMAGES)

print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
