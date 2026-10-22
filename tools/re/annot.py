#!/usr/bin/env python3
"""Annotate Ghidra output of the ORIGINAL executable (RE material, build/re only).
- _UNK_<addr> / DAT_<addr> globals -> their double value read from the ELF (e.g. _UNK_021b5a38{=0.5})
- "(lVar + 0x58)" style alterable offsets -> alterable letter: values A..Z are doubles at alt+0x50+8k
usage: annot.py <binary> <file.c> [...]   (rewrites <file>.a.c next to each input)"""
import re, struct, sys

def sections(path):
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('<Q', d, 0x28); shentsize, shnum = struct.unpack_from('<HH', d, 0x3a)
    out = []
    for i in range(shnum):
        o = shoff + i * shentsize
        typ, = struct.unpack_from('<I', d, o + 4)
        addr, off, size = struct.unpack_from('<QQQ', d, o + 0x10)
        if addr and typ != 8: out.append((addr, off, size))   # skip NOBITS
    return d, out

def rd(d, secs, va, fmt):
    for a, o, s in secs:
        if a <= va < a + s: return struct.unpack_from(fmt, d, o + va - a)[0]
    return None

def main():
    d, secs = sections(sys.argv[1])
    for fn in sys.argv[2:]:
        s = open(fn).read()
        def g(m):
            v = rd(d, secs, int(m.group(2), 16), '<d')
            return m.group(0) if v is None else '%s{=%.10g}' % (m.group(0), v)
        s = re.sub(r'\b(_UNK_|DAT_)([0-9a-f]{6,})\b', g, s)
        def alt(m):
            k = (int(m.group(1), 16) - 0x50) // 8
            return m.group(0) + ('/*%s*/' % chr(65 + k) if 0 <= k < 26 and (int(m.group(1), 16) - 0x50) % 8 == 0 else '')
        s = re.sub(r'\+ (0x[0-9a-f]+)\)', alt, s)
        open(fn[:-2] + '.a.c', 'w').write(s)

if __name__ == '__main__':
    main()
