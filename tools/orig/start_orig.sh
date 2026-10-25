#!/bin/bash
# Start the ORIGINAL Linux Freedom Planet headless, fully visible.
# usage: start_orig.sh [gdb-script]     env: RUN=/home/claude/fprun DISP=:93
# RUN must be a writable copy of the game (never the user's folder).
# Display fix (2026-10-22): the game renders a 2x (852x480) image but its window is created
# 426x240 when no window manager is running, so only the bottom-left quarter was visible.
# We force the window to 852x480 at (0,0) on an 852x480 Xvfb screen.
RUN=${RUN:-/home/claude/fprun}; DISP=${DISP:-:93}
export DISPLAY=$DISP
if ! xdpyinfo -display $DISP >/dev/null 2>&1; then
  (setsid Xvfb $DISP -screen 0 852x480x24 </dev/null >/dev/null 2>&1 8>&- 9>&- &)   # no inherited lock fds (flock 9)
  for i in $(seq 50); do xdpyinfo -display $DISP >/dev/null 2>&1 && break; timeout 0.2 tail -f /dev/null; done
fi
kill -9 $(pgrep -x Chowdren) $(pgrep -x gdb) 2>/dev/null
cd $RUN
# no D-Bus in the VM: without this SDL forks dbus-launch every frame (51k forks, traces crawl)
export DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
export SDL_AUDIODRIVER=dummy ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 LD_LIBRARY_PATH=$RUN/bin64
if [ -n "$1" ]; then
  (setsid gdb -q -batch -x "$1" ./bin64/Chowdren > /tmp/fp_run.log 2>&1 &)
else
  (setsid ./bin64/Chowdren > /tmp/fp_run.log 2>&1 &)
fi
for i in $(seq 150); do grep -aq "Frame set" /tmp/fp_run.log && break; timeout 0.2 tail -f /dev/null; done
for i in $(seq 50); do W=$(xdotool search --name 'Freedom Planet' 2>/dev/null | head -1); [ -n "$W" ] && break; timeout 0.2 tail -f /dev/null; done
# resize once, after the game has finished its own window setup (resizing earlier leaves a stale viewport)
timeout 5 tail -f /dev/null
xdotool windowsize $W 852 480 windowmove $W 0 0
grep -a "Setting frame" /tmp/fp_run.log | tail -1
