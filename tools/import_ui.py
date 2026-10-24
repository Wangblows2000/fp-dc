#!/usr/bin/env python3
"""
Import original front-end frames (menus, title, character/stage select) as UI scenes.

usage: import_ui.py <frames_dir> <images_dir> <out_dir> <name> <frame_no> [<frame_no> ...]

Every object of the listed frames (Frames::on_frame_N_init placements recovered by dump_frames.py)
is exported with ALL of its animations and directions, so the menu code can drive the original
objects by class name (show/hide, highlighted direction, value frames) instead of redrawing them.

Output (game data, gitignored):
  <name>_ui<k>.spr / .tex   sprite banks (FPS2, ARGB1555 atlases <= 1024x1024; UI uses no palette)
  <name>_<frame>.ui         scene: 'FPU1' u16 w, h, nobj, nbanks; per object:
                              char cls[24]; s16 x, y; u8 layer, kind (0 backdrop, 1 active), nent, pad;
                              nent x { u8 anim, u8 dir, u8 bank, u8 pad; u16 bank_anim }
                            kind 2 (a solid-colour quick backdrop, no image): nent 0, then u16 w, h; u32 argb1, argb2
                              (vertical gradient end colour, = argb1 when flat)
"""
import json, os, struct, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert import write_tex, pack
import title_credit

MAX_ENT = 40
ACT = {}

def load_img(src, i):
    im = Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')
    if i == title_credit.IMAGE_ID: im = title_credit.patch(im)      # port credit (intentional change)
    m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()]
    hx, hy = [v - 65536 if v > 32767 else v for v in m[:2]]
    ax, ay = ([v - 65536 if v > 32767 else v for v in m[2:4]] if len(m) >= 4 else (hx, hy))
    ACT[i] = (ax, ay)                 # Fusion action point (relative to the image's top-left)
    return im, hx, hy

