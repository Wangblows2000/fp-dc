#!/bin/bash
# usage: run_trace.sh <input_script> <out.csv> <nframes> [restart(1)]
# Attaches to the running original game (must already be in gameplay with menu state set, e.g. via
# the menu route in RUNTIME_MAP.md), optionally restarts frame FRAME (default 20 = Dragon Valley 1) via the
# game's own GameManager::set_frame, then traces with tools/orig/trace_orig.py.
PID=$(pgrep -x Chowdren | head -1); [ -z "$PID" ] && { echo "game not running"; exit 1; }
R=${4:-1}
cd /home/claude/fpdc
RESTART_CMDS=()
if [ "$R" = 1 ]; then
  RESTART_CMDS=(-ex 'break *0x9fcb7a' -ex continue -ex 'call ((void(*)(void*,int))0x9fc8c0)((void*)0x285ec60, '${FRAME:-20}')' -ex 'delete')
fi
timeout 3000 gdb -q -batch -p $PID "${RESTART_CMDS[@]}" \
  -ex "set \$script=\"$1\"" -ex "set \$out=\"$2\"" -ex "set \$nframes=$3" -ex "set \$restart=$R" \
  -ex 'python exec(open("tools/orig/trace_orig.py").read())' -ex continue -ex detach 2>&1 \
  > /tmp/gdb_trace.log 2>&1; grep -E "objects|match|Error|error|Traceback" /tmp/gdb_trace.log | head
wc -l "$2"
