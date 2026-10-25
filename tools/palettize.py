#!/usr/bin/env python3
"""
Convert ARGB1555 .tex files to 8-bit palettized textures sharing one palette per group.
The PowerVR has 1024 palette entries = 4 banks of 256 for PAL8 textures.

usage: palettize.py <dir> <group0_regex> [<group1_regex> ...]   (max 4 groups)

Each matching .tex is rewritten in place as:
  'FPTX' u16 w, h, fmt=1 (PAL8, linear; the runtime twiddles at upload), srcw, srch, group
  then w*h u8 indices (0 = transparent)
and pal<group>.pal is written: 'FPPL' u16 group, u16 count, then 256 u16 ARGB1555.
Exits with an error if a group needs more than 255 opaque colours (no lossy fallback).
"""
import os, re, struct, sys

def main():
    d = sys.argv[1]
    args = [a for a in sys.argv[2:] if not a.startswith(('--flex=', '--pal4=', '--gid='))]
    # --gid=N: number the (single) group N instead of 0 (a palette that replaces bank N at run time, e.g. Carol's
    # textures use Lilac's bank 2 when she plays; written as pal<N>.pal in <dir>)
    gid = next((int(a[6:]) for a in sys.argv[2:] if a.startswith('--gid=')), None)
    # --pal4=<group>:<regex>: 4 bpp textures (<= 15 colours per image key); one 16-entry block per key (images whose
    # colours fit together share a block) at the start of palette group <group>; FPTX fmt 3, header field = bank 0..63
    pal4 = [a[7:].split(':', 1) for a in sys.argv[2:] if a.startswith('--pal4=')]
    flex = [re.compile(a[7:]) for a in sys.argv[2:] if a.startswith('--flex=')]
    groups = [re.compile(r) for r in args]
    if not groups or len(groups) > 4:
        raise SystemExit('need 1..4 group regexes')
    files = sorted(f for f in os.listdir(d) if f.endswith('.tex'))
    def load(f):
        raw = open(os.path.join(d, f), 'rb').read()
        w, h, fmt, sw, sh, _ = struct.unpack_from('<6H', raw, 4)
        if fmt != 0:
            raise SystemExit('%s: not ARGB1555 (already converted?)' % f)
        return (w, h, sw, sh, struct.unpack_from('<%dH' % (w * h), raw, 16))
    # several --pal4 options may name different groups; each group gets its own 16-colour blocks (session 11: the
    # 4-colour hair / tail / shield overlays become 4 bpp in the character's group 2)
    p4files, p4grp, blocks, p4key = [], {}, {}, {}
    keyof = lambda f: re.sub(r'_\d+\.tex$', '', f)
    for g4, rx4 in pal4:
        g4 = int(g4); rx4 = re.compile(rx4); blocks.setdefault(g4, [])
        mine = [f for f in files if rx4.search(f) and f not in p4files]
        kcols = {}
        for f in mine:
            t = load(f); kcols.setdefault(keyof(f), set()).update(q for q in t[4] if q & 0x8000); p4grp[f] = g4
        for k in sorted(kcols, key=lambda k: -len(kcols[k])):
            if len(kcols[k]) > 15: raise SystemExit('%s: %d colours > 15 (pal4)' % (k, len(kcols[k])))
            bl = blocks[g4]
            b = next((i for i, bc in enumerate(bl) if len(bc | kcols[k]) <= 15), None)
            if b is None: bl.append(set()); b = len(bl) - 1
            bl[b] |= kcols[k]; p4key[k] = b
        p4files += mine
    files = [f for f in files if f not in p4files]
    nblk = lambda g: 16 * len(blocks.get(g, []))
    members = [[f for f in files if rx.search(f)] for rx in groups]
    texs = {f: load(f) for m in members for f in m}
    cols = [set(p for f in m for p in texs[f][4] if p & 0x8000) for m in members]
    # spill (session 14, generic): a group whose textures need more than its room (e.g. rm3's level pieces: 282 colours)
    # moves whole textures - its later atlases first - to another group with room for them, losslessly, before the
    # flex banks are placed. Lilac's group stays fixed (resident banks). Levels whose groups fit are unchanged.
    for g in range(len(groups)):
        while len(cols[g]) > 255 - nblk(g) and len(members[g]) > 1:
            moved = False
            for f in sorted(members[g], reverse=True):
                c = set(p for p in texs[f][4] if p & 0x8000)
                rest = set(p for f2 in members[g] if f2 != f for p in texs[f2][4] if p & 0x8000)
                cand = [h for h in range(len(groups)) if h != g and not groups[h].search('lilac2.tex')]
                h = min(cand, key=lambda h: len(cols[h] | c), default=None)
                if h is None or len(cols[h] | c) > 255 - nblk(h): continue
                members[g].remove(f); members[h].append(f); cols[g] = rest; cols[h] |= c
                print('%s: spilled from group %d to group %d' % (f, g, h)); moved = True; break
            if not moved: break
    # --flex=<regex>: textures that join the first group with room for their colours (fewest added first)
    for rx in flex:
        for f in files:
            if not rx.search(f) or any(f in m for m in members): continue
            texs[f] = load(f)
            c = set(p for p in texs[f][4] if p & 0x8000)
            cand = [g for g in range(len(groups)) if not groups[g].search('lilac2.tex')]   # Lilac's palette stays fixed (resident banks)
            order = sorted(cand, key=lambda g: len(cols[g] | c))
            g = next((g for g in order if len(cols[g] | c) <= 255 - nblk(g)), None)
            if g is None:
                # no room: keep the most used colours that fit, map the rest to the nearest kept colour (RGB555
                # distance). Reported in the build log; only rare near-duplicate shades are merged.
                g = min(cand, key=lambda g: len(cols[g]) + nblk(g))
                room = 255 - len(cols[g]) - nblk(g)
                hist = {}
                for q in texs[f][4]:
                    if q & 0x8000 and q not in cols[g]: hist[q] = hist.get(q, 0) + 1
                keep = set(sorted(hist, key=lambda q: -hist[q])[:room])
                pool = list(cols[g] | keep)
                def rgb(q): return ((q >> 10) & 31, (q >> 5) & 31, q & 31)
                remap = {}
                for q in hist:
                    if q in keep: continue
                    a = rgb(q)
                    remap[q] = min(pool, key=lambda r: sum((x - y) ** 2 for x, y in zip(a, rgb(r))))
                w, h, sw, sh, px = texs[f]
                texs[f] = (w, h, sw, sh, tuple(remap.get(q, q) for q in px))
                print('%s: %d colours merged into nearest of group %d (%d kept)' % (f, len(remap), g, len(keep)))
                c = set(q for q in texs[f][4] if q & 0x8000)
            members[g].append(f); cols[g] |= c
    for g, rx in enumerate(groups):
        if len(cols[g]) > 255:
            raise SystemExit('group %d (%s): %d colours > 255' % (g, rx.pattern, len(cols[g])))
        pre = []
        if blocks.get(g):
            bl = blocks[g]
            for b, bc in enumerate(bl):
                ent = [0] + sorted(bc); ent += [0] * (16 - len(ent)); pre += ent
            bidx = [{c: i for i, c in enumerate(pre[16 * b:16 * b + 16]) if i} for b in range(len(bl))]
            mine = [f for f in p4files if p4grp[f] == g]
            for f in mine:
                w, h, sw, sh, px = load(f)
                b = p4key[keyof(f)]; m = bidx[b]
                nib = [m.get(q, 0) if q & 0x8000 else 0 for q in px]
                data = bytes(nib[i] | (nib[i + 1] << 4) for i in range(0, len(nib), 2))
                open(os.path.join(d, f), 'wb').write(b'FPTX' + struct.pack('<6H', w, h, 3, sw, sh, (g if gid is None else gid) * 16 + b) + data)
            print('pal4 group %d: %d textures, %d blocks' % (g, len(mine), len(bl)))
        pal = (pre if pre else [0]) + sorted(cols[g])
        if len(pal) > 256: raise SystemExit('group %d: %d entries > 256 with the pal4 blocks' % (g, len(pal)))
        index = {c: i for i, c in enumerate(pal) if i >= len(pre) or not pre}
        for f in members[g]:
            w, h, sw, sh, px = texs[f]
            idx = bytes(index[p] if p & 0x8000 else 0 for p in px)
            open(os.path.join(d, f), 'wb').write(b'FPTX' + struct.pack('<6H', w, h, 1, sw, sh, g if gid is None else gid) + idx)
        pal += [0] * (256 - len(pal))
        gg = g if gid is None else gid; open(os.path.join(d, 'pal%d.pal' % gg), 'wb').write(b'FPPL' + struct.pack('<2H', gg, max(len(cols[g]) + 1, len(pre) + len(cols[g]) + 1 if pre else 0)) + struct.pack('<256H', *pal))
        print('group %d %-22s %2d textures, %3d colours' % (g, rx.pattern, len(members[g]), len(cols[g])))

if __name__ == '__main__':
    main()
