#!/bin/bash
# Drive the original game and screenshot the WHOLE window (852x480 = 2x of 426x240).
# usage: key.sh <shot-name|-> key...   keys: xdotool names, wait:S, hold:KEY:S, down:KEY, up:KEY
export DISPLAY=${DISP:-:93}
W=$(xdotool search --name 'Freedom Planet' 2>/dev/null | head -1); [ -n "$W" ] && xdotool windowfocus --sync $W 2>/dev/null
name=$1; shift
for k in "$@"; do
  case $k in
    wait:*) timeout ${k#wait:} tail -f /dev/null;;
    hold:*) kk=$(echo $k | cut -d: -f2); s=$(echo $k | cut -d: -f3); xdotool keydown $kk; timeout $s tail -f /dev/null; xdotool keyup $kk;;
    down:*) xdotool keydown ${k#down:};;
    up:*) xdotool keyup ${k#up:};;
    *) xdotool keydown $k; timeout 0.12 tail -f /dev/null; xdotool keyup $k; timeout 0.5 tail -f /dev/null;;
  esac
done
if [ "$name" != "-" ]; then
  mkdir -p /home/claude/fpdc/build/orig
  import -window root /home/claude/fpdc/build/orig/$name.png
fi
grep -a 'Setting frame' /tmp/fp_run.log | tail -1
