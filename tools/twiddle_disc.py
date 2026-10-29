#!/usr/bin/env python3
"""Pre-twiddle the 8-bit paletted textures of the disc (FPTX fmt 1 -> fmt 2) so the Dreamcast uploads
them with a straight copy (pvr_txr_load) instead of twiddling every texel on the SH4 at load time
(pvr_txr_load_ex PVR_TXRLOAD_8BPP: measured 1.6 s of the DV1 load in Flycast).

The output is byte-for-byte what KOS 2.3 pvr_txr_load_ex(..., PVR_TXRLOAD_8BPP) writes to VRAM
(kernel/arch/dreamcast/hardware/pvr/pvr_texture.c, TWIDTAB/TWIDOUT, 16-bit writes holding the texels
of rows y and y+1). Plain Python (no NumPy, so the drag-and-drop builder needs one package less): one byte
permutation per texture size, applied with operator.itemgetter. `--selftest` checks it against a direct
transcription of the KOS loop. Only the disc copy is converted; converted2/ (host build) keeps fmt 1.
usage: twiddle_disc.py <disc_data_dir> | --selftest"""
import os, sys, struct, operator, random

TWID = [0] * 1024
for _v in range(1024):
    TWID[_v] = sum(((_v >> b) & 1) << (2 * b) for b in range(10))

_PERM = {}
def perm(w, h):
    """output byte k of the twiddled image = input byte perm[k] (input: rows of w bytes)"""
    if (w, h) not in _PERM:
        mn = min(w, h); mask = mn - 1; blk = mn * mn // 2
        p = [0] * (w * h)
        for y in range(0, h, 2):
            ty = TWID[(y & mask) // 2] << 1; by = (y // mn) * blk
            r0 = y * w; r1 = r0 + w
            for x in range(w):
                i = 2 * ((TWID[x & mask] | ty) + (x // mn) * blk + by)
                p[i] = r0 + x; p[i + 1] = r1 + x
        _PERM[(w, h)] = operator.itemgetter(*p)
    return _PERM[(w, h)]

def twiddle8(pix, w, h):
    """pix: w*h bytes (rows) -> the bytes of the VRAM image (w*h bytes)"""
    return bytes(perm(w, h)(pix))

def twiddle8_ref(pix, w, h):                       # direct transcription of the KOS loop
    T = lambda v: TWID[v]
    TWIDOUT = lambda a, b: T(b) | (T(a) << 1)
    mn = min(w, h); mask = mn - 1; out = [0] * (w * h // 2)
    for y in range(0, h, 2):
        for x in range(w):
            out[TWIDOUT((y & mask) // 2, x & mask) + (x // mn + y // mn) * mn * mn // 2] = pix[y * w + x] | (pix[(y + 1) * w + x] << 8)
    return struct.pack('<%dH' % len(out), *out)

def selftest():
    rng = random.Random(1)
    for w, h in ((8, 8), (16, 8), (8, 32), (64, 64), (128, 32), (256, 1024)):
        p = bytes(rng.randrange(256) for _ in range(w * h))
        assert twiddle8(p, w, h) == twiddle8_ref(p, w, h), (w, h)
    print('twiddle selftest ok')

def main(d):
    n = 0
    for root, _, files in os.walk(d):
        for f in sorted(files):
            if not f.endswith('.tex'): continue
            p = os.path.join(root, f); b = bytearray(open(p, 'rb').read())
            if len(b) < 16 or b[:4] != b'FPTX': continue
            w, h, fmt = struct.unpack_from('<HHH', b, 4)
            if fmt != 1: continue
            b[16:16 + w * h] = twiddle8(bytes(b[16:16 + w * h]), w, h)
            struct.pack_into('<H', b, 8, 2)
            open(p, 'wb').write(b); n += 1
    print('twiddle_disc: %d textures pre-twiddled' % n)

if __name__ == '__main__':
    selftest() if sys.argv[1] == '--selftest' else main(sys.argv[1])
