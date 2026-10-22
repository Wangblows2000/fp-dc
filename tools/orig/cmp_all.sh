#!/bin/bash
# Run every tests/orig/*.txt sequence in the fpdc host build and compare with the original's trace.
# usage: cmp_all.sh [sequence names...]   (frames 50..FR, default 200)
cd /home/claude/fpdc
FR=${FR:-200}
mkdir -p build/fpdc_traces
pass=0; fail=0
for s in ${@:-$(ls tests/orig/*.txt | xargs -n1 basename | sed 's/.txt$//')}; do
  [ -f build/orig_traces/$s.csv ] || { echo "SKIP $s (no original trace)"; continue; }
  FP_SPAWN_X=68.715 FP_CSV=build/fpdc_traces/$s.csv ./host/fpdc_host converted2 $((FR+5)) tests/orig/$s.txt > /dev/null 2>&1
  if python3 tools/orig/compare.py build/orig_traces/$s.csv build/fpdc_traces/$s.csv --from 50 --to $FR ${Q:+--quiet}; then pass=$((pass+1)); else fail=$((fail+1)); fi
done
echo "== $pass passed, $fail failed"
