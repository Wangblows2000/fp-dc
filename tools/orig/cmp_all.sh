#!/bin/bash
# Run every tests/orig/*.txt sequence in the fpdc host build and compare with the original's trace.
# usage: cmp_all.sh [sequence names...]   (frames 50..FR, default 200)
# Start state comes from the original trace's frame 0 (X, energy, petals) so that sequences recorded
# after the 2026-10-22 D-Bus fix (Lilac at X 0, not 68.715 after the intro walk) line up.
cd /home/claude/fpdc
FR=${FR:-200}
mkdir -p build/fpdc_traces
pass=0; fail=0
rm -f build/strict_now.txt
for s in ${@:-$(ls tests/orig/*.txt | xargs -n1 basename | sed 's/.txt$//')}; do
  [ -f build/orig_traces/$s.csv ] || { echo "SKIP $s (no original trace)"; continue; }
  # start state from the original trace's frame 0: X, energy, petals, and the enemies' animation phase
  # (ticks of Turretus idle at speed 20 = 5 * frame + counter / 20; 0 for traces recorded without it)
  read X EN HP PT < <(python3 -c "
import csv,sys
r=next(csv.DictReader(open('build/orig_traces/$s.csv')))
pt = 5 * int(r['turr_afr']) + int(r['turr_acc']) // 20 if r.get('turr_acc') not in (None, '', '0') or r.get('turr_afr') not in (None, '', '0') else 0
print(r['mv_X'], r.get('en_A') or 100, r.get('c_petals') or 14, pt)")
  [ "$EN" = "0" ] && EN=100
  FP_LEVEL=dv1 FP_SPAWN_X=$X FP_ENERGY=$EN FP_HP=$HP FP_OBJ_PRETICK=$PT FP_CSV=build/fpdc_traces/$s.csv ./host/fpdc_host converted2 $((FR+5)) tests/orig/$s.txt > /dev/null 2>&1
  SKIP=$(awk '$2=="TP" && $1>=40 {printf "%d-%d,", $1-3, $1+3}' tests/orig/$s.txt)
  if python3 tools/orig/compare.py build/orig_traces/$s.csv build/fpdc_traces/$s.csv --from 50 --to $FR ${SKIP:+--skip $SKIP} --strict-out build/strict_now.txt ${Q:+--quiet}; then pass=$((pass+1)); else fail=$((fail+1)); fi
done
echo "== $pass passed, $fail failed"
# strict ratchet: exact-frame counts must not fall below tests/orig/strict_baseline.txt
# (STRICT_UPDATE=1 rewrites the baseline after an intended improvement)
python3 - <<'PY'
import os
cur = {l.split()[0]: list(map(int, l.split()[1:])) for l in open('build/strict_now.txt')} if os.path.exists('build/strict_now.txt') else {}
bp = 'tests/orig/strict_baseline.txt'
base = {l.split()[0]: list(map(int, l.split()[1:])) for l in open(bp) if l.strip() and not l.startswith('#')} if os.path.exists(bp) else {}
tot = sum(v[0] for v in cur.values()); n = sum(v[1] for v in cur.values())
print('== strict: %d/%d exact frames (%.1f%%)' % (tot, n, 100. * tot / max(n, 1)))
worse = [(k, base[k][0], v[0]) for k, v in cur.items() if k in base and v[0] < base[k][0]]
for k, b, c in worse: print('   STRICT REGRESSION %s: %d -> %d exact frames' % (k, b, c))
if os.environ.get('STRICT_UPDATE'):
    base.update(cur)
    with open(bp, 'w') as f:
        f.write('# sequence exact_frames compared_frames exact_prefix (tools/orig/cmp_all.sh STRICT_UPDATE=1)\n')
        for k in sorted(base): f.write('%s %d %d %d\n' % (k, *base[k]))
PY
