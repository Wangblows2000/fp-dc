#!/usr/bin/env python3
"""Pack each data directory (entry: name[36], crc32 of the raw content, offset, stored, raw) into <dir>.pak (format in src/resources/pack.c) and remove the loose files.

- Entries are LZ4 blocks (lz4.block, high compression) when that saves at least 10 %, else stored.
- Every entry starts at a 32-byte aligned offset.
- Kept loose (read outside loads or by name from the disc): ui/loading.*, ui/vmu_*.
- bgm/ is never packed (the music is streamed).
Requires: pip install lz4.
usage: pack_disc.py <data_dir> [--keep]   (--keep: leave the loose files in place)"""
import os, sys, struct, fnmatch
import zlib
import lz4.block

LOOSE = ['loading.*', 'vmu_*']
SKIP_DIRS = {'bgm'}

def pack_dir(root, d, keep):
    src = os.path.join(root, d)
    names = sorted(f for f in os.listdir(src) if os.path.isfile(os.path.join(src, f))
                   and not any(fnmatch.fnmatch(f, p) for p in LOOSE))
    if not names: return 0, 0, 0
    ents, blobs = [], []
    off = 8 + 52 * len(names)
    off = (off + 31) & ~31
    raw_tot = st_tot = 0
    for n in names:
        if len(n.encode()) >= 36: raise SystemExit('name too long for a pack entry: %s/%s' % (d, n))
        raw = open(os.path.join(src, n), 'rb').read()
        c = lz4.block.compress(raw, mode='high_compression', compression=12, store_size=False)
        data = c if len(c) < 0.9 * len(raw) else raw
        ents.append((n, zlib.crc32(raw) & 0xffffffff, off, len(data), len(raw))); blobs.append((off, data))
        off = (off + len(data) + 31) & ~31
        raw_tot += len(raw); st_tot += len(data)
    out = bytearray(off)
    struct.pack_into('<4sI', out, 0, b'FPK1', len(ents))
    for i, (n, crc, o, st, rw) in enumerate(ents):
        struct.pack_into('<36sIIII', out, 8 + 52 * i, n.encode(), crc, o, st, rw)
    for o, data in blobs: out[o:o + len(data)] = data
    open(os.path.join(root, d + '.pak'), 'wb').write(out)
    if not keep:
        for n in names: os.remove(os.path.join(src, n))
        if not os.listdir(src): os.rmdir(src)
    return len(names), raw_tot, len(out)

def main():
    root = sys.argv[1]; keep = '--keep' in sys.argv
    for d in sorted(os.listdir(root)):
        if d in SKIP_DIRS or not os.path.isdir(os.path.join(root, d)): continue
        n, raw, packed = pack_dir(root, d, keep)
        if n: print('pack_disc: %-5s %3d files %8d KB -> %6d KB' % (d, n, raw // 1024, packed // 1024))

if __name__ == '__main__':
    main()
