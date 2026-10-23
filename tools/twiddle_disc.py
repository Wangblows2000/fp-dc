#!/usr/bin/env python3
"""Pre-twiddle the 8-bit paletted textures of the disc (FPTX fmt 1 -> fmt 2) so the Dreamcast uploads
them with a straight copy (pvr_txr_load) instead of twiddling every texel on the SH4 at load time
(pvr_txr_load_ex PVR_TXRLOAD_8BPP: measured 1.6 s of the DV1 load in Flycast).

The output is byte-for-byte what KOS 2.3 pvr_txr_load_ex(..., PVR_TXRLOAD_8BPP) writes to VRAM
(kernel/arch/dreamcast/hardware/pvr/pvr_texture.c, TWIDTAB/TWIDOUT, 16-bit writes holding the texels
of rows y and y+1). `--selftest` checks the vectorised version against a direct transcription of the
KOS loop. Only the disc copy is converted; converted2/ (host build) keeps fmt 1.
usage: twiddle_disc.py <disc_data_dir> | --selftest"""
import os, sys, struct
import numpy as np

def twidtab(x):
    x = np.asarray(x, dtype=np.int64); r = np.zeros_like(x)
    for b in range(10): r |= (x & (1 << b)) << b
    return r

def twiddle8(pix, w, h):
    """pix: uint8 (h, w) -> uint8 bytes of the VRAM image (w*h bytes)."""
    mn = min(w, h); mask = mn - 1
    y = np.arange(0, h, 2)[:, None]; x = np.arange(w)[None, :]
    idx = (twidtab((x & mask)) | (twidtab(((y & mask) // 2)) << 1)) + (x // mn + y // mn) * mn * mn // 2
    val = pix[0::2, :].astype(np.uint16) | (pix[1::2, :].astype(np.uint16) << 8)
    out = np.zeros(w * h // 2, dtype=np.uint16)
    out[idx.ravel()] = val.ravel()
    return out.astype('<u2').tobytes()

def twiddle8_ref(pix, w, h):                       # direct transcription of the KOS loop
    T = lambda v: int(twidtab(v))
    TWIDOUT = lambda a, b: T(b) | (T(a) << 1)
    mn = min(w, h); mask = mn - 1; out = [0] * (w * h // 2)
    for y in range(0, h, 2):
        for x in range(w):
            out[TWIDOUT((y & mask) // 2, x & mask) + (x // mn + y // mn) * mn * mn // 2] = int(pix[y, x]) | (int(pix[y + 1, x]) << 8)
    return np.array(out, dtype='<u2').tobytes()

def selftest():
    rng = np.random.default_rng(1)
    for w, h in ((8, 8), (16, 8), (8, 32), (64, 64), (128, 32)):
        p = rng.integers(0, 256, (h, w), dtype=np.uint8)
        assert twiddle8(p, w, h) == twiddle8_ref(p, w, h), (w, h)
    print('twiddle selftest ok')

def main(d):
    n = 0
    for root, _, files in os.walk(d):
        for f in files:
            if not f.endswith('.tex'): continue
            p = os.path.join(root, f); b = bytearray(open(p, 'rb').read())
            if len(b) < 16 or b[:4] != b'FPTX': continue
            w, h, fmt = struct.unpack_from('<HHH', b, 4)
            if fmt != 1: continue
            pix = np.frombuffer(bytes(b[16:16 + w * h]), dtype=np.uint8).reshape(h, w)
            b[16:16 + w * h] = twiddle8(pix, w, h)
            struct.pack_into('<H', b, 8, 2)
            open(p, 'wb').write(b); n += 1
    print('twiddle_disc: %d textures pre-twiddled' % n)

if __name__ == '__main__':
    selftest() if sys.argv[1] == '--selftest' else main(sys.argv[1])
