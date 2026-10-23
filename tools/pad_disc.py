#!/usr/bin/env python3
"""Pad every file under the disc data dir to a multiple of 32 bytes (zero bytes at the end).

Why: KOS 2.3 fs_iso9660 reads a file's last (size % 32) bytes with a 32-byte DMA into a buffer
inside its malloc'd file descriptor; that buffer is not guaranteed to be 32-byte aligned and the
read could hang (seen in Flycast on sfx/s176.wav after switching plat_load_file to aligned fs_read).
With sizes that are multiples of 32 that path is never taken. All loaders accept trailing padding
(level_load allows up to 2047 extra bytes; WAV/FPS2/FPTX/.ui parse by their own counts).
usage: pad_disc.py <dir>"""
import os, sys
n = added = 0
for root, _, files in os.walk(sys.argv[1]):
    for f in files:
        p = os.path.join(root, f); s = os.path.getsize(p)
        if s % 32:
            with open(p, 'ab') as fh: fh.write(b'\0' * (32 - s % 32))
            n += 1; added += 32 - s % 32
print('pad_disc: %d files padded, %d bytes added' % (n, added))
