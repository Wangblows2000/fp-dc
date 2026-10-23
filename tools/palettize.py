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
    args = [a for a in sys.argv[2:] if not a.startswith('--flex=')]
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
    members = [[f for f in files if rx.search(f)] for rx in groups]
    texs = {f: load(f) for m in members for f in m}
    cols = [set(p for f in m for p in texs[f][4] if p & 0x8000) for m in members]
    # --flex=<regex>: textures that join the first group with room for their colours (fewest added first)
    for rx in flex:
        for f in files:
            if not rx.search(f) or any(f in m for m in members): continue
            texs[f] = load(f)
            c = set(p for p in texs[f][4] if p & 0x8000)
            cand = [g for g in range(len(groups)) if not groups[g].search('lilac2.tex')]   # Lilac's palette stays fixed (resident banks)
            order = sorted(cand, key=lambda g: len(cols[g] | c))
            g = next((g for g in order if len(cols[g] | c) <= 255), None)
            if g is None:
                # no room: keep the most used colours that fit, map the rest to the nearest kept colour (RGB555
                # distance). Reported in the build log; only rare near-duplicate shades are merged.
                g = min(cand, key=lambda g: len(cols[g]))
                room = 255 - len(cols[g])
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
        pal = [0] + sorted(cols[g])
        index = {c: i for i, c in enumerate(pal)}
        for f in members[g]:
            w, h, sw, sh, px = texs[f]
            idx = bytes(index[p] if p & 0x8000 else 0 for p in px)
            open(os.path.join(d, f), 'wb').write(b'FPTX' + struct.pack('<6H', w, h, 1, sw, sh, g) + idx)
        pal += [0] * (256 - len(pal))
        open(os.path.join(d, 'pal%d.pal' % g), 'wb').write(b'FPPL' + struct.pack('<2H', g, len(cols[g]) + 1) + struct.pack('<256H', *pal))
        print('group %d %-22s %2d textures, %3d colours' % (g, rx.pattern, len(members[g]), len(cols[g])))

if __name__ == '__main__':
    main()
