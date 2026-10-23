#!/bin/bash
# Boot build/fpdc.cdi in Flycast headless and drive it with keys.
# usage: flycast_run.sh "<step> <step> ..."  step = wait:S | key:NAME | shot:NAME
# Flycast keyboard: x=A, c=B, s=X, Return=Start, arrows = d-pad. Shots go to build/fc/<name>.png
DISP=:95; export DISPLAY=$DISP
mkdir -p build/fc
# fresh display every run: a reused Xvfb sometimes stops delivering xdotool keys to Flycast
kill $(pgrep -x flycast) 2>/dev/null
for p in $(pgrep -x Xvfb); do grep -q "$DISP" /proc/$p/cmdline && kill $p; done
timeout 1 tail -f /dev/null
(setsid Xvfb $DISP -screen 0 1280x960x24 >/dev/null 2>&1 &); timeout 2 tail -f /dev/null
(LIBGL_ALWAYS_SOFTWARE=1 setsid third_party/flycast/build/flycast ${CDI:-build/fpdc.cdi} > /tmp/fc.log 2>&1 &)
for i in $(seq 60); do W=$(xdotool search --class flycast 2>/dev/null | tail -1); [ -n "$W" ] && break; timeout 0.5 tail -f /dev/null; done
timeout 3 tail -f /dev/null
xdotool windowfocus --sync $W 2>/dev/null; [ "${CLICK:-1}" = 1 ] && xdotool mousemove --window $W 50 50 click 1
for st in $1; do
  case $st in
    wait:*) timeout ${st#wait:} tail -f /dev/null;;
    key:*) xdotool keydown ${st#key:}; timeout 0.15 tail -f /dev/null; xdotool keyup ${st#key:};;
    hold:*) k=${st#hold:}; xdotool keydown ${k%@*}; timeout ${k#*@} tail -f /dev/null; xdotool keyup ${k%@*};;
    shot:*) import -window root build/fc/${st#shot:}.png;;
  esac
done
kill $(pgrep -x flycast) 2>/dev/null
grep -a -E "perf|missing|FATAL|failed" /tmp/fc.log | tail -5
