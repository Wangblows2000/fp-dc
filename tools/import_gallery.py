#!/usr/bin/env python3
"""
Gallery (original frame index 10 = frame_11.json) data that is not a UI scene: the 16 artworks and the string tables.
The scene itself (cards, icons, header, DATA rows, cursor ...) comes from tools/import_ui.py (ASSET_PIPELINE.md
"Session 15: Gallery").

usage: import_gallery.py <frames_dir> <images_dir> <chowdren_binary> <decompiled_dir> <out_dir>
  frames_dir      build/frames (anims.json: Artwork_287's 16 frames)
  images_dir      extracted/linux/images (img_N.png)
  chowdren_binary the original Linux executable (bin64/Chowdren): the List* text constructors' string literals
  decompiled_dir  build/re (ListSongFilenames_285 / ListSongs_283 / ListNames_272 / ListDescriptions_273 /
                  ListWarnings_281 constructors)
  out_dir         converted2/gal (game data, gitignored; loaded on demand, kept loose on the disc)

Output:
  art_NN.tex  Artwork frame NN (card "4NN"): FPTX fmt 1 (PAL8, palette group 0), width rounded up to a power of two,
              height 256; the image (129-347 x 240) sits at the top-left. Index 0 is transparent (padding).
  art_NN.pal  its palette: 'FPPL' u16 group 0, u16 count, 256 x ARGB1555 (255 colours, median cut, no dithering)
  gal_text.txt  'FPGT1 100 56 9' then 100 card files, 100 card labels, 56 achievement names, 56 descriptions and the
              PC warnings / hints 0..8 (ListWarnings paragraphs, spaces kept: the original lays the hint line out with
              spaces in a 50-column box). Card file field = type digit + name: 0 music (loop), 1 music (once), 2 sound,
              3 music after a game clear, 4NN art frame NN. Sound cards without a file in the original play a sample
              (RV t_gal6): written as 2s<id>.
"""
import json, os, re, struct, sys
from PIL import Image

SAMPLE_FOR_CARD = {35: 188, 68: 362, 75: 0, 76: 57, 77: 3, 78: 2, 79: 1}   # RV t_gal6 (spec_gallery 3.2)

def elf_strings(binary, src):
    d = open(binary, 'rb').read()
    shoff, = struct.unpack_from('<Q', d, 0x28); shentsize, shnum = struct.unpack_from('<HH', d, 0x3a)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        typ, = struct.unpack_from('<I', d, o + 4); addr, off, size = struct.unpack_from('<QQQ', d, o + 0x10)
        if addr and typ != 8: secs.append((addr, off, size))
    def cstr(va):
        for a, o, s in secs:
            if a <= va < a + s:
                e = d.index(b'\0', o + va - a); return d[o + va - a:e].decode('latin1')
        raise SystemExit('string at %x not found' % va)
    txt = open(src).read()
    out = [cstr(int(m.group(1), 16)) for m in re.finditer(r'&(?:UNK|DAT|s_\w+?)_?([0-9a-f]{6,})', txt)]
    return out[1:]          # [0] is the font name ('Arial')

def argb1555(r, g, b, a=255):
    return ((1 if a >= 128 else 0) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)

def pow2(n):
    p = 8
    while p < n: p *= 2
    return p

def art(images, img_id, out_base):
    im = Image.open(os.path.join(images, 'img_%d.png' % img_id)).convert('RGBA')
    w, h = im.size
    rgb = im.convert('RGB')
    q = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:255 * 3]
    idx = [v + 1 for v in q.tobytes()]                       # 0 = transparent padding
    alpha = im.getchannel('A').tobytes()
    idx = [0 if alpha[i] < 128 else v for i, v in enumerate(idx)]
    tw, th = pow2(w), pow2(h)
    data = bytearray(tw * th)
    for y in range(h): data[y * tw:y * tw + w] = bytes(idx[y * w:(y + 1) * w])
    open(out_base + '.tex', 'wb').write(b'FPTX' + struct.pack('<6H', tw, th, 1, w, h, 0) + bytes(data))
    p = [0] + [argb1555(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]) for i in range(len(pal) // 3)]
    p += [0] * (256 - len(p))
    open(out_base + '.pal', 'wb').write(b'FPPL' + struct.pack('<2H', 0, 256) + struct.pack('<256H', *p))
    return w, h, tw, th

def main():
    frames, images, binary, re_dir, out = sys.argv[1:6]
    os.makedirs(out, exist_ok=True)
    an = json.load(open(os.path.join(frames, 'anims.json')))
    arts = an['artwork_287']['anims'][0]['0']['frames']
    tot = 0
    for n, img in enumerate(arts):
        w, h, tw, th = art(images, img, os.path.join(out, 'art_%02d' % n))
        tot += tw * th
        print('art_%02d: img %d %dx%d -> %dx%d PAL8 (%d KB)' % (n, img, w, h, tw, th, tw * th // 1024))
    print('%d artworks, %d KB of PAL8 texels (one resident at a time)' % (len(arts), tot // 1024))
    L = lambda cls: elf_strings(binary, os.path.join(re_dir, '%s__%s_int__int.c' % (cls, cls)))
    files, songs, names, descs, warns = L('ListSongFilenames_285'), L('ListSongs_283'), L('ListNames_272'), L('ListDescriptions_273'), L('ListWarnings_281')
    assert len(files) == 100 and len(songs) == 100 and len(names) == 56 and len(descs) == 56 and len(warns) >= 9, (len(files), len(songs), len(names), len(descs), len(warns))
    cf = []
    for i, f in enumerate(files):
        f = f.replace('\\', '/')
        if f == '2': f = '2s%d' % SAMPLE_FOR_CARD[i + 1]
        elif f[0] == '2': f = '2' + f.rsplit('/', 1)[-1]
        cf.append(f)
    lines = ['FPGT1 100 56 9'] + cf + songs + names + descs + warns[:9]
    for s in lines: assert '\n' not in s and '\r' not in s and all(32 <= ord(c) < 127 for c in s), repr(s)
    open(os.path.join(out, 'gal_text.txt'), 'w').write('\n'.join(lines) + '\n')
    print('gal_text.txt: %d lines' % len(lines))

if __name__ == '__main__':
    main()
