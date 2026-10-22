#!/bin/bash
# Run every tests/orig/*.txt sequence in the fpdc host build and compare with the original's trace.
# usage: cmp_all.sh [sequence names...]   (frames 50..FR, default 200)
# Start state comes from the original trace's frame 0 (X, energy, petals) so that sequences recorded
# after the 2026-10-22 D-Bus fix (Lilac at X 0, not 68.715 after the intro walk) line up.
cd /home/claude/fpdc
FR=${FR:-200}
mkdir -p build/fpdc_traces
pass=0; fail=0
for s in ${@:-$(ls tests/orig/*.txt | xargs -n1 basename | sed 's/.txt$//')}; do
  [ -f build/orig_traces/$s.csv ] || { echo "SKIP $s (no original trace)"; continue; }
  read X EN HP < <(python3 -c "
import csv,sys
r=next(csv.DictReader(open('build/orig_traces/$s.csv')))
print(r['mv_X'], r.get('en_A') or 100, r.get('c_petals') or 14)")
  [ "$EN" = "0" ] && EN=100
  FP_SPAWN_X=$X FP_ENERGY=$EN FP_HP=$HP FP_CSV=build/fpdc_traces/$s.csv ./host/fpdc_host converted2 $((FR+5)) tests/orig/$s.txt > /dev/null 2>&1
  if python3 tools/orig/compare.py build/orig_traces/$s.csv build/fpdc_traces/$s.csv --from 50 --to $FR ${Q:+--quiet}; then pass=$((pass+1)); else fail=$((fail+1)); fi
done
echo "== $pass passed, $fail failed"
