#!/usr/bin/env python3
"""
fpdc asset converter: extracted Freedom Planet resources -> Dreamcast-ready data.

usage: convert.py <extracted_assets_dir> <bgm_dir> <out_dir> [--no-music]

Inputs (read-only):
  extracted_assets_dir  output of third_party/fp-assets/fp-assets.py -x
  bgm_dir               the game's Data/BGM directory (ogg vorbis)
Driven by tools/slice_manifest.json (which image/sound ids to use).

Outputs (all little-endian, matches SH-4):
  *.tex   'FPTX' u16 w, u16 h, u16 fmt(0=ARGB1555), u16 srcw, u16 srch, u16 pad,
          then w*h u16 texels, row-major, non-twiddled. w,h are powers of two.
  *.spr   'FPSB' sprite bank (see write_bank)
  *.lvl   'FPLV' level (see convert_level)
  sfx_*.wav  16-bit mono PCM, for snd_sfx_load
  *.pcm   raw signed 16-bit stereo interleaved PCM at 22050 Hz (music stream)
  manifest.json + conversion report on stdout.
Deterministic: same inputs -> byte-identical outputs.
"""
import json, os, struct, subprocess, sys, hashlib
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def pow2(n):
    p = 8
    while p < n:
        p *= 2
    return p


def argb1555(px):
    r, g, b, a = px
    return ((1 if a >= 128 else 0) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


def write_tex(img, path):
    img = img.convert('RGBA')
    w, h = pow2(img.width), pow2(img.height)
    if w > 1024 or h > 1024:
        raise ValueError('%s: texture too large %dx%d' % (path, w, h))
    canvas = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    canvas.paste(img, (0, 0))
    data = canvas.tobytes()
    out = bytearray(b'FPTX' + struct.pack('<6H', w, h, 0, img.width, img.height, 0))
    for i in range(0, len(data), 4):
        out += struct.pack('<H', argb1555(data[i:i + 4]))
    with open(path, 'wb') as f:
        f.write(out)
    return w, h


def load_img(src, idx):
    p = os.path.join(src, 'images', 'img_%d.png' % idx)
    meta = [int(v, 0) for v in open(os.path.join(src, 'images', 'img_%d_meta.txt' % idx)).read().split()]
    if len(meta) < 4:
        raise ValueError('bad meta for image %d' % idx)
    im = Image.open(p).convert('RGBA')
    hx, hy = meta[0], meta[1]
    if hx > 0x7fff: hx -= 0x10000
    if hy > 0x7fff: hy -= 0x10000
    return im, hx, hy


def pack(images, size, height=None):
    """Simple shelf packer. images: list of (key, PIL image). Returns {key:(x,y)}"""
    height = height or size
    order = sorted(images, key=lambda kv: (-kv[1].height, kv[0]))
    x = y = shelf = 0
    pos = {}
    for key, im in order:
        w, h = im.width + 1, im.height + 1  # 1px gutter
        if im.width > size:                 # session 12: wider than the atlas (was placed at x 0 and overflowed)
            return None
        if x + w > size:
            x, y, shelf = 0, y + shelf, 0
        if y + h > height:
            return None
        pos[key] = (x, y)
        x += w
        shelf = max(shelf, h)
    return pos


def pack_skyline(images, size, height=None):
    """Skyline bottom-left packer (session 21, Jade Creek _ens banks: the shelf packer filled 74 % of a page). Same contract
    as pack(): {key: (x, y)} or None; 1 px gutter; tallest first."""
    height = height or size
    order = sorted(images, key=lambda kv: (-kv[1].height, -kv[1].width, kv[0]))
    sky = [(0, 0, size)]                      # segments (x, y, w): the top of the used area
    pos = {}
    for key, im in order:
        w, h = im.width + 1, im.height + 1
        if im.width > size: return None
        best = None
        for i in range(len(sky)):
            x = sky[i][0]
            if x + w > size: break
            y, wl, j = 0, w, i
            while wl > 0:                     # the highest segment under [x, x + w)
                y = max(y, sky[j][1]); wl -= sky[j][2]; j += 1
                if wl > 0 and j >= len(sky): y = None; break
            if y is None or y + h > height: continue
            if best is None or (y, x) < (best[1], best[0]): best = (x, y, i)
        if best is None: return None
        x, y, i = best
        pos[key] = (x, y)
        ns = [(x, y + h, w)]                  # rebuild the skyline over [x, x + w)
        rest = []
        for sx, sy, sw in sky:
            if sx + sw <= x or sx >= x + w: rest.append((sx, sy, sw)); continue
            if sx < x: rest.append((sx, sy, x - sx))
            if sx + sw > x + w: rest.append((x + w, sy, sx + sw - x - w))
        sky = sorted(rest + ns)
        merged = []
        for seg in sky:
            if merged and merged[-1][1] == seg[1] and merged[-1][0] + merged[-1][2] == seg[0]: merged[-1] = (merged[-1][0], seg[1], merged[-1][2] + seg[2])
            else: merged.append(seg)
        sky = merged
    return pos


def pack_skyline_fill(images, size, height=None, order_key=None):
    """pack_skyline that skips what does not fit (session 21): fills one page greedily, largest first; returns the {key: (x, y)}
    of the images it placed (the rest go to the next page)."""
    height = height or size
    order = sorted(images, key=order_key or (lambda kv: (-kv[1].width * kv[1].height, kv[0])))
    sky = [(0, 0, size)]                      # segments (x, y, w): the top of the used area
    pos = {}
    for key, im in order:
        w, h = im.width + 1, im.height + 1
        if im.width > size or im.height > height: continue
        best = None
        for i in range(len(sky)):
            x = sky[i][0]
            if x + w > size: break
            y, wl, j = 0, w, i
            while wl > 0:                     # the highest segment under [x, x + w)
                y = max(y, sky[j][1]); wl -= sky[j][2]; j += 1
                if wl > 0 and j >= len(sky): y = None; break
            if y is None or y + h > height: continue
            if best is None or (y, x) < (best[1], best[0]): best = (x, y, i)
        if best is None: continue
        x, y, i = best
        pos[key] = (x, y)
        ns = [(x, y + h, w)]                  # rebuild the skyline over [x, x + w)
        rest = []
        for sx, sy, sw in sky:
            if sx + sw <= x or sx >= x + w: rest.append((sx, sy, sw)); continue
            if sx < x: rest.append((sx, sy, x - sx))
            if sx + sw > x + w: rest.append((x + w, sy, sx + sw - x - w))
        sky = sorted(rest + ns)
        merged = []
        for seg in sky:
            if merged and merged[-1][1] == seg[1] and merged[-1][0] + merged[-1][2] == seg[0]: merged[-1] = (merged[-1][0], seg[1], merged[-1][2] + seg[2])
            else: merged.append(seg)
        sky = merged
    return pos


def write_bank(src, bank, out_dir, report):
    """Sprite bank: 'FPSB' u16 nframes, u16 nanims, u16 texw, u16 texh
       frame: u16 u, v, w, h; s16 hx, hy                  (12 bytes)
       anim : char name[16]; u16 first, count, speed, loop (24 bytes)
       u16 seq[sum(count)] (indices into frames)"""
    ids = []
    for a in bank['anims']:
        for i in a['frames']:
            if i not in ids:
                ids.append(i)
    imgs = {i: load_img(src, i) for i in ids}
    size = 128
    while True:
        pos = pack([(i, imgs[i][0]) for i in ids], size)
        if pos:
            break
        size *= 2
        if size > 1024:
            raise ValueError('bank %s does not fit in 1024x1024' % bank['name'])
    atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    for i in ids:
        atlas.paste(imgs[i][0], pos[i])
    write_tex(atlas, os.path.join(out_dir, bank['name'] + '.tex'))
    out = bytearray(b'FPSB' + struct.pack('<4H', len(ids), len(bank['anims']), size, size))
    for i in ids:
        im, hx, hy = imgs[i]
        out += struct.pack('<4H2h', pos[i][0], pos[i][1], im.width, im.height, hx, hy)
    seq, first = [], 0
    for a in bank['anims']:
        name = a['name'].encode()[:15].ljust(16, b'\0')
        out += name + struct.pack('<4H', first, len(a['frames']), a.get('speed', 6), 1 if a.get('loop', True) else 0)
        seq += [ids.index(i) for i in a['frames']]
        first += len(a['frames'])
    out += struct.pack('<%dH' % len(seq), *seq)
    with open(os.path.join(out_dir, bank['name'] + '.spr'), 'wb') as f:
        f.write(out)
    report.append('bank %-8s %3d frames %2d anims atlas %dx%d' % (bank['name'], len(ids), len(bank['anims']), size, size))
    return {'frames': ids, 'atlas': size}


TILE_CHARS = {'.': 0, '#': 1, '/': 2, '\\': 3, '=': 4, '^': 5, 'a': 6, 'b': 7, 'd': 8, 'e': 9}
OBJ_CHARS = {'P': 1, 'c': 2, 'h': 3, 'k': 4, 'G': 5}


def convert_level(path, out):
    """Level: 'FPLV' u16 w, u16 h, u16 nobj, u16 pad; u8 tiles[w*h];
       objects: u16 type, s16 x, s16 y, u16 pad (world px, tile centre bottom)"""
    rows = [r.rstrip('\n') for r in open(path) if not r.startswith(';')]
    w = max(len(r) for r in rows)
    h = len(rows)
    tiles = bytearray(w * h)
    objs = []
    for y, r in enumerate(rows):
        for x, ch in enumerate(r.ljust(w, '.')):
            if ch in TILE_CHARS:
                tiles[y * w + x] = TILE_CHARS[ch]
            elif ch in OBJ_CHARS:
                objs.append((OBJ_CHARS[ch], x * 16 + 8, y * 16 + 16))
            else:
                raise ValueError('%s: unknown char %r at %d,%d' % (path, ch, x, y))
    if sum(1 for o in objs if o[0] == 1) != 1:
        raise ValueError('%s: need exactly one player start P' % path)
    data = bytearray(b'FPLV' + struct.pack('<4H', w, h, len(objs), 0)) + tiles
    for t, x, y in objs:
        data += struct.pack('<Hhhh', t, x, y, 0)
    with open(out, 'wb') as f:
        f.write(data)
    return w, h, len(objs)


def ffmpeg(args):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-bitexact'] + args, check=True)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    src, bgm, out = sys.argv[1], sys.argv[2], sys.argv[3]
    no_music = '--no-music' in sys.argv
    man = json.load(open(os.path.join(HERE, 'slice_manifest.json')))
    os.makedirs(out, exist_ok=True)
    report, manifest = [], {'banks': {}, 'textures': {}, 'sfx': {}, 'music': {}, 'levels': {}}

    for bank in man['banks']:
        manifest['banks'][bank['name']] = write_bank(src, bank, out, report)

    for name, idx in man['textures'].items():
        im, _, _ = load_img(src, idx)
        w, h = write_tex(im, os.path.join(out, name + '.tex'))
        manifest['textures'][name] = {'src': idx, 'w': im.width, 'h': im.height, 'tex': [w, h]}
        report.append('tex  %-10s img_%d %dx%d -> %dx%d' % (name, idx, im.width, im.height, w, h))

    for name, idx in man['sfx'].items():
        dst = os.path.join(out, 'sfx_%s.wav' % name)
        ffmpeg(['-i', os.path.join(src, 'audio', 'audio_%d.ogg' % idx), '-ac', '1', '-ar', '22050',
                '-c:a', 'pcm_s16le', '-map_metadata', '-1', '-fflags', '+bitexact', dst])
        manifest['sfx'][name] = {'src': idx, 'bytes': os.path.getsize(dst)}
        report.append('sfx  %-10s audio_%d -> %d bytes' % (name, idx, os.path.getsize(dst)))

    if not no_music:
        for name, fn in man['music'].items():
            dst = os.path.join(out, name + '.pcm')
            ffmpeg(['-i', os.path.join(bgm, fn), '-ac', '2', '-ar', '22050', '-f', 's16le', dst])
            manifest['music'][name] = {'src': fn, 'bytes': os.path.getsize(dst)}
            report.append('mus  %-10s %s -> %d bytes' % (name, fn, os.path.getsize(dst)))

    for name, fn in man['levels'].items():
        w, h, n = convert_level(os.path.join(HERE, '..', 'data', fn), os.path.join(out, name + '.lvl'))
        manifest['levels'][name] = {'w': w, 'h': h, 'objects': n}
        report.append('lvl  %-10s %dx%d tiles, %d objects' % (name, w, h, n))

    # validation: sizes / VRAM estimate
    vram = 0
    for fn in sorted(os.listdir(out)):
        if fn.endswith('.tex'):
            w, h = struct.unpack_from('<2H', open(os.path.join(out, fn), 'rb').read(8), 4)
            vram += w * h * 2
    manifest['vram_bytes'] = vram
    report.append('VRAM textures total: %d bytes (%.2f MB of 8 MB)' % (vram, vram / 1048576))
    if vram > 6 * 1048576:
        report.append('ERROR: texture budget exceeded')
        sys.exit(2)
    for fn in sorted(os.listdir(out)):
        if fn != 'manifest.json':
            manifest.setdefault('sha256', {})[fn] = hashlib.sha256(open(os.path.join(out, fn), 'rb').read()).hexdigest()
    json.dump(manifest, open(os.path.join(out, 'manifest.json'), 'w'), indent=1, sort_keys=True)
    print('\n'.join(report))


if __name__ == '__main__':
    main()
