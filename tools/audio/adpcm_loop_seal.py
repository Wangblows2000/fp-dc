#!/usr/bin/env python3
"""Seal a whole-file-looping AICA ADPCM stream so its loop seam is clean (D-033).

The AICA ADPCM decoder has no leak: its state (previous sample, step size) carries straight across the loop
seam into the first bytes of the file, which were encoded from the reset state (0, 127). A track whose decoder
ends at, say, prev = 846 / step = 846 then plays every following sample with that offset and a wrong step size
until the clamp at +-32767 wears it down: loud, distorted, one-sided audio that peaks and fades (seen with
m_invincibility.adp after its first loop at ~23.8 s, i.e. ~25 s into invincibility).

This tool appends a few samples per channel that drive each channel's decoder exactly back to (0, 127), then pads
the file to a multiple of 32 bytes (the KOS stereo split works in 32-byte blocks) with +15/-15 pairs, which keep
the state at (0, 127). The added tail is 2-4 ms of near-silence. The file is otherwise unchanged.

Format: headerless, KOS byte-interleaved stereo (wav2adpcm -n -i -t): each byte = low nibble one channel,
high nibble the other; snd_adpcm_split separates them.
usage: adpcm_loop_seal.py <in.adp> [out.adp]    (default: in place)    --check: report the seam state only"""
import sys

QS = [0xe6, 0xe6, 0xe6, 0xe6, 0x133, 0x199, 0x200, 0x266]


def step(prev, q, nib):
    rv = min((q * (1 + 2 * (nib & 7))) >> 3, 0x7fff)
    prev = max(-32768, min(32767, (-rv if nib & 8 else rv) + prev))
    return prev, max(127, min(24576, (q * QS[nib & 7]) >> 8))


def end_state(nibs):
    prev, q = 0, 127
    for n in nibs:
        prev, q = step(prev, q, n)
    return prev, q


def tail(prev, q, parity, beam=4000, max_len=400):
    """Shortest nibble sequence (length % 2 == parity) taking (prev, q) to exactly (0, 127): beam search."""
    if (prev, q) == (0, 127) and parity == 0:
        return []
    front = {(prev, q): []}
    for depth in range(1, max_len + 1):
        nxt = {}
        for (p, s), seq in front.items():
            for n in range(16):
                st = step(p, s, n)
                if st not in nxt:
                    nxt[st] = seq + [n]
        if (0, 127) in nxt and depth % 2 == parity:
            return nxt[(0, 127)]
        front = dict(sorted(nxt.items(), key=lambda kv: abs(kv[0][0]) + 4 * (kv[0][1] - 127))[:beam])
    raise SystemExit('no seal found')


def main():
    args = [a for a in sys.argv[1:] if a != '--check']
    src = args[0]; dst = args[1] if len(args) > 1 else src
    d = bytearray(open(src, 'rb').read())
    lo = [b & 15 for b in d]; hi = [b >> 4 for b in d]
    sl, sh = end_state(lo), end_state(hi)
    print('%s: %d bytes, seam state low %s high %s' % (src, len(d), sl, sh))
    if '--check' in sys.argv:
        return
    par = (32 - len(d) % 32) % 2   # total tail length must have this parity to reach a multiple of 32
    tl, th = tail(*sl, par), tail(*sh, par)
    n = max(len(tl), len(th))
    n += (-(len(d) + n)) % 32
    tl += [0, 8] * ((n - len(tl)) // 2); th += [0, 8] * ((n - len(th)) // 2)
    assert len(tl) == len(th) == n and (len(d) + n) % 32 == 0
    d += bytes(l | (h << 4) for l, h in zip(tl, th))
    lo = [b & 15 for b in d]; hi = [b >> 4 for b in d]
    assert end_state(lo) == (0, 127) and end_state(hi) == (0, 127)
    open(dst, 'wb').write(d)
    print('  sealed: +%d bytes -> %d (seam state (0, 127) on both channels)' % (n, len(d)))


main()
