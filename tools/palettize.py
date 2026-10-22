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
    groups = [re.compile(r) for r in sys.argv[2:]]
    if not groups or len(groups) > 4:
        raise SystemExit('need 1..4 group regexes')
    files = sorted(f for f in os.listdir(d) if f.endswith('.tex'))
    for g, rx in enumerate(groups):
        members = [f for f in files if rx.search(f)]
        texs = {}
        cols = set()
        for f in members:
            raw = open(os.path.join(d, f), 'rb').read()
            w, h, fmt, sw, sh, _ = struct.unpack_from('<6H', raw, 4)
            if fmt != 0:
                raise SystemExit('%s: not ARGB1555 (already converted?)' % f)
            px = struct.unpack_from('<%dH' % (w * h), raw, 16)
            texs[f] = (w, h, sw, sh, px)
            cols |= {p for p in px if p & 0x8000}
        if len(cols) > 255:
            raise SystemExit('group %d (%s): %d colours > 255' % (g, rx.pattern, len(cols)))
        pal = [0] + sorted(cols)
        index = {c: i for i, c in enumerate(pal)}
        for f, (w, h, sw, sh, px) in texs.items():
            idx = bytes(index[p] if p & 0x8000 else 0 for p in px)
            open(os.path.join(d, f), 'wb').write(b'FPTX' + struct.pack('<6H', w, h, 1, sw, sh, g) + idx)
        pal += [0] * (256 - len(pal))
        open(os.path.join(d, 'pal%d.pal' % g), 'wb').write(b'FPPL' + struct.pack('<2H', g, len(cols) + 1) + struct.pack('<256H', *pal))
        print('group %d %-22s %2d textures, %3d colours' % (g, rx.pattern, len(members), len(cols)))

if __name__ == '__main__':
    main()
