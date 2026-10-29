#!/usr/bin/env python3
"""
Duration and sample rate of an Ogg Vorbis file without ffprobe (the converters run on plain Windows too).
Prints the same text as `ffprobe -v error -show_entries format=duration -of csv=p=0 <file>` (checked on every .ogg the
pipeline reads: tests in tools/builder/selftest.py).

usage: oggdur.py <file.ogg>        -> duration in seconds, 6 decimals
"""
import struct, sys

def _pages(d):
    o = 0
    while True:
        o = d.find(b'OggS', o)
        if o < 0 or o + 27 > len(d): return
        nseg = d[o + 26]; segs = d[o + 27:o + 27 + nseg]
        yield o, struct.unpack_from('<q', d, o + 6)[0], o + 27 + nseg, sum(segs)
        o += 27 + nseg + sum(segs)

def _wav(d, path):
    o = 12; rate = byterate = None; size = None
    while o + 8 <= len(d):
        cid, n = d[o:o + 4], struct.unpack_from('<I', d, o + 4)[0]
        if cid == b'fmt ': rate, byterate = struct.unpack_from('<II', d, o + 12)
        elif cid == b'data': size = min(n, len(d) - o - 8); break
        o += 8 + n + (n & 1)
    if not byterate or size is None: raise ValueError('%s: bad WAV' % path)
    return (size * 1000000 + byterate // 2) // byterate, rate

def info(path):
    """(duration_s, sample_rate): duration = (last granule - start) / rate, start = the first audio page's granule minus
    the samples it holds (0 for every normal file), as libavformat's ogg demuxer computes it"""
    d = open(path, 'rb').read()
    if d[:4] == b'RIFF' and d[8:12] == b'WAVE':       # a few of Assets.dat's sounds are WAV files named .ogg
        return _wav(d, path)
    pages = list(_pages(d))
    if not pages: raise ValueError('%s: not an Ogg file' % path)
    o, g, body, n = pages[0]
    if d[body:body + 7] != b'\x01vorbis': raise ValueError('%s: not Ogg Vorbis' % path)
    rate = struct.unpack_from('<I', d, body + 12)[0]
    last = max((g for o, g, b, n in pages if g >= 0), default=0)
    dur_us = (last * 1000000 + rate // 2) // rate        # av_rescale rounding (nearest)
    return dur_us, rate

def duration_text(path):
    us, rate = info(path)
    return '%d.%06d' % (us // 1000000, us % 1000000)

if __name__ == '__main__':
    for p in sys.argv[1:]: print(duration_text(p))
