#!/usr/bin/env python3
"""RV-file check of the save model (spec_records_saves) against the original's PC saves, read in place and never copied:
prints only pass/fail per invariant, no values. The port's save v3 mirrors these layouts (save.c).
usage: check_user_saves.py <dir with file<n>.sav and records.dat>   (e.g. /home/claude/fprun_user; read-only)"""
import struct, sys, os, glob

def load(path):   # Clickteam ArrayObject: "CNC ARRAY\\0", u16 2, u16 0, i32 X, Y, Z, flags, then X*Y*Z int32 (x fastest)
    d = open(path, 'rb').read()
    assert d[:10] == b'CNC ARRAY\0', path
    X, Y, Z, fl = struct.unpack_from('<4i', d, 14)
    v = struct.unpack_from('<%di' % (X * Y * Z), d, 30)
    return X, Y, lambda x, y: v[(y - 1) * X + (x - 1)]   # 1-based A[x][y]

fails = 0
def check(name, cond):
    global fails
    print(('PASS ' if cond else 'FAIL ') + name); fails += 0 if cond else 1

src = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/fprun_user'
files = sorted(glob.glob(os.path.join(src, 'file*.sav')))
STAGES = 15
for f in files:
    X, Y, A = load(f)
    n = os.path.basename(f)
    if X != 3 or Y != 99: check('%s_shape' % n, False); continue
    if A(1, 6) <= 0: continue                                      # unused slot
    t = sum(A(3, 6 + 5 * s) for s in range(STAGES)); k = sum(A(3, 7 + 5 * s) for s in range(STAGES)); d = sum(A(3, 8 + 5 * s) for s in range(STAGES))
    check('%s_totals_are_sums_of_stage_blocks' % n, (t, k, d) == (A(1, 6), A(1, 4), A(1, 7)))
    last = A(1, 5)
    check('%s_last_stage_has_a_block' % n, 0 <= last < STAGES and A(3, 6 + 5 * last) > 0)
    check('%s_unused_block_rows_zero' % n, all(A(3, 9 + 5 * s) == 0 and A(3, 10 + 5 * s) == 0 for s in range(STAGES)))
    check('%s_scratch_clear' % n, all(A(3, y) == 0 for y in range(1, 6)))
rp = os.path.join(src, 'records.dat')
if os.path.exists(rp):
    X, Y, R = load(rp)
    check('records_shape_20x100', (X, Y) == (20, 100))
    # the Time Attack table holds a story file's stage values for its character (crystals equal, RE 908 has no mode test)
    J = {0: 0, 2: 2, 3: 2, 4: 3}
    ok = True
    for f in files:
        _, _, A = load(f)
        if A(1, 6) <= 0: continue
        c = 3 + J.get(A(1, 8), 0)
        for s in range(STAGES):
            if A(3, 6 + 5 * s) > 0 and R(c, 2 * s + 2) < A(3, 7 + 5 * s): ok = False
    check('records_crystals_at_least_every_file_block', ok)
    check('records_progress_rows_in_range', all(0 <= R(1, 40 + j) < STAGES for j in range(5)))
print('%d failure(s)' % fails)
sys.exit(1 if fails else 0)