def main():
    frames_dir, src, out, name = sys.argv[1:5]
    # "<frame>+<class_id>" adds a runtime-created class (not placed in the frame) to that frame's scene,
    # off-screen at (-1000, -1000), e.g. 3+titlestar_53 (event 307 creates 250 of them)
    # "-x REGEX" leaves out classes whose name matches (e.g. stage previews of stages the port lacks)
    import re
    argv = sys.argv[5:]; excl = []
    while '-x' in argv:
        i = argv.index('-x'); excl.append(re.compile(argv[i + 1])); del argv[i:i + 2]
    sys.argv[5:] = argv
    frame_nos = [int(v) for v in sys.argv[5:] if '+' not in v]
    extra = {}
    for v in sys.argv[5:]:
        if '+' in v:
            f, k = v.split('+', 1); extra.setdefault(int(f), []).append(k)
    an = json.load(open(os.path.join(frames_dir, 'anims.json')))
    bd = json.load(open(os.path.join(frames_dir, 'backdrops.json')))
    def solid(key):   # Fusion quick backdrop without an image: size 0x4c / 0x50, colour bytes 0xc4-0xc7 (R G B A), gradient 0xc8 = 1 -> 0xcc-0xcf
        e = bd.get(key)
        if not e or e.get('image') is not None or '0xc4' not in e.get('fields', {}): return None
        f = e['fields']; c1 = (255 << 24) | (f.get('0xc4', 0) << 16) | (f.get('0xc5', 0) << 8) | f.get('0xc6', 0)
        c2 = (255 << 24) | (f.get('0xcc', 0) << 16) | (f.get('0xcd', 0) << 8) | f.get('0xce', 0) if f.get('0xc8') == 1 else c1
        return (f.get('0x4c', 0), f.get('0x50', 0), c1, c2)
    os.makedirs(out, exist_ok=True)
    scenes, groups = [], []          # groups: (key, frames, speed, loop, back_to)
    gindex = {}
    for fno in frame_nos:
        fr = json.load(open(os.path.join(frames_dir, 'frame_%d.json' % fno)))
        objs = []
        for k in extra.get(fno, []):
            c, i = k.rsplit('_', 1)
            fr['objects'].append({'cls': c, 'id': int(i), 'x': -1000, 'y': -1000, 'layer': 0, 'kind': 'obj'})
        fr['objects'] = [o for o in fr['objects'] if not any(r.search(o['cls']) for r in excl)]
        for o in fr['objects']:
            key = '%s_%d' % (o['cls'], o['id'])
            e = an.get(key, {})
            ents = []
            if e.get('anims'):
                for ai, ds in enumerate(e['anims']):
                    if not ds: continue
                    for dk, d in sorted(ds.items(), key=lambda kv: int(kv[0])):
                        fr_ids = [i for i in d['frames'] if i is not None]
                        if fr_ids: ents.append((ai, int(dk), fr_ids, d['max_speed'], d['loop'], d['back_to']))
            elif e.get('images'):
                ents.append((0, 0, [i for i in e['images'] if i is not None], 0, -1, 0))
            ents = ents[:MAX_ENT]
            if not ents:
                sb = solid(key) if o.get('kind') == 'bg' else None
                if sb: objs.append((o['cls'], o['x'] if o['x'] is not None else -1000, o['y'] if o['y'] is not None else -1000, o['layer'], 2, sb))
                continue
            refs = []
            for ai, dk, ids, sp, lp, bt in ents:
                gk = (tuple(ids), sp, lp, bt)
                if gk not in gindex:
                    gindex[gk] = len(groups); groups.append(gk)
                refs.append((ai, dk, gindex[gk]))
            objs.append((o['cls'], o['x'] if o['x'] is not None else -1000, o['y'] if o['y'] is not None else -1000, o['layer'], 0 if o.get('kind') == 'bg' else 1, refs))
        scenes.append((fno, fr['width'], fr['height'], objs))
    # assign groups to atlases (whole sequences per atlas)
    imgs = {}
    for g in groups:
        for i in g[0]:
            if i not in imgs: imgs[i] = load_img(src, i)
    banks, cur, cur_ids = [], [], []
    def fits(ids):
        return pack([(i, imgs[i][0]) for i in ids], 1024) is not None
    dropped = set()
    for gi, g in enumerate(groups):
        if not fits(list(dict.fromkeys(g[0]))):
            print('WARNING: group %d (%d images, e.g. img %d) exceeds one 1024x1024 atlas: dropped' % (gi, len(g[0]), g[0][0]))
            dropped.add(gi); continue
        trial = cur_ids + [i for i in g[0] if i not in cur_ids]
        if cur and not fits(trial):
            banks.append((cur, cur_ids)); cur, cur_ids = [], []
            trial = list(dict.fromkeys(g[0]))
        if not fits(trial): raise SystemExit('group %d does not fit one atlas' % gi)
        cur.append(gi); cur_ids = trial
    if cur: banks.append((cur, cur_ids))
    gloc = {}
    for bi, (gl, ids) in enumerate(banks):
        pos = pack([(i, imgs[i][0]) for i in ids], 1024)
        size = 1024
        while size > 64 and pack([(i, imgs[i][0]) for i in ids], size // 2): size //= 2
        pos = pack([(i, imgs[i][0]) for i in ids], size)
        atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        for i in ids: atlas.paste(imgs[i][0], pos[i])
        bb = atlas.getbbox() or (0, 0, 8, 8)
        atlas = atlas.crop((0, 0, size, max(8, bb[3])))
        bn = '%s_ui%d' % (name, bi)
        write_tex(atlas, os.path.join(out, bn + '.tex'))
        data = bytearray(b'FPS2' + struct.pack('<4H', len(ids), len(gl), size, size))
        for i in ids:
            im, hx, hy = imgs[i]
            data += struct.pack('<4H2h', pos[i][0], pos[i][1], im.width, im.height, hx, hy)
        seq, first = [], 0
        for k, gi in enumerate(gl):
            fr_ids, sp, lp, bt = groups[gi]
            data += ('g%d' % gi).encode()[:15].ljust(16, b'\0') + struct.pack('<2H2Bh', first, len(fr_ids), min(255, sp), min(255, bt), lp)
            seq += [ids.index(i) for i in fr_ids]; first += len(fr_ids)
            gloc[gi] = (bi, k)
        data += struct.pack('<%dH' % len(seq), *seq)
        # 'ACT1' + per frame s16 ax, ay: action points (menus place the cursor on a label's action point)
        data += b'ACT1' + b''.join(struct.pack('<2h', *ACT.get(i, (0, 0))) for i in ids)
        open(os.path.join(out, bn + '.spr'), 'wb').write(data)
        print('bank %s: %d images, %d anims, atlas %d' % (bn, len(ids), len(gl), size))
    for fno, w, h, objs in scenes:
        d = bytearray(b'FPU1' + struct.pack('<4H', w, h, len(objs), len(banks)))
        for cls, x, y, layer, kind, refs in objs:
            if kind == 2:
                d += cls.encode()[:23].ljust(24, b'\0') + struct.pack('<2h4B', x, y, layer, 2, 0, 0) + struct.pack('<2H2I', *refs)
                continue
            refs = [r for r in refs if r[2] not in dropped]
            d += cls.encode()[:23].ljust(24, b'\0') + struct.pack('<2h4B', x, y, layer, kind, len(refs), 0)
            for ai, dk, gi in refs:
                bi, k = gloc[gi]
                d += struct.pack('<4BH', ai, dk, bi, 0, k)
        open(os.path.join(out, '%s_%d.ui' % (name, fno)), 'wb').write(d)
        print('scene %s_%d: %dx%d, %d objects' % (name, fno, w, h, len(objs)))

if __name__ == '__main__':
    main()
