#!/usr/bin/env python3
"""
Import a recovered Freedom Planet frame (level) into fpdc runtime data.

usage: import_frame.py <frames_dir> <images_dir> <frame_no> <name> <out_dir> [--layers 2]

Inputs (from tools/re/*.py, run on the user's Linux build):
  frames_dir/frame_N.json   object placements (class, x, y, layer, kind)
  frames_dir/backdrops.json backdrop classes: image id + collision field 0xac
  frames_dir/anims.json     active classes: animation tables (image ids, speeds)
  images_dir                fp-assets extraction of the Linux Assets.dat

Collision typing (RESEARCH_LOG "backdrop collision", CONFIRMED via collider vtables):
  0xac=9  + InstanceBox       -> solid, whole image rectangle
  0xac=8  + BackdropCollision -> solid, pixel mask (alpha >= 128)
  0xac=11 + InstanceBox       -> ladder (not solid)
  absent                      -> decoration
Pieces with blend colour alpha 0 (field 0x5f == 0) are invisible (collision only).
Collision is rasterised on the collision layers (default: 2, the main play layer).

Outputs:
  <name>_pN.tex   piece atlases (ARGB1555, <=1024x1024)
  <name>.lv2      level (see write_lv2)
  <name>_obj.tex/.spr  sprite bank of the active objects the engine implements
  <name>_report.txt
"""
import json, os, struct, sys, hashlib
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert import write_tex, pack, argb1555

# active classes the runtime understands -> engine type id
ENGINE_TYPES = {
    'playerstartingpoint': 1, 'objectcrystal': 2, 'enemydragonfly': 3, 'objectpetal': 6,
    'hazardspikesup': 7, 'yellowspringup': 8, 'sign': 9, 'enemyturretus': 10,
    'breakablerock': 11, 'monitorcrystals': 12, 'monitorrings': 12, 'monitor1up': 12,
    'monitorinvincibility': 12, 'levelparallaxpiece0': 20, 'levelparallaxpiece1': 21,
    'levelparallaxpiece2': 22, 'levelparallaxpiece3': 23, 'levelparallaxpiece4': 24,
    'levelparallaxpiece5': 25, 'levelparallaxpiece6': 26, 'levelparallaxpiece7': 27,
}

def load_img(src, i):
    im = Image.open(os.path.join(src, 'img_%d.png' % i)).convert('RGBA')
    m = [int(v, 0) for v in open(os.path.join(src, 'img_%d_meta.txt' % i)).read().split()]
    hx, hy = [v - 65536 if v > 32767 else v for v in m[:2]]
    return im, hx, hy

def first_anim_frames(entry):
    """frames of the first non-empty animation, right-facing direction"""
    for ds in entry.get('anims') or []:
        if ds:
            d = ds.get('0') or ds[sorted(ds, key=int)[0]]
            return d
    return None

