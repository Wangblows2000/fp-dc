#!/usr/bin/env python3
"""Validate an AICA ADPCM stream against its source with an AICA-accurate decoder (same model as
Flycast core/hw/aica/sgc_if.cpp DecodeADPCM: no leak; low nibble first). Note: wav2adpcm -f adds a
'high pass' leak that the AICA does not have, so its round trip under-reports quality (8 dB vs 30 dB).
usage: adpcm_check.py <mono.adp> <source_mono_s16.wav>"""
import sys, wave, numpy as np
qs = [0xe6, 0xe6, 0xe6, 0xe6, 0x133, 0x199, 0x200, 0x266]
def decode(d):
    out = np.zeros(len(d) * 2); prev = 0; q = 127; k = 0
    for byte in d:
        for nib in (byte & 15, byte >> 4):
            rv = min((q * (1 + 2 * (nib & 7))) >> 3, 0x7fff)
            prev = max(-32768, min(32767, (-rv if nib & 8 else rv) + prev))
            q = max(127, min(24576, (q * qs[nib & 7]) >> 8)); out[k] = prev; k += 1
    return out
b = decode(open(sys.argv[1], 'rb').read())
w = wave.open(sys.argv[2]); a = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(float)
n = min(len(a), len(b)); a, b = a[:n], b[:n]
print('SNR %.1f dB' % (10 * np.log10((a ** 2).sum() / ((a - b) ** 2).sum())))
