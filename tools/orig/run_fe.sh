#!/bin/bash
# Front-end trace of the running ORIGINAL: run_fe.sh <script> <out.jsonl> <nframes> <watch offsets> [goto frame index]
PID=$(pgrep -x Chowdren | head -1); [ -z "$PID" ] && { echo "game not running"; exit 1; }
cd /home/claude/fpdc
GOTO=()
[ -n "$5" ] && GOTO=(-ex 'break *0x9fcb7a' -ex continue -ex "call ((void(*)(void*,int))0x9fc8c0)((void*)0x285ec60, $5)" -ex delete)
timeout 3000 gdb -q -batch -p $PID "${GOTO[@]}" -ex "set \$script=\"$1\"" -ex "set \$out=\"$2\"" -ex "set \$nframes=$3" -ex "set \$watch=\"$4\"" \
  -ex 'python exec(open("tools/orig/trace_fe.py").read())' -ex continue -ex detach > /tmp/gdb_fe.log 2>&1
grep -E "Error|Traceback" /tmp/gdb_fe.log | head -5
wc -l "$2"