def main():
    fd, src, n, name, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
    col_layers = {2}
    if '--layers' in sys.argv:
        col_layers = {int(v) for v in sys.argv[sys.argv.index('--layers') + 1].split(',')}
    os.makedirs(out, exist_ok=True)
    fr = json.load(open(os.path.join(fd, 'frame_%d.json' % n)))
    bd = json.load(open(os.path.join(fd, 'backdrops.json')))
    an = json.load(open(os.path.join(fd, 'anims.json')))
    W, H = fr['width'], fr['height']
    report = ['frame %d -> %s  (%dx%d px, %d placements)' % (n, name, W, H, len(fr['objects']))]

    pieces, objs, skipped = [], [], {}
    for o in fr['objects']:
        key = '%s_%d' % (o['cls'], o['id'])
        if o['x'] is None or o['y'] is None:
            skipped['unresolved'] = skipped.get('unresolved', 0) + 1; continue
        if key in bd:
            b = bd[key]
            if b['image'] is None:
                skipped['no-image'] = skipped.get('no-image', 0) + 1; continue
            ac = b['fields'].get('0xac')
            col = 0 if ac is None else {9: 1, 8: 1, 11: 3}.get(ac, 0)
            box = ac == 9
            invisible = b['fields'].get('0x5f') == 0 and '0x5c' in b['fields']
            pieces.append({'img': b['image'], 'x': o['x'], 'y': o['y'], 'layer': o['layer'] or 0, 'col': col,
                           'box': box, 'invisible': invisible, 'cls': o['cls']})
        else:
            t = ENGINE_TYPES.get(o['cls'])
            if t is None:
                skipped[o['cls']] = skipped.get(o['cls'], 0) + 1; continue
            objs.append({'type': t, 'x': o['x'], 'y': o['y'], 'cls': key})

    # ---- piece atlases
    ids = sorted({p['img'] for p in pieces})
    imgs = {i: load_img(src, i) for i in ids}
    remaining = list(ids); atlases = []; where = {}
    while remaining:
        pos = None; take = list(remaining)
        while take:
            pos = pack([(i, imgs[i][0]) for i in take], 1024)
            if pos: break
            take = take[:len(take) * 3 // 4]
        if not take: raise SystemExit('piece too large for 1024 atlas')
        k = len(atlases)
        atlas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
        for i in take:
            atlas.paste(imgs[i][0], pos[i]); where[i] = (k, pos[i][0], pos[i][1])
        bbox = atlas.getbbox() or (0, 0, 8, 8)
        atlas = atlas.crop((0, 0, max(bbox[2], 8), max(bbox[3], 8)))
        tw, th = write_tex(atlas, os.path.join(out, '%s_p%d.tex' % (name, k)))
        atlases.append((tw, th))
        remaining = [i for i in remaining if i not in take]
    report.append('pieces: %d placements, %d unique images, %d atlases %s' % (len(pieces), len(ids), len(atlases), atlases))

    # ---- collision rasterisation (alpha >= 128) on collision layers
    TW, TH = (W + 15) // 16, (H + 15) // 16
    solid = bytearray(TW * 16 * TH * 16)
    plat = bytearray(TW * 16 * TH * 16)
    stride = TW * 16
    masks = {}
    for p in pieces:
        if p['col'] not in (1, 2) or p['layer'] not in col_layers: continue
        im, hx, hy = imgs[p['img']]
        mk = (p['img'], p['box'])
        if mk not in masks:
            masks[mk] = b'\x01' * (im.width * im.height) if p['box'] else im.getchannel('A').point(lambda v: 1 if v >= 128 else 0).tobytes()
        m = masks[mk]; x0 = p['x'] - hx; y0 = p['y'] - hy
        tgt = solid if p['col'] == 1 else plat
        for yy in range(im.height):
            Y = y0 + yy
            if Y < 0 or Y >= TH * 16: continue
            row = m[yy * im.width:(yy + 1) * im.width]
            base = Y * stride
            for xx, v in enumerate(row):
                if v:
                    X = x0 + xx
                    if 0 <= X < stride: tgt[base + X] = 1
    # dedupe 16x16 tiles into 32-byte bit patterns
    uniq = {bytes(32): 0, b'\xff' * 32: 1}
    table = [bytes(32), b'\xff' * 32]
    def tilemap(buf):
        out_map = []
        for ty in range(TH):
            for tx in range(TW):
                bits = bytearray(32)
                for yy in range(16):
                    base = (ty * 16 + yy) * stride + tx * 16
                    v = 0
                    for xx in range(16):
                        if buf[base + xx]: v |= 1 << xx
                    bits[yy * 2] = v & 255; bits[yy * 2 + 1] = v >> 8
                b = bytes(bits)
                if b not in uniq:
                    uniq[b] = len(table); table.append(b)
                out_map.append(uniq[b])
        return out_map
    smap = tilemap(solid); pmap = tilemap(plat)
    report.append('collision: %dx%d tiles, %d unique 16x16 patterns (%d bytes), layers %s' % (TW, TH, len(table), len(table) * 32, sorted(col_layers)))

    # ---- active-object sprite bank (first anim, right-facing) for implemented classes
    bank_classes = sorted({o['cls'] for o in objs if o['type'] < 20 and o['type'] != 1})
    bank = {'name': name + '_obj', 'anims': []}
    for key in bank_classes:
        d = first_anim_frames(an.get(key, {}))
        if not d or not d['frames'] or None in d['frames']:
            report.append('  no animation for %s' % key); continue
        bank['anims'].append({'name': key[:15], 'frames': d['frames'], 'speed': max(1, d['max_speed']),
                              'loop': d['loop'], 'back_to': d['back_to']})
    for extra, nm in (('effectsexplosion01_367', 'explosion'),):
        d = first_anim_frames(an.get(extra, {}))
        if d: bank['anims'].append({'name': nm, 'frames': d['frames'], 'speed': d['max_speed'], 'loop': d['loop'], 'back_to': d['back_to']})
    write_bank2(src, bank, out, report)
    # parallax pieces (first frame image as a texture each)
    par = []
    for o in objs:
        if o['type'] >= 20:
            e = an.get(o['cls'], {})
            d = first_anim_frames(e)
            img = d['frames'][0] if d and d['frames'] and d['frames'][0] is not None else (e.get('images') or [None])[0]
            if img is not None:
                im, hx, hy = load_img(src, img)
                tn = '%s_par%d' % (name, len(par))
                write_tex(im, os.path.join(out, tn + '.tex'))
                par.append((o['type'] - 20, o['x'] - hx, o['y'] - hy, im.width, im.height))
                o['par_index'] = len(par) - 1
    report.append('parallax pieces: %s' % par)

    # ---- Lilac (playerskinsash_341) from recovered animation tables, both facings
    LILAC = [('idle', 0), ('walk', 1), ('run', 2), ('lookup', 5), ('spin', 7), ('charge', 8), ('crouch', 10),
             ('boost', 13), ('hurt', 16), ('cyclone', 22), ('skid', 24), ('airatk', 25), ('attack', 26),
             ('uppercut', 28), ('divekick', 30), ('defeat', 38)]
    la = an['playerskinsash_341']['anims']
    lb = {'name': 'lilac2', 'anims': []}
    for nm, ai in LILAC:
        for dkey, suf in (('0', ''), ('16', '_l')):
            dd = la[ai].get(dkey) or la[ai]['0']
            lb['anims'].append({'name': nm + suf, 'frames': dd['frames'], 'speed': dd['max_speed'],
                                'loop': dd['loop'], 'back_to': dd['back_to']})
    write_bank2(src, lb, out, report)
    write_lv2(os.path.join(out, name + '.lv2'), W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, len(atlases), bank)
    report.append('objects exported: %d (types %s)' % (len(objs), sorted({o['type'] for o in objs})))
    report.append('skipped classes (not implemented / helpers): %s' % sorted(skipped.items(), key=lambda kv: -kv[1])[:40])
    open(os.path.join(out, name + '_report.txt'), 'w').write('\n'.join(report) + '\n')
    print('\n'.join(report))

def write_bank2(src, bank, out, report):
    """'FPS2' u16 nframes, nanims, texw, texh; frames {u16 u,v,w,h; s16 hx,hy};
       anims {char name[16]; u16 first, count; u8 speed(Fusion 0-100), u8 back_to; s16 loop}; u16 seq[]"""
    ids = []
    for a in bank['anims']:
        for i in a['frames']:
            if i not in ids: ids.append(i)
    imgs = {i: load_img(src, i) for i in ids}
    size = 64
    while True:
        pos = pack([(i, imgs[i][0]) for i in ids], size)
        if pos: break
        size *= 2
        if size > 1024: raise SystemExit('bank %s too large' % bank['name'])
    atlas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    # (atlas height is cropped to content by write_tex's pow2 rounding below)
    for i in ids: atlas.paste(imgs[i][0], pos[i])
    bb = atlas.getbbox() or (0, 0, 8, 8)
    atlas = atlas.crop((0, 0, size, max(8, bb[3])))
    write_tex(atlas, os.path.join(out, bank['name'] + '.tex'))
    data = bytearray(b'FPS2' + struct.pack('<4H', len(ids), len(bank['anims']), size, size))
    for i in ids:
        im, hx, hy = imgs[i]
        data += struct.pack('<4H2h', pos[i][0], pos[i][1], im.width, im.height, hx, hy)
    seq, first = [], 0
    for a in bank['anims']:
        data += a['name'].encode()[:15].ljust(16, b'\0') + struct.pack('<2H2Bh', first, len(a['frames']), min(255, a['speed']), min(255, a['back_to']), a['loop'])
        seq += [ids.index(i) for i in a['frames']]; first += len(a['frames'])
    data += struct.pack('<%dH' % len(seq), *seq)
    open(os.path.join(out, bank['name'] + '.spr'), 'wb').write(data)
    report.append('bank %s: %d frames, %d anims, atlas %d' % (bank['name'], len(ids), len(bank['anims']), size))

def write_lv2(path, W, H, pieces, where, imgs, objs, TW, TH, table, smap, pmap, natlas, bank):
    """'FPL2' u32 w, h (px); u16 npieces, nobj, natlas, nuniq; u16 tw, th; u32 pad
       piece {s32 x, y (top-left); u16 u, v, w, h; u8 atlas, layer, col, pad}  (20 B)
       obj   {u16 type, anim (index in <name>_obj bank or 0xffff); s32 x, y}  (12 B)
       uniq tiles: 32 B each (16 rows of u16, bit x = pixel x)
       solid map u16[tw*th]; platform map u16[tw*th]"""
    pieces = sorted(pieces, key=lambda p: (p['layer'], p['x']))
    names = [a['name'] for a in bank['anims']]
    d = bytearray(b'FPL2' + struct.pack('<2I4H2HI', W, H, len(pieces), len(objs), natlas, len(table), TW, TH, 0))
    for p in pieces:
        im, hx, hy = imgs[p['img']]
        k, u, v = where[p['img']]
        d += struct.pack('<2i4H4B', p['x'] - hx, p['y'] - hy, u, v, im.width, im.height, k, p['layer'], p['col'], 1 if p['invisible'] else 0)
    for o in objs:
        a = names.index(o['cls'][:15]) if o['cls'][:15] in names else o.get('par_index', 0xffff)
        d += struct.pack('<2H2i', o['type'], a, o['x'], o['y'])
    for t in table: d += t
    d += struct.pack('<%dH' % len(smap), *smap)
    d += struct.pack('<%dH' % len(pmap), *pmap)
    open(path, 'wb').write(d)

if __name__ == '__main__':
    main()
