#!/usr/bin/env python3
"""Loading-screen sprite bank: the collectible crystal's spin (ObjectCrystal_317, anim 0 dir 0) as an
ARGB1555 bank independent of the level palettes (levels reset VRAM and palettes while it is shown).

usage: import_loading.py <frames_dir> <images_dir> <out_dir>
Output (game data, gitignored): <out_dir>/loading.tex / loading.spr  (FPS2, one anim "crystal")."""
import json, os, struct, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert import write_tex, pack

def main():
    frames_dir, src, out = sys.argv[1:4]
    d = json.load(open(os.path.join(frames_dir, 'anims.json')))['objectcrystal_317']['anims'][0]['0']
    ids = [i for i in d['frames'] if i is not None]
    uniq = list(dict.fromkeys(ids))
    imgs = {}
    for i in uniq:
        m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()]
        hx, hy = [v - 65536 if v > 32767 else v for v in m[:2]]
        imgs[i] = (Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA'), hx, hy)
    size = 64
    while pack([(i, imgs[i][0]) for i in uniq], size) is None: size *= 2
    pos = pack([(i, imgs[i][0]) for i in uniq], size)
    atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    for i in uniq: atlas.paste(imgs[i][0], pos[i])
    os.makedirs(out, exist_ok=True)
    write_tex(atlas, os.path.join(out, 'loading.tex'))
    data = bytearray(b'FPS2' + struct.pack('<4H', len(uniq), 1, size, size))
    for i in uniq:
        im, hx, hy = imgs[i]
        data += struct.pack('<4H2h', pos[i][0], pos[i][1], im.width, im.height, hx, hy)
    data += b'crystal'.ljust(16, b'\0') + struct.pack('<2H2Bh', 0, len(ids), min(255, d['max_speed']), 0, d['loop'])
    data += struct.pack('<%dH' % len(ids), *[uniq.index(i) for i in ids])
    open(os.path.join(out, 'loading.spr'), 'wb').write(data)
    print('loading bank: %d frames, speed %d, atlas %d' % (len(ids), d['max_speed'], size))

if __name__ == '__main__':
    main()
