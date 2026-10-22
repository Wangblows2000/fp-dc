#!/bin/bash
# Trace every tests/orig/*.txt sequence on the running original (each restarts DV1).
cd /home/claude/fpdc
mkdir -p build/orig_traces
for s in ${@:-tests/orig/*.txt}; do
  n=$(basename $s .txt)
  tools/orig/run_trace.sh $s build/orig_traces/$n.csv ${NF:-260} 1 | tail -1
done
