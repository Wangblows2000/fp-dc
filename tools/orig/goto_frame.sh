#!/bin/bash
# Jump the running ORIGINAL game to frame index N (as logged by "Setting frame: N") via its own
# GameManager::set_frame, then screenshot after S seconds. usage: goto_frame.sh N [S=2.5] [name]
PID=$(pgrep -x Chowdren | head -1); [ -z "$PID" ] && { echo "game not running"; exit 1; }
N=$1; S=${2:-2.5}; NAME=${3:-frame_$1}
timeout 60 gdb -q -batch -p $PID -ex 'break *0x9fcb7a' -ex continue \
  -ex "call ((void(*)(void*,int))0x9fc8c0)((void*)0x285ec60, $N)" -ex delete -ex detach >/dev/null 2>&1
timeout $S tail -f /dev/null
mkdir -p /home/claude/fpdc/build/orig
DISPLAY=${DISP:-:93} import -window root /home/claude/fpdc/build/orig/$NAME.png
grep -a "Setting frame" /tmp/fp_run.log | tail -1
